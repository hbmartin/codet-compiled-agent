"""Relocated integrity audit with explicitly scoped Python API guards."""
import argparse
import builtins
import errno
import hashlib
import io
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path


def canonical(path, *, lstat=os.lstat, readlink=os.readlink):
    """Resolve path identity using saved primitives, never recursively patched APIs."""
    value = os.fsdecode(path)
    if not os.path.isabs(value):
        value = os.getcwd() + os.sep + value
    todo, done, hops = value.split(os.sep), [], 0
    while todo:
        part = todo.pop(0)
        if part in ('', '.'):
            continue
        if part == '..':
            if done:done.pop()
            continue
        candidate = os.sep + os.sep.join(done + [part])
        try:
            mode = lstat(candidate).st_mode
        except OSError as error:
            if error.errno not in (errno.ENOENT, errno.ENOTDIR):raise
            mode = 0
        if stat.S_ISLNK(mode):
            hops += 1
            if hops > 40:raise ValueError('Excessive offline-audit resolution symlinks')
            target = readlink(candidate)
            if os.path.isabs(target):done = []
            todo = target.split(os.sep) + todo
        else:
            done.append(part)
    return os.sep + os.sep.join(done)


def within(value, root):
    return value == root or value.startswith(root.rstrip(os.sep) + os.sep)


@contextmanager
def guarded_apis(originals):
    blocked, saved = [], []
    def check(path):
        if isinstance(path, int):return
        value = canonical(path)
        if any(within(value, root) for root in originals):
            blocked.append(value)
            raise PermissionError('Original checkout I/O is forbidden in this audit')
    def protect(owner, name):
        native = getattr(owner, name)
        def guarded(path, *args, **kwargs):
            check(path)
            return native(path, *args, **kwargs)
        saved.append((owner, name, native))
        setattr(owner, name, guarded)
    def no_process(*args, **kwargs):
        raise PermissionError('Process launch is forbidden in this audit')
    try:
        for owner,name in [(builtins,'open'),(io,'open'),(os,'open'),(os,'stat'),(os,'lstat')]:protect(owner,name)
        for name in ('Popen','run','check_output','check_call','call'):
            saved.append((subprocess,name,getattr(subprocess,name)))
            setattr(subprocess,name,no_process)
        yield blocked
    finally:
        for owner,name,native in reversed(saved):setattr(owner,name,native)


def validate_disposition(result, attempt, expected=None):
    if attempt:
        if result.get('integrity_verified') is not True or result.get('eligible_current') is not False:
            raise ValueError('Unexpected attempt audit disposition')
        unmet = result.get('unmet_required')
        if not isinstance(unmet, dict) or not unmet or any(not isinstance(k, str) or not isinstance(v, str) for k,v in unmet.items()):
            raise ValueError('Malformed audited unmet-check disposition')
    elif result.get('verified') is not True or result.get('eligible_current') is not True:
        raise ValueError('Unexpected complete audit disposition')
    if expected is not None and result.get('unmet_required', {}) != expected:
        raise ValueError('Audited unmet checks differ from --expect-unmet')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('originals', nargs='+')
    parser.add_argument('--expect-unmet', help='Optional JSON map of expected unmet checks')
    args = parser.parse_args(argv)
    expected = None
    if args.expect_unmet is not None:
        expected = json.loads(args.expect_unmet)
        if not isinstance(expected, dict) or any(not isinstance(k,str) or not isinstance(v,str) for k,v in expected.items()):
            raise ValueError('--expect-unmet must be a JSON string map')
    bundle, destination = args.bundle.resolve(), args.destination.resolve()
    originals = [canonical(value) for value in args.originals]
    if any(not Path(value).is_dir() for value in originals):
        raise ValueError('Original checkout roots must be existing directories')
    source = Path(__file__).read_bytes()
    helper_sha = hashlib.sha256(source).hexdigest()
    inventory_sha = hashlib.sha256((bundle/'inventory.json').read_bytes()).hexdigest()
    interpreter_sha = hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()
    previous_cwd, previous_path = os.getcwd(), list(sys.path)
    imports = [canonical(value) for value in sys.path if value]
    sys.dont_write_bytecode = True
    with tempfile.TemporaryDirectory(prefix='codet relocated audit ') as temporary:
        root = Path(temporary)
        if any(within(canonical(root), value) for value in originals):
            raise ValueError('Relocated audit workspace overlaps an original checkout root')
        moved = root/'moved bundle'
        shutil.copytree(bundle,moved)
        try:
            os.chdir(root)
            sys.path[:] = [p for p in imports if not any(within(p,o) for o in originals)]
            sys.path[:0] = [str(moved),str(moved/'verifier_tooling')]
            if not (moved/'verifier_tooling').exists():
                tooling = root/'archived verifier tooling'
                tooling.mkdir()
                index = json.loads((moved/'inputs.json').read_text())
                for item in index:
                    name = item['path']
                    if item['check_id']=='run' and item['role']=='fingerprint:validation' and name.startswith('scripts/') and name.count('/')==1 and name.endswith('.py'):
                        data = (moved/item['storage']).read_bytes()
                        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Changed archived verifier')
                        (tooling/Path(name).name).write_bytes(data)
                sys.path.insert(0,str(tooling))
            report = json.loads((moved/'report.json').read_text())
            attempt = 'attempt_bundle_version' in report
            # Installation precedes verifier imports, including aliases captured there.
            with guarded_apis(originals) as blocked:
                if attempt:
                    import evidence_attempt_audit
                    verify = evidence_attempt_audit.verify
                else:
                    from evidence_bundle import verify_bundle
                    verify = verify_bundle
                result = verify(moved)
                validate_disposition(result, attempt, expected)
                member = next(p for p in (moved/'inputs').iterdir() if p.is_file())
                member.write_bytes(b'corrupted offline negative control')
                try:verify(moved)
                except ValueError as error:
                    reason = str(error)
                    if 'inventory' not in reason.lower():raise
                    corruption = dict(rejected=True,member='inputs/'+member.name,reason=reason)
                else:raise ValueError('Corrupt archived input passed audit')
                if blocked:raise ValueError('Audit attempted original checkout I/O')
        finally:
            os.chdir(previous_cwd)
            sys.path[:] = previous_path
    receipt = dict(argv=sys.argv if argv is None else argv,kind='postpublication-offline-audit',driver_source=source.decode(),bundle_id=bundle.name,
        bundle_inventory_sha256=inventory_sha,helper_sha256=helper_sha,canonical_original_roots=originals,
        interpreter=dict(path=sys.executable,version=list(sys.version_info[:3]),sha256=interpreter_sha),
        relocated=True,guarded_file_apis=['builtins.open','io.open','os.open','os.stat','os.lstat'],
        guarded_process_apis=['subprocess.Popen','subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call'],
        guard_phase='Archived verifier import through verification and corruption control',
        guard_scope='Selected Python APIs only; previously cached aliases, cached pathlib accessors, directory listings and direct os process primitives are not blocked. This is not an OS sandbox.',
        audit=result,negative_control=corruption)
    destination.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))
    return receipt


if __name__ == '__main__':
    main()
