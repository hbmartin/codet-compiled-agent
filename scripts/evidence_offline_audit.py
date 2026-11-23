"""Relocated integrity audit with explicitly scoped Python API guards."""
import builtins
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    bundle=Path(sys.argv[1]).resolve();destination=Path(sys.argv[2]).resolve()
    originals=[Path(value) for value in sys.argv[3:]]
    if not originals:raise ValueError('Supply the original checkout roots to forbid')
    helper_sha=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    inventory_sha=hashlib.sha256((bundle/'inventory.json').read_bytes()).hexdigest()
    interpreter_sha=hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()
    sys.dont_write_bytecode=True
    with tempfile.TemporaryDirectory(prefix='codet relocated audit ') as temporary:
        root=Path(temporary);moved=root/'moved bundle';shutil.copytree(bundle,moved)
        os.chdir(root)
        sys.path[:]=[p for p in sys.path if p and not any(Path(p).resolve().is_relative_to(o) for o in originals)]
        sys.path.insert(0,str(moved));sys.path.insert(0,str(moved/'verifier_tooling'))
        if not (moved/'verifier_tooling').exists():
            tooling=root/'archived verifier tooling';tooling.mkdir()
            index=json.loads((moved/'inputs.json').read_text())
            for item in index:
                name=item['path']
                if item['check_id']=='run' and item['role']=='fingerprint:validation' and name.startswith('scripts/') and name.count('/')==1 and name.endswith('.py'):
                    archived=moved/item['storage'];data=archived.read_bytes()
                    if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Changed archived verifier')
                    (tooling/Path(name).name).write_bytes(data)
            sys.path.insert(0,str(tooling))
        report=json.loads((moved/'report.json').read_text())
        attempt='attempt_bundle_version' in report
        if attempt:
            import evidence_attempt_audit
            verify=evidence_attempt_audit.verify
        else:
            from evidence_bundle import verify_bundle
            verify=verify_bundle
        blocked=[]
        def check(path):
            if isinstance(path,int):return
            value=os.path.abspath(os.fsdecode(path))
            if any(value==str(o) or value.startswith(str(o)+os.sep) for o in originals):
                blocked.append(value);raise PermissionError('Original checkout I/O is forbidden in this audit')
        saved=[]
        def protect(owner,name):
            native=getattr(owner,name)
            def guarded(path,*args,**kwargs):check(path);return native(path,*args,**kwargs)
            saved.append((owner,name,native));setattr(owner,name,guarded)
        for owner,name in [(builtins,'open'),(io,'open'),(os,'open'),(os,'stat'),(os,'lstat')]:protect(owner,name)
        def no_process(*args,**kwargs):raise PermissionError('Process launch is forbidden in this audit')
        for name in ('Popen','run','check_output','check_call','call'):
            saved.append((subprocess,name,getattr(subprocess,name)));setattr(subprocess,name,no_process)
        try:
            result=verify(moved)
            if attempt:
                if result.get('integrity_verified') is not True or result.get('eligible_current') is not False:
                    raise ValueError('Unexpected attempt audit disposition')
                if result['unmet_required']!={'benchmarks':'failed'}:
                    raise ValueError('Unexpected required failures')
            elif result.get('verified') is not True or result.get('eligible_current') is not True:
                raise ValueError('Unexpected complete audit disposition')
            member=next(p for p in (moved/'inputs').iterdir() if p.is_file())
            member.write_bytes(b'corrupted offline negative control')
            try:verify(moved)
            except ValueError as error:
                reason=str(error)
                if 'inventory' not in reason.lower():raise
                corruption=dict(rejected=True,member='inputs/'+member.name,reason=reason)
            else:raise ValueError('Corrupt archived input passed audit')
            if blocked:raise ValueError('Audit attempted original checkout I/O')
        finally:
            for owner,name,native in reversed(saved):setattr(owner,name,native)
    receipt=dict(argv=sys.argv,kind='postpublication-offline-audit',driver_source=Path(__file__).read_text(),bundle_id=bundle.name,
        bundle_inventory_sha256=inventory_sha,helper_sha256=helper_sha,
        interpreter=dict(path=sys.executable,version=list(sys.version_info[:3]),sha256=interpreter_sha),
        relocated=True,guarded_file_apis=['builtins.open','io.open','os.open','os.stat','os.lstat'],
        guarded_process_apis=['subprocess.Popen','subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call'],
        guard_scope='Selected Python APIs only; cached pathlib accessors, directory listings and direct os process primitives are not blocked. This is not an OS sandbox.', 
        audit=result,negative_control=corruption)
    destination.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))


if __name__ == "__main__":
    main()
