"""Bounded path-resolution evidence, also checkable without the original filesystem."""
import os
from pathlib import Path


def _resolve(path, lookup):
    if not isinstance(path, str) or not path.startswith('/') or '\0' in path:
        raise ValueError('Invalid absolute resolution path')
    todo = path.split('/')[1:]
    done = []
    hops = 0
    while todo:
        part = todo.pop(0)
        if part in ('', '.'):
            continue
        if part == '..':
            if done:
                done.pop()
            continue
        candidate = '/' + '/'.join(done + [part])
        target = lookup(candidate)
        if target is None:
            done.append(part)
        else:
            hops += 1
            if hops > 40 or not isinstance(target, str) or not target or '\0' in target:
                raise ValueError('Invalid or excessive resolution symlinks')
            if target.startswith('/'):
                done = []
            todo = target.split('/') + todo
    return '/' + '/'.join(done)


def resolution(path):
    path = str(path)
    if not path.startswith('/'):
        path = str(Path.cwd()) + '/' + path
    links = []
    def lookup(name):
        entry = Path(name)
        if entry.is_symlink():
            target = os.readlink(entry)
            links.append(dict(path=name, target=target))
            return target
        if not entry.exists():
            raise ValueError('Resolution component missing: ' + name)
        return None
    return dict(path=path, canonical=_resolve(path, lookup), links=links)


def verify_resolution(value, path):
    if not isinstance(value, dict) or value.get('path') != path or not isinstance(value.get('links'), list):
        raise ValueError('Resolution path does not match its root/specification')
    links = list(value['links'])
    if len(links) > 40 or any(not isinstance(link, dict) or set(link) != {'path', 'target'} for link in links):
        raise ValueError('Invalid resolution chain')
    def lookup(name):
        if links and links[0]['path'] == name:
            return links.pop(0)['target']
        return None
    canonical = _resolve(path, lookup)
    if links or canonical != value.get('canonical'):
        raise ValueError('Inconsistent resolution chain')
    return canonical


def working_provenance(manifest, check):
    root = resolution(manifest['root'])
    spec = check.get('cwd', '.')
    path = spec if spec.startswith('/') else root['canonical'] + '/' + spec
    return dict(root=root, cwd=resolution(path), cwd_spec=spec)


def verify_working_provenance(manifest, run, record):
    before = record.get('path_before')
    if not isinstance(before, dict) or before != record.get('path_after'):
        raise ValueError('Working-directory resolution changed during execution')
    root = verify_resolution(before['root'], manifest['root'])
    if root != run['canonical_root'] or before['root'] != run.get('root_resolution'):
        raise ValueError('Working-directory root relationship mismatch')
    spec = check_spec = record.get('cwd_spec')
    if spec != before.get('cwd_spec') or not isinstance(check_spec, str):
        raise ValueError('Working-directory specification mismatch')
    path = spec if spec.startswith('/') else root + '/' + spec
    cwd = verify_resolution(before['cwd'], path)
    if record.get('cwd') != cwd or record.get('canonical_cwd') != cwd:
        raise ValueError('Working-directory provenance mismatch')
    return cwd
