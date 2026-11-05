"""Run the complete enabled GoogleTest inventory in fresh, individually audited processes."""
import argparse
import json
import os
import re
import tempfile
import time
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

from process_supervision import run as supervised_run
from verified_sources import file_hash as digest


def parsed_xml(xml):
    try:
        return ET.fromstring(xml)
    except (ET.ParseError, TypeError) as error:
        raise ValueError('Malformed GoogleTest XML') from error


def inventory(xml):
    root = parsed_xml(xml)
    names = [suite.get('name', '') + '.' + case.get('name', '')
             for suite in root.findall('testsuite') for case in suite.findall('testcase')]
    if not names or len(set(names)) != len(names) or int(root.get('tests', '-1')) != len(names):
        raise ValueError('Invalid or duplicate GoogleTest inventory')
    if any(not re.fullmatch(r'[A-Za-z0-9_/]+\.[A-Za-z0-9_/]+', name) for name in names):
        raise ValueError('Unsafe GoogleTest filter in inventory')
    disabled = [name for name in names if re.search(r'(^|[./])DISABLED_', name)]
    return names, disabled


def verify_payload(value, inputs=None):
    if type(value.get("payload_version", 1)) is not int or value.get("payload_version", 1) not in (1, 2):
        raise ValueError("Unsupported isolated GoogleTest payload version")
    if value.get("payload_version") == 2 and value.get("failures") == 0 and value.get("complete") is not True:
        raise ValueError("Incomplete isolated GoogleTest run cannot report success")
    if inputs is not None and (not isinstance(value.get('binary'), str) or
            not re.fullmatch(r'[0-9a-f]{64}', str(value.get('binary_sha256', ''))) or
            inputs.get(value.get('binary_input', value['binary'])) != value['binary_sha256']):
        raise ValueError('Isolated GoogleTest executable binding mismatch')
    command = value.get('inventory_argv', [])
    if (len(command) != 3 or command[0] != value.get('binary') or
            command[1] != '--gtest_list_tests' or not command[2].startswith('--gtest_output=xml:')):
        raise ValueError('Isolated inventory command mismatch')
    names, disabled = inventory(value['inventory_xml'])
    expected = [name for name in names if name not in disabled]
    actual = [record['test'] for record in value['results']]
    if Counter(actual) != Counter(expected):
        raise ValueError('Isolated GoogleTest coverage mismatch')
    if value['disabled'] != disabled or value['enabled'] != len(expected):
        raise ValueError('Isolated GoogleTest inventory accounting mismatch')
    skipped = []
    failures = 0
    for record in value['results']:
        command = record.get('argv', [])
        if (len(command) != 3 or command[0] != value['binary'] or
                command[1] != '--gtest_filter='+record['test'] or
                not command[2].startswith('--gtest_output=xml:')):
            raise ValueError('Isolated test command mismatch')
        if record.get('error'):
            failures += 1
            continue
        root = parsed_xml(record['xml'])
        cases = list(root.iter('testcase'))
        if (int(root.get('tests', '-1')) != 1 or len(cases) != 1 or
                cases[0].attrib['classname'] + '.' + cases[0].attrib['name'] != record['test'] or
                cases[0].get('status') != 'run'):
            raise ValueError('Isolated GoogleTest did not run exactly the requested test')
        failed_nodes, errors = cases[0].findall('failure'), cases[0].findall('error')
        if (int(root.get('failures', '0')) != int(bool(failed_nodes)) or
                int(root.get('errors', '0')) != int(bool(errors))):
            raise ValueError('Isolated GoogleTest XML outcome count mismatch')
        failed = bool(failed_nodes or errors)
        if value.get('payload_version', 1) == 1 and record['exit'] != int(failed):
            raise ValueError('Isolated GoogleTest exit/result mismatch')
        if type(record.get('exit')) is not int or (failed and record['exit'] == 0):
            raise ValueError('Isolated GoogleTest exit/result mismatch')
        failures += bool(failed or record['exit'])
        if cases[0].find('skipped') is not None:
            skipped.append(record['test'])
    if value['skipped'] != skipped or value['failures'] != failures:
        raise ValueError('Isolated GoogleTest outcome accounting mismatch')
    return dict(tests=len(expected), disabled=len(disabled), skipped=len(skipped), failures=failures)


def run(binary, checkpoint=None, suite_timeout=850):
    if any(os.environ.get(name) for name in ('GTEST_FILTER', 'GTEST_TOTAL_SHARDS', 'GTEST_SHARD_INDEX',
                                           'GTEST_REPEAT', 'GTEST_ALSO_RUN_DISABLED_TESTS')):
        raise ValueError('Isolation requires an unfiltered, unsharded GoogleTest environment')
    before = digest(binary)
    results, skipped = [], []
    failures = 0
    deadline = time.monotonic() + suite_timeout
    with tempfile.TemporaryDirectory(prefix='codet-isolated-gtest-') as temporary:
        report = Path(temporary)/'result.xml'
        command = [str(binary), '--gtest_list_tests', '--gtest_output=xml:'+str(report)]
        listed = supervised_run(command, capture_output=True, text=True, timeout=30, check=True)
        raw = report.read_text()
        names, disabled = inventory(raw)
        enabled = [name for name in names if name not in disabled]
        value = dict(payload_version=2, binary=str(binary), binary_sha256=before,
                     inventory_argv=listed.args, inventory_xml=raw, enabled=len(enabled),
                     disabled=disabled, skipped=skipped, failures=0, results=results, complete=False)
        for number, name in enumerate(enabled):
            report.unlink(missing_ok=True)
            command = [str(binary), '--gtest_filter='+name, '--gtest_output=xml:'+str(report)]
            record = dict(test=name, argv=command, exit=None)
            try:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError('Isolated suite deadline exhausted before launch')
                completed = supervised_run(command, capture_output=True, text=True, timeout=min(30, remaining))
                record.update(exit=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
                record['xml'] = report.read_text()
                root = parsed_xml(record['xml'])
                if list(root.iter('skipped')):
                    skipped.append(name)
                failed = bool(list(root.iter('failure')) or list(root.iter('error')))
                if failed and not completed.returncode:
                    raise ValueError('Failed assertions with successful process exit')
                if failed or completed.returncode:
                    failures += 1
                    print(completed.stdout+completed.stderr, flush=True)
            except Exception as error:
                failures += 1
                # XML/file/parser failures must not erase a return code already observed.
                if record['exit'] is None:
                    record['exit'] = getattr(error, 'returncode', None)
                record.update(error=str(error), cleanup=getattr(error, 'process_cleanup', {}))
                print('Isolated test failed: '+name+': '+str(error), flush=True)
            results.append(record)
            value['failures'] = failures
            if checkpoint:
                checkpoint(value)
            if (number+1) % 50 == 0:
                print('Isolated GoogleTest coverage:', number+1, '/', len(enabled), flush=True)
    if digest(binary) != before:
        raise ValueError('Isolated GoogleTest executable changed')
    value['complete'] = not any('error' in record for record in results)
    verify_payload(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    outputs = parser.add_mutually_exclusive_group(required=True)
    outputs.add_argument('--report', type=Path)
    outputs.add_argument('--report-root', type=Path)
    parser.add_argument('--inputs', type=json.loads, default=[])
    parser.add_argument('binary', type=Path)
    args = parser.parse_args()
    if args.report_root:
        args.report_root.mkdir(parents=True, exist_ok=True)
        args.report = Path(tempfile.mkdtemp(prefix='run-', dir=args.report_root))/'report.json'
        if 'CODET_EVIDENCE_CONTEXT' in os.environ:
            context = json.loads(os.environ['CODET_EVIDENCE_CONTEXT'])
            selected = [item for item in context['declared_artifacts'] if item['kind'] == 'gtest-isolation']
            if len(selected) != 1:
                raise ValueError('Isolation capture must declare exactly one report')
            args.report = Path(context['root'])/selected[0]['path']
    if args.report.exists():
        raise ValueError('Isolated report already exists; select a fresh output')
    from validation_evidence import atomic_json
    progress = args.report.with_suffix('.progress.json')
    value = run(args.binary.resolve(), lambda current:atomic_json(progress, current))
    if 'CODET_EVIDENCE_CONTEXT' in os.environ:
        from validation_evidence import write_artifact
        root = Path(json.loads(os.environ['CODET_EVIDENCE_CONTEXT'])['root'])
        candidates = [name for name in args.inputs if (root / name).resolve() == args.binary.resolve()]
        if len(candidates) != 1:
            raise ValueError('Isolated executable must be declared exactly once')
        value['binary_input'] = candidates[0]
        write_artifact(args.report, 'gtest-isolation', value, args.inputs)
    else:
        atomic_json(args.report, dict(binding='standalone-unbound', payload=value))
    progress.unlink(missing_ok=True)
    print('Isolated GoogleTest result:', verify_payload(value), flush=True)
    return int(value['failures'] != 0)


if __name__ == '__main__':
    raise SystemExit(main())
