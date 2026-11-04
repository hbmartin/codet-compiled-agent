"""Run the complete enabled GoogleTest inventory in fresh, individually audited processes."""
import argparse
import json
import os
import re
import tempfile
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
    if inputs is not None and (not isinstance(value.get('binary'), str) or
            not re.fullmatch(r'[0-9a-f]{64}', str(value.get('binary_sha256', ''))) or
            inputs.get(value['binary']) != value['binary_sha256']):
        raise ValueError('Isolated GoogleTest executable binding mismatch')
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
        if (int(root.get('failures', '0')) != len(failed_nodes) or
                int(root.get('errors', '0')) != len(errors)):
            raise ValueError('Isolated GoogleTest XML outcome count mismatch')
        failed = bool(failed_nodes or errors)
        if record['exit'] != int(failed):
            raise ValueError('Isolated GoogleTest exit/result mismatch')
        failures += failed
        if cases[0].find('skipped') is not None:
            skipped.append(record['test'])
    if value['skipped'] != skipped or value['failures'] != failures:
        raise ValueError('Isolated GoogleTest outcome accounting mismatch')
    return dict(tests=len(expected), disabled=len(disabled), skipped=len(skipped), failures=failures)


def run(binary):
    before = digest(binary)
    results, skipped = [], []
    failures = 0
    with tempfile.TemporaryDirectory(prefix='codet-isolated-gtest-') as temporary:
        report = Path(temporary)/'result.xml'
        command = [str(binary), '--gtest_list_tests', '--gtest_output=xml:'+str(report)]
        listed = supervised_run(command, capture_output=True, text=True, timeout=30, check=True)
        raw = report.read_text()
        names, disabled = inventory(raw)
        enabled = [name for name in names if name not in disabled]
        for number, name in enumerate(enabled):
            report.unlink(missing_ok=True)
            command = [str(binary), '--gtest_filter='+name, '--gtest_output=xml:'+str(report)]
            record = dict(test=name, argv=command)
            try:
                completed = supervised_run(command, capture_output=True, text=True, timeout=30)
                record.update(exit=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
                record['xml'] = report.read_text()
                root = parsed_xml(record['xml'])
                if list(root.iter('skipped')):
                    skipped.append(name)
                if completed.returncode:
                    failures += 1
                    print(completed.stdout+completed.stderr, flush=True)
            except Exception as error:
                failures += 1
                record.update(exit=getattr(error, 'returncode', None), error=str(error),
                              cleanup=getattr(error, 'process_cleanup', {}))
                print('Isolated test failed: '+name+': '+str(error), flush=True)
            results.append(record)
            if (number+1) % 50 == 0:
                print('Isolated GoogleTest coverage:', number+1, '/', len(enabled), flush=True)
    if digest(binary) != before:
        raise ValueError('Isolated GoogleTest executable changed')
    value = dict(binary=str(binary), binary_sha256=before, inventory_argv=listed.args,
                 inventory_xml=raw, enabled=len(enabled), disabled=disabled,
                 skipped=skipped, failures=failures, results=results)
    verify_payload(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--inputs', type=json.loads, default=[])
    parser.add_argument('binary', type=Path)
    args = parser.parse_args()
    if args.report.exists():
        raise ValueError('Isolated report already exists; select a fresh output')
    value = run(args.binary.resolve())
    if 'CODET_EVIDENCE_CONTEXT' in os.environ:
        from validation_evidence import write_artifact
        candidates = [name for name in args.inputs if (Path.cwd() / name).resolve() == args.binary.resolve()]
        if len(candidates) != 1:
            raise ValueError('Isolated executable must be declared exactly once')
        value['binary'] = candidates[0]
        write_artifact(args.report, 'gtest-isolation', value, args.inputs)
    else:
        args.report.write_text(json.dumps(dict(binding='standalone-unbound', payload=value), indent=2)+'\n')
    print('Isolated GoogleTest result:', verify_payload(value), flush=True)
    return int(value['failures'] != 0)


if __name__ == '__main__':
    raise SystemExit(main())
