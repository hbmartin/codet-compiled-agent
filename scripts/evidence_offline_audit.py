"""Relocated integrity audit with explicitly scoped Python API guards."""
import argparse
import builtins
import errno
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import unicodedata
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
    def __init__(self, roots, native_stat=os.stat, native_lstat=os.lstat):
        self.native_stat = native_stat
        self.native_lstat = native_lstat
        self.paths = tuple(canonical(root) for root in roots)
        self.identities = set()
        for root in self.paths:
            info = native_stat(root)
            if not stat.S_ISDIR(info.st_mode):raise ValueError('Original checkout root is not a directory: ' + root)
            self.identities.add((info.st_dev, info.st_ino))

    def owns(self, probe, path):
        """Whether a native identity probe of an existing path names an original root."""
        try:info = probe(path)
        except OSError as error:
            if error.errno not in (errno.ENOENT, errno.ENOTDIR):raise
            return False
        return (info.st_dev, info.st_ino) in self.identities

    def contains(self, path, *, follow_final=True):
        value = canonical(path, follow_final=follow_final)
        if any(within(value, root) for root in self.paths):return True
        # A no-follow final component can itself be a case/firmlink alias of a root.
        if not follow_final and self.owns(self.native_lstat, value):return True
        current = value if follow_final else os.path.dirname(value)
        while True:
            if self.owns(self.native_stat, current):return True
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
            # Fail closed and record it: a verifier catching the error cannot hide the attempt.
            blocked.append(os.fsdecode(path))
            raise PermissionError('Offline guard cannot establish relative dir_fd path identity')
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


def bind_inventory(folder, expected_sha=None):
    """Bind every relocated byte to inventory.json with this driver's hashlib before any archived import."""
    path = folder / 'inventory.json'
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except OSError as error:
        if error.errno != errno.ELOOP:
            raise
        raise ValueError('Relocated inventory must be a regular file') from error
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise ValueError('Relocated inventory must be a regular file')
        with os.fdopen(descriptor, 'rb', closefd=False) as stream:
            raw = stream.read()
    finally:
        os.close(descriptor)
    inventory_sha = hashlib.sha256(raw).hexdigest()
    if expected_sha is not None and inventory_sha != expected_sha:
        raise ValueError('Relocated inventory SHA-256 differs from expected digest')
    try:inventory = json.loads(raw)
    except ValueError as error:raise ValueError('Invalid archived inventory') from error
    if not isinstance(inventory, dict) or any(not isinstance(k,str) or not isinstance(v,str) for k,v in inventory.items()):
        raise ValueError('Invalid archived inventory')
    actual = {}
    for path in sorted(folder.rglob('*')):
        if path.is_symlink():raise ValueError('Relocated bundle contains symlink: ' + path.relative_to(folder).as_posix())
        if path.is_file() and path != folder/'inventory.json':
            digest = hashlib.sha256()
            with open(path, 'rb') as stream:
                for block in iter(lambda: stream.read(1 << 20), b''):digest.update(block)
            actual[path.relative_to(folder).as_posix()] = digest.hexdigest()
    if actual != inventory:
        changed = sorted(name for name in set(actual) | set(inventory) if actual.get(name) != inventory.get(name))
        raise ValueError('Relocated bundle differs from archived inventory before verifier import: ' + ', '.join(changed[:5]))
    return inventory_sha, actual


def stage_verifier(moved, root, bound, entry_name):
    """Stage only verified Python sources in a fresh import directory."""
    sources = {Path(member).name: (member, digest) for member, digest in bound.items()
               if member.startswith('verifier_tooling/') and member.count('/') == 1 and member.endswith('.py')}
    if sources:
        tooling = root / 'verified sources' / 'verifier_tooling'
    else:
        tooling = root / 'archived verifier tooling'
        filenames = set()
        index = json.loads((moved / 'inputs.json').read_text())
        for item in index:
            name = item['path']
            if (item['check_id'] != 'run' or item['role'] != 'fingerprint:validation' or
                    not name.startswith('scripts/') or name.count('/') != 1 or not name.endswith('.py')):
                continue
            storage, digest = item['storage'], item['sha256']
            if not isinstance(storage, str) or storage not in bound or bound[storage] != digest:
                raise ValueError('Archived verifier source is not a bound bundle member: ' + repr(storage))
            filename = Path(name).name
            portable_name = unicodedata.normalize('NFC', filename).casefold()
            if portable_name in filenames:
                raise ValueError('Duplicate archived verifier source: ' + filename)
            filenames.add(portable_name)
            sources[filename] = (storage, digest)
    if entry_name not in sources:
        raise ValueError('Missing bound archived verifier entrypoint: ' + entry_name)
    tooling.mkdir(parents=True)
    for filename, (storage, digest) in sources.items():
        data = (moved / storage).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Changed archived verifier: ' + storage)
        (tooling / filename).write_bytes(data)
        if filename == entry_name:
            entry_data = data
    return tooling, entry_data, sources[entry_name][1]


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
    parser.add_argument('--expected-inventory-sha256', help='Trusted SHA-256 of the raw inventory.json bytes')
    args = parser.parse_args(argv)
    expected_sha = args.expected_inventory_sha256
    if expected_sha is not None and re.fullmatch(r'[0-9a-f]{64}', expected_sha) is None:
        parser.error('--expected-inventory-sha256 requires 64 lowercase hexadecimal digits')
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
    interpreter_sha = hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()
    previous_cwd, previous_path = os.getcwd(), list(sys.path)
    imports = [canonical(value) for value in sys.path if value]
    previous_bytecode = sys.dont_write_bytecode
    with tempfile.TemporaryDirectory(prefix='codet relocated audit ') as temporary:
        root = Path(temporary).resolve()
        if roots.contains(root):
            raise ValueError('Relocated audit workspace overlaps an original checkout root')
        moved = root/'moved bundle'
        shutil.copytree(bundle,moved,symlinks=True)
        inventory_sha, bound = bind_inventory(moved, expected_sha)
        report = json.loads((moved/'report.json').read_text())
        attempt = 'attempt_bundle_version' in report
        entry_name = 'evidence_attempt_audit.py' if attempt else 'evidence_bundle.py'
        tooling, entry_data, entry_sha = stage_verifier(moved, root, bound, entry_name)
        entry = tooling / entry_name
        try:
            os.chdir(root)
            sys.path[:] = [p for p in imports if not roots.contains(p)]
            sys.dont_write_bytecode = True
            sys.path[:0] = [str(tooling)]
            # Installation precedes verifier imports, including aliases captured there.
            with guarded_apis(originals) as blocked, archived_verifier(tooling):
                spec = importlib.util.spec_from_file_location(entry.stem, entry)
                module = importlib.util.module_from_spec(spec)
                sys.modules[entry.stem] = module
                exec(compile(entry_data, str(entry), 'exec', dont_inherit=True), module.__dict__)
                verify = module.verify if attempt else module.verify_bundle
                verifier = dict(path=str(entry.relative_to(root)), sha256=entry_sha)
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
                if blocked:raise ValueError('Audit attempted guarded I/O: ' + repr(blocked))
        finally:
            os.chdir(previous_cwd)
            sys.path[:] = previous_path
            sys.dont_write_bytecode = previous_bytecode
    receipt = dict(argv=sys.argv if argv is None else argv,kind='postpublication-offline-audit',driver_source=source.decode(),bundle_id=bundle.name,
        bundle_inventory_sha256=inventory_sha,helper_sha256=helper_sha,canonical_original_roots=originals,
        interpreter=dict(path=sys.executable,version=list(sys.version_info[:3]),sha256=interpreter_sha),
        relocated=True,verifier=verifier,pre_import_inventory_files=len(bound),guarded_file_apis=['builtins.open','io.open','os.open','os.stat','os.lstat'],
        guarded_process_apis=['subprocess.Popen','subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call'],
        guard_phase='Archived verifier import through verification and corruption control',
        guard_scope='Selected Python APIs only; descriptor-only access (integer paths), previously cached aliases, cached pathlib accessors, directory listings and direct os process primitives are not blocked. Relative dir_fd lookups are denied, recorded and fail the audit. This is not an OS sandbox.',
        audit=result,negative_control=corruption)
    if expected_sha is not None:
        receipt['expected_inventory_sha256'] = expected_sha
    destination.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))
    return receipt


if __name__ == '__main__':
    main()
