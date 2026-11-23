"""Relocated integrity audit with explicitly scoped Python API guards."""
import argparse
import builtins
import errno
import hashlib
import importlib
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


def canonical(path, *, follow_final=True, lstat=os.lstat, readlink=os.readlink):
    """Resolve path identity using saved primitives, never recursively patched APIs."""
    value = os.fsdecode(path)
    if not follow_final:
        parent, name = os.path.split(value)
        return os.path.join(canonical(parent or '.', lstat=lstat, readlink=readlink), name)
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


class OriginalRoots:
    """Use native ancestor identities as well as paths, including Darwin firmlinks."""
    def __init__(self, roots, native_stat=os.stat):
        self.native_stat = native_stat
        self.paths = tuple(canonical(root) for root in roots)
        self.identities = set()
        for root in self.paths:
            info = native_stat(root)
            if not stat.S_ISDIR(info.st_mode):raise ValueError('Original checkout root is not a directory: ' + root)
            self.identities.add((info.st_dev, info.st_ino))

    def contains(self, path, *, follow_final=True):
        value = canonical(path, follow_final=follow_final)
        if any(within(value, root) for root in self.paths):return True
        current = value if follow_final else os.path.dirname(value)
        while True:
            try:info = self.native_stat(current)
            except OSError as error:
                if error.errno not in (errno.ENOENT, errno.ENOTDIR):raise
            else:
                if (info.st_dev, info.st_ino) in self.identities:return True
            parent = os.path.dirname(current)
            if parent == current:return False
            current = parent


@contextmanager
def guarded_apis(originals):
    blocked, saved = [], []
    roots = OriginalRoots(originals)
    def check(path, follow_final, kwargs):
        if isinstance(path, int):return
        if kwargs.get('dir_fd') is not None and not os.path.isabs(path):
            raise ValueError('Offline guard cannot establish relative dir_fd path identity')
        if roots.contains(path, follow_final=follow_final):
            blocked.append(os.fsdecode(path))
            raise PermissionError('Original checkout I/O is forbidden in this audit')
    def protect(owner, name):
        native = getattr(owner, name)
        def guarded(path, *args, **kwargs):
            check(path, name != 'lstat' and kwargs.get('follow_symlinks', True), kwargs)
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


@contextmanager
def archived_verifier(tooling):
    """Checkout module caches must not override the chosen archived tooling."""
    names = {p.stem for p in tooling.glob('*.py')}
    previous = {name:sys.modules.pop(name) for name in names if name in sys.modules}
    try:
        importlib.invalidate_caches()
        yield
    finally:
        for name in names:sys.modules.pop(name, None)
        sys.modules.update(previous)


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
    roots = OriginalRoots(originals)
    source = Path(__file__).read_bytes()
    helper_sha = hashlib.sha256(source).hexdigest()
    inventory_sha = hashlib.sha256((bundle/'inventory.json').read_bytes()).hexdigest()
    interpreter_sha = hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()
    previous_cwd, previous_path = os.getcwd(), list(sys.path)
    imports = [canonical(value) for value in sys.path if value]
    sys.dont_write_bytecode = True
    with tempfile.TemporaryDirectory(prefix='codet relocated audit ') as temporary:
        root = Path(temporary).resolve()
        if roots.contains(root):
            raise ValueError('Relocated audit workspace overlaps an original checkout root')
        moved = root/'moved bundle'
        shutil.copytree(bundle,moved)
        try:
            os.chdir(root)
            sys.path[:] = [p for p in imports if not roots.contains(p)]
            tooling = moved/'verifier_tooling'
            if not tooling.exists():
                tooling = root/'archived verifier tooling'
                tooling.mkdir()
                index = json.loads((moved/'inputs.json').read_text())
                for item in index:
                    name = item['path']
                    if item['check_id']=='run' and item['role']=='fingerprint:validation' and name.startswith('scripts/') and name.count('/')==1 and name.endswith('.py'):
                        data = (moved/item['storage']).read_bytes()
                        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Changed archived verifier')
                        (tooling/Path(name).name).write_bytes(data)
            sys.path[:0] = [str(tooling),str(moved)]
            report = json.loads((moved/'report.json').read_text())
            attempt = 'attempt_bundle_version' in report
            # Installation precedes verifier imports, including aliases captured there.
            with guarded_apis(originals) as blocked, archived_verifier(tooling):
                if attempt:
                    import evidence_attempt_audit
                    module = evidence_attempt_audit
                    verify = module.verify
                else:
                    module = importlib.import_module("evidence_bundle")
                    verify = module.verify_bundle
                entry = Path(module.__file__).resolve()
                if entry.parent != tooling.resolve():raise ValueError('Verifier entrypoint is outside selected archived tooling')
                verifier = dict(path=str(entry.relative_to(root)), sha256=hashlib.sha256(entry.read_bytes()).hexdigest())
                if tooling == moved/'verifier_tooling':
                    inventory = json.loads((moved/'inventory.json').read_text())
                    if inventory.get('verifier_tooling/'+entry.name) != verifier['sha256']:
                        raise ValueError('Verifier entrypoint differs from archived inventory')
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
        relocated=True,verifier=verifier,guarded_file_apis=['builtins.open','io.open','os.open','os.stat','os.lstat'],
        guarded_process_apis=['subprocess.Popen','subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call'],
        guard_phase='Archived verifier import through verification and corruption control',
        guard_scope='Selected Python APIs only; previously cached aliases, cached pathlib accessors, directory listings and direct os process primitives are not blocked. This is not an OS sandbox.',
        audit=result,negative_control=corruption)
    destination.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))
    return receipt


if __name__ == '__main__':
    main()
