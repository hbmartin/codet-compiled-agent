"""Verified, clean archive extraction shared by dependency provisioning and builds."""
import hashlib
import pathlib
import shutil
import tarfile
import tempfile

def file_hash(path):
    with pathlib.Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()

def tree_hashes(root):
    root = pathlib.Path(root)
    result = {}
    for path in sorted(root.rglob('*')):
        if path.name == 'manifest.json':
            continue
        name = path.relative_to(root).as_posix()
        if path.is_symlink():
            result[name] = 'link:' + str(path.readlink())
        elif path.is_file():
            result[name] = file_hash(path)
    return result

def extract_verified(archive, expected, destination):
    """Hash and extract the same open archive; never overlay a mutable source tree."""
    destination = pathlib.Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = pathlib.Path(tempfile.mkdtemp(prefix=destination.name + '-extract-', dir=destination.parent))
    try:
        with pathlib.Path(archive).open('rb') as source:
            if hashlib.file_digest(source, 'sha256').hexdigest() != expected:
                raise ValueError('Archive checksum mismatch: ' + str(archive))
            source.seek(0)
            with tarfile.open(fileobj=source) as package:
                prefix = None
                for member in package:
                    original = pathlib.PurePosixPath(member.name)
                    if original.is_absolute() or '..' in original.parts or not original.parts:
                        raise ValueError('Unsafe archive member: ' + member.name)
                    if prefix is None:
                        prefix = original.parts[0]
                    if original.parts[0] != prefix:
                        raise ValueError('Archive must have one source root')
                    relative = pathlib.PurePosixPath(*original.parts[1:])
                    if str(relative) == '.':
                        continue
                    member = member.replace(name=str(relative))
                    if member.islnk():
                        link = pathlib.PurePosixPath(member.linkname)
                        if link.is_absolute() or '..' in link.parts or not link.parts or link.parts[0] != prefix:
                            raise ValueError('Unsafe archive hard link')
                        member = member.replace(linkname=str(pathlib.PurePosixPath(*link.parts[1:])))
                    package.extract(member, staging, filter='data')
        if destination.is_symlink():
            raise ValueError('Source destination must not be a symlink')
        if destination.exists():
            shutil.rmtree(destination)
        staging.rename(destination)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
