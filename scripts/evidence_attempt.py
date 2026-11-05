"""Publish an explicitly incomplete attempt without weakening strict campaign acceptance."""
import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

from evidence_bundle import rename_exclusive
from evidence_inputs import directory_hashes, file_specs, output_hashes, source_inputs, verify_tools
from validation_evidence import (
    _context,
    atomic_json,
    digest,
    input_hashes,
    load_manifest,
    production_hashes,
    validate_artifact,
    validation_hashes,
)


def assemble(root, campaign, destination, reason):
    root, campaign, destination = (Path(root).resolve(), Path(campaign).resolve(), Path(destination).absolute())
    if not isinstance(reason, str) or not reason.strip():
        raise ValueError('An explicit unavailable-measurement explanation is required')
    manifest_path = campaign / 'manifest.json'
    manifest = load_manifest(manifest_path)
    capture = campaign / 'capture'
    if Path(manifest['root']).resolve() != root:
        raise ValueError('Attempt root differs from manifest')
    run = json.loads((capture / 'run.json').read_text())
    production = production_hashes(root)
    validation = validation_hashes(root)
    verify_tools(manifest)
    if run['manifest_sha256'] != digest(manifest_path):
        raise ValueError('Changed attempt manifest')
    if destination.exists():
        raise ValueError('Attempt destination already exists')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.attempt-', dir=destination.parent) as temporary:
        stage = Path(temporary) / 'bundle'
        (stage / 'capture').mkdir(parents=True)
        shutil.copy2(manifest_path, stage / 'manifest.json')
        shutil.copy2(capture / 'run.json', stage / 'capture/run.json')
        index = []
        records = []
        artifacts = []

        def archive(check, role, name, path, sha, retained=True):
            if digest(path) != sha:
                raise ValueError('Changed attempt input: ' + name)
            storage = 'inputs/' + sha if retained else None
            if storage and (not (stage / storage).exists()):
                (stage / storage).parent.mkdir(exist_ok=True)
                shutil.copy2(path, stage / storage)
            index.append(dict(check_id=check, role=role, path=name, sha256=sha, storage=storage))
        for role, hashes in (('production', production), ('validation', validation)):
            for name, sha in hashes.items():
                archive('run', 'fingerprint:' + role, name, root / name, sha)
        for check in manifest['checks']:
            name = check['id']
            record = json.loads((capture / (name + '.json')).read_text())
            records.append(record)
            if record['source_before'] != production or record['source_after'] != production or record['validation_before'] != validation or (record['validation_after'] != validation):
                raise ValueError('Stale attempt fingerprints: ' + name)
            if record['manifest_sha256'] != run['manifest_sha256'] or record['log_sha256'] != digest(capture / record['log']):
                raise ValueError('Changed attempt record/log')
            shutil.copy2(capture / (name + '.json'), stage / 'capture' / (name + '.json'))
            shutil.copy2(capture / record['log'], stage / 'capture' / record['log'])
            if record['outcome'] in ('passed', 'failed'):
                if record['input_before'] != input_hashes(root, file_specs(check)) or record['input_after'] != record['input_before']:
                    raise ValueError('Changed file inputs: ' + name)
                if record['build_inputs_before'] != source_inputs(root, check.get('source_inputs', [dict(path='.')])) or record['build_inputs_after'] != record['build_inputs_before']:
                    raise ValueError('Changed source inputs: ' + name)
                if record['external_inputs_before'] != directory_hashes(root, check.get('directory_inputs', [])) or record['external_inputs_after'] != record['external_inputs_before']:
                    raise ValueError('Changed dependency inputs: ' + name)
            if record['outcome'] == 'passed' or check.get('policy') == 'observation':
                if record['outputs'] != output_hashes(root, check.get('outputs', [])):
                    raise ValueError('Changed produced outputs')
            for source, files in record.get('build_inputs_before', {}).items():
                for filename, sha in files.items():
                    archive(name, 'source:' + source, filename, root / source / filename, sha)
            specs = file_specs(check)
            for filename, sha in record.get('input_before', {}).items():
                archive(name, 'input', filename, root / filename, sha, specs[filename].get('archive', True))
            for output in record['outputs']:
                archive(name, 'output', output['path'], root / output['path'], output['sha256'], output['archive'])
            for position, saved in enumerate(record['artifacts']):
                spec = check['artifacts'][position]
                context = _context(root, run, name, production, validation)
                envelope = validate_artifact(root / saved['path'], spec, context)
                if digest(root / saved['path']) != saved['sha256']:
                    raise ValueError('Changed artifact')
                target = stage / 'artifacts' / name / Path(saved['path']).name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(root / saved['path'], target)
                artifacts.append(envelope)
        (stage / 'verifier_tooling').mkdir()
        for name, sha in validation.items():
            if name.startswith('scripts/') and '/' not in name[len('scripts/'):]:
                if digest(root / name) != sha:
                    raise ValueError('Changed verifier tooling: ' + name)
                shutil.copy2(root / name, stage / 'verifier_tooling' / Path(name).name)
        helpers = {name: validation['scripts/' + name] for name in ('evidence_attempt.py', 'evidence_attempt_audit.py')}
        for name, sha in helpers.items():
            source = root / 'scripts' / name
            if digest(source) != sha:
                raise ValueError('Changed assembly helper: ' + name)
            shutil.copy2(source, stage / name)
        (stage / 'verify-attempt.py').write_text('import sys\nsys.dont_write_bytecode = True\nfrom evidence_attempt_audit import main\nmain()\n')
        (stage / 'assemble-attempt.py').write_text('import sys\nsys.dont_write_bytecode = True\nfrom evidence_attempt import main\nmain()\n')
        report = dict(attempt_bundle_version=2, assembly_inputs=helpers, schema_version=5, binding='postcapture-attempt-inventory-not-campaign-acceptance', complete=False, eligible_current=False, campaign_id=run['campaign_id'], capture_run_id=run['id'], production_hashes=production, validation_hashes=validation, validation=records, derived_artifacts=artifacts, benchmark_exception=reason, measurement_samples=0)
        atomic_json(stage / 'report.json', report)
        atomic_json(stage / 'inputs.json', index)
        atomic_json(stage / 'measurements.json', [])
        (stage / 'README.md').write_text('This is an incomplete attempt bundle, not a passing campaign. The original v5 manifest, required failure/block, captures and bound artifacts are preserved. The quiet-window check observed another Python unittest; no benchmark samples are attributed to this source. Run Python 3.12+ verify-attempt.py to audit inventory, archives and producer links. Exit 0 means attempt integrity only; eligible_current remains false. Standard campaign acceptance still rejects the required measurement failure. Original checkout paths are opaque provenance. No missing historical captures were reconstructed.\n')
        atomic_json(stage / 'inventory.json', {str(p.relative_to(stage)): digest(p) for p in sorted(stage.rglob('*')) if p.is_file()})
        import importlib.util
        spec = importlib.util.spec_from_file_location('attempt_verifier', stage / 'evidence_attempt_audit.py')
        module = importlib.util.module_from_spec(spec)
        previous = sys.dont_write_bytecode
        try:
            sys.dont_write_bytecode = True
            spec.loader.exec_module(module)
            print(json.dumps(module.verify(stage), indent=2))
        finally:
            sys.dont_write_bytecode = previous
        if production_hashes(root) != production or validation_hashes(root) != validation:
            raise ValueError('Source changed while staging')
        rename_exclusive(stage, destination)
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--campaign', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--unavailable-reason', required=True)
    args = parser.parse_args()
    print(assemble(args.root, args.campaign, args.output, args.unavailable_reason))
if __name__ == '__main__':
    main()
