"""Audit an explicitly incomplete campaign; exit 0 verifies integrity, never acceptance."""
import ast
import json
import re
import sys
from pathlib import Path


def digest(path):
    from verified_sources import file_hash
    return file_hash(path)


def verify(folder):
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(Path(__file__).resolve().parent / 'verifier_tooling'))
    from evidence_bundle import safe_member, valid_digest
    from evidence_inputs import environment, file_specs, role_selected
    from evidence_paths import verify_working_provenance
    from validation_evidence import load_manifest

    folder = folder.resolve()
    inventory = json.loads(safe_member(folder, 'inventory.json').read_text())
    if not isinstance(inventory, dict):
        raise ValueError('Invalid inventory')
    for name, value in inventory.items():
        valid_digest(value)
        safe_member(folder, name)
    if any(p.is_symlink() for p in folder.rglob('*')):
        raise ValueError('Symlink in attempt bundle')
    actual = {str(p.relative_to(folder)): digest(p) for p in folder.rglob('*') if p.is_file() and p != folder / 'inventory.json'}
    if actual != inventory:
        raise ValueError('Attempt inventory mismatch: missing=' + repr(sorted(set(inventory) - set(actual)))
                         + ', extra=' + repr(sorted(set(actual) - set(inventory)))
                         + ', mismatched=' + repr(sorted(name for name in set(actual) & set(inventory)
                                                          if actual[name] != inventory[name])))
    manifest = load_manifest(folder / 'manifest.json')
    run = json.loads((folder / 'capture/run.json').read_text())
    report = json.loads((folder / 'report.json').read_text())
    try:
        measurements = json.loads(safe_member(folder, 'measurements.json').read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError('Missing or malformed attempt measurements.json') from error
    if (not isinstance(measurements, list) or measurements
            or type(report.get('measurement_samples')) is not int
            or report['measurement_samples'] != 0):
        raise ValueError('Incomplete attempt must declare an empty measurement list and zero samples')
    if report.get('attempt_bundle_version') != 2 or report.get('eligible_current') is not False or report.get('complete') is not False:
        raise ValueError('Incomplete attempt must not claim acceptance')
    if run.get('schema_version') != 5 or report.get('schema_version') != 5:
        raise ValueError('Unexpected capture format')
    if run['manifest_sha256'] != digest(folder / 'manifest.json'):
        raise ValueError('Manifest binding mismatch')
    if report['campaign_id'] != run['campaign_id'] or report['capture_run_id'] != run['id']:
        raise ValueError('Run identity mismatch')
    for key in ('id', 'campaign_id'):
        if re.fullmatch('[A-Za-z0-9_-]{1,128}', run[key]) is None:
            raise ValueError('Invalid run identity')
    index = json.loads((folder / 'inputs.json').read_text())
    actual_index = {}
    for entry in index:
        key = (entry['check_id'], entry['role'], entry['path'])
        if key in actual_index:
            raise ValueError('Duplicate archive declaration')
        valid_digest(entry['sha256'])
        if entry['storage'] is not None:
            if entry['storage'] != 'inputs/' + entry['sha256'] or digest(safe_member(folder, entry['storage'])) != entry['sha256']:
                raise ValueError('Archived input mismatch')
        actual_index[key] = (entry['sha256'], entry['storage'] is not None)
    expected = {}

    def need(check, role, name, sha, retained=True):
        valid_digest(sha)
        expected[check, role, name] = (sha, retained)
    for role in ('production', 'validation'):
        for name, sha in report[role + '_hashes'].items():
            if not role_selected(name, role) or name.startswith('/') or '..' in name.split('/'):
                raise ValueError('Invalid fingerprint path')
            need('run', 'fingerprint:' + role, name, sha)
    for name, sha in report['validation_hashes'].items():
        if name.startswith('scripts/') and '/' not in name[len('scripts/'):]:
            copied = folder / 'verifier_tooling' / Path(name).name
            if not copied.is_file() or digest(copied) != sha:
                raise ValueError('Verifier tooling source mismatch')
    helpers = report.get('assembly_inputs')
    if not isinstance(helpers, dict) or set(helpers) != {'evidence_attempt.py', 'evidence_attempt_audit.py'}:
        raise ValueError('Incomplete assembly helper declarations')
    for name, sha in helpers.items():
        if report['validation_hashes'].get('scripts/' + name) != sha or digest(safe_member(folder, name)) != sha:
            raise ValueError('Assembly helper source binding mismatch')
    expected_files = {'run.json'}
    derived = []
    failed = {}
    observations = []
    if len(report['validation']) != len(manifest['checks']):
        raise ValueError('Missing check coverage')
    for position, check in enumerate(manifest['checks']):
        name = check['id']
        expected_files.update((name + '.json', name + '.log'))
        record = json.loads(safe_member(folder, 'capture/' + name + '.json').read_text())
        if record != report['validation'][position]:
            raise ValueError('Report differs from original capture')
        if record['id'] != name or record['run_id'] != run['id'] or record['campaign_id'] != run['campaign_id'] or (record['manifest_sha256'] != run['manifest_sha256']) or (record['argv'] != check['argv']) or (record.get('env') != check.get('env', {})):
            raise ValueError('Check identity/command mismatch: ' + name)
        for role, key in (('production', 'source'), ('validation', 'validation')):
            if record[key + '_before'] != report[role + '_hashes'] or record[key + '_after'] != record[key + '_before']:
                raise ValueError('Changed check fingerprints: ' + name)
        verify_working_provenance(manifest, run, record)
        if record['log'] != name + '.log' or record['log_sha256'] != digest(folder / 'capture' / record['log']):
            raise ValueError('Log binding mismatch')
        completed = record['outcome'] in ('passed', 'failed') and type(record['exit']) is int
        acceptable = record['outcome'] == 'passed' and record['exit'] == 0
        if check.get('policy') == 'observation' and completed:
            observations.append(name)
            acceptable = True
        if not acceptable:
            failed[name] = record['outcome']
            if name not in ('hash-experiment', 'benchmarks'):
                raise ValueError('Unexpected unmet required check: ' + name)
            if record['outcome'] == 'blocked':
                if name != 'benchmarks' or record['exit'] is not None or record['artifacts']:
                    raise ValueError('Blocked benchmark was represented as executed')
            elif record['outcome'] == 'failed' and record['exit'] == 1 and (not record['artifacts']):
                log = (folder / 'capture' / record['log']).read_text()
                prefix = 'ValueError: Concurrent validation/build/analyzer processes: '
                lines = [line[len(prefix):] for line in log.splitlines() if line.startswith(prefix)]
                processes = ast.literal_eval(lines[-1]) if lines else []
                from evidence_processes import blocking_processes
                if not isinstance(processes, list) or not processes or blocking_processes(processes) != processes:
                    prefix = 'Measurement inspection unavailable: '
                    inspections = [line[len(prefix):] for line in log.splitlines() if line.startswith(prefix)]
                    if len(inspections) != 1:
                        raise ValueError('Unavailable benchmark reason not evidenced')
                    from evidence_processes import validate_inspection_failure
                    validate_inspection_failure(json.loads(inspections[0]), report['validation_hashes']['scripts/evidence_processes.py'])
            else:
                raise ValueError('Unexpected measurement failure')
        if completed:
            env = environment(manifest, check)
            context = dict(schema_version=5, campaign_id=run['campaign_id'], capture_run_id=run['id'], check_id=name, root=run['canonical_root'], production_hashes=report['production_hashes'], validation_hashes=report['validation_hashes'], declared_artifacts=check.get('artifacts', []))
            env['CODET_EVIDENCE_CONTEXT'] = json.dumps(context)
            if env != record['effective_environment']:
                raise ValueError('Effective environment mismatch')
            executable = record['executable']
            valid_digest(executable['sha256'])
            if record['executed_argv'] != [executable['path']] + check['argv'][1:]:
                raise ValueError('Resolved executable mismatch')
            for key in ('input', 'build_inputs', 'external_inputs'):
                if record[key + '_before'] != record[key + '_after']:
                    raise ValueError('Changed check inputs')
        for source, files in record.get('build_inputs_before', {}).items():
            for filename, sha in files.items():
                need(name, 'source:' + source, filename, sha)
        specs = file_specs(check)
        for filename, sha in record.get('input_before', {}).items():
            if filename not in specs:
                raise ValueError('Undeclared input')
            need(name, 'input', filename, sha, specs[filename].get('archive', True))
        for output in record['outputs']:
            need(name, 'output', output['path'], output['sha256'], output['archive'])
        if acceptable:
            if {x['path'] for x in record['outputs']} != {x['path'] for x in check.get('outputs', [])}:
                raise ValueError('Incomplete successful output declarations')
            if len(record['artifacts']) != len(check.get('artifacts', [])):
                raise ValueError('Incomplete successful artifacts')
        for position, saved in enumerate(record['artifacts']) if acceptable else []:
            spec = check.get('artifacts', [])[position]
            target = 'artifacts/' + name + '/' + Path(saved['path']).name
            if saved['path'] != spec['path'] or digest(safe_member(folder, target)) != saved['sha256']:
                raise ValueError('Artifact digest mismatch')
            envelope = json.loads((folder / target).read_text())
            context = {k: v for k, v in context.items() if k not in ('root', 'declared_artifacts')}
            if any((envelope.get(k) != v for k, v in context.items())) or envelope['kind'] != spec['kind']:
                raise ValueError('Artifact producer identity mismatch')
            if envelope['input_hashes'] != {p: record['input_before'][p] for p in spec.get('inputs', [])}:
                raise ValueError('Artifact input mismatch')
            if envelope['kind'] == 'gtest-isolation':
                from gtest_isolation import verify_payload
                if verify_payload(envelope['payload'], envelope['input_hashes'])['failures']:
                    raise ValueError('Isolated run failed')
            derived.append(target)
    if not failed or (failed.get('benchmarks') == 'blocked' and failed.get('hash-experiment') != 'failed'):
        raise ValueError('Unavailable scope mismatch')
    if {str(p.relative_to(folder / 'capture')) for p in (folder / 'capture').rglob('*') if p.is_file()} != expected_files:
        raise ValueError('Capture inventory coverage mismatch')
    if actual_index != expected:
        raise ValueError('Archive declaration coverage mismatch')
    if {str(p.relative_to(folder)) for p in (folder / 'artifacts').rglob('*') if p.is_file()} != set(derived):
        raise ValueError('Artifact inventory coverage mismatch')
    return dict(integrity_verified=True, eligible_current=False, complete=False, checks=len(manifest['checks']), unmet_required=failed, observations=observations, measurement_samples=0, limitation='This audits an incomplete attempt, not full campaign acceptance.')

def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.bundle), indent=2))
if __name__ == '__main__':
    main()
