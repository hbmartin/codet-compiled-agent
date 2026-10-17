#!/usr/bin/env python3
"""Explicit network step; normal configure/build/test never downloads sources."""
import argparse
import hashlib
import json
import pathlib
import tarfile
import urllib.request
from verified_sources import extract_verified

ROOT = pathlib.Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--tiger', action='store_true', help='Also fetch cross-build dependencies and TigerPorts patches')
    args = parser.parse_args()
    dest = ROOT / '.deps'
    dest.mkdir(exist_ok=True)
    locks=[(json.loads((ROOT / 'dependencies.lock.json').read_text()), '')]
    if args.tiger:
        locks.append((json.loads((ROOT / 'tiger-dependencies.lock.json').read_text()), 'tiger-'))
    for lock,prefix in locks:
      for dep in lock['dependencies']:
        archive = dest / (prefix+dep['name'] + ('.tar' if prefix else '.tar.gz'))
        if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != dep['sha256']:
            if args.offline:
                raise SystemExit('Missing or outdated archive: ' + str(archive))
            with urllib.request.urlopen(dep['url'], timeout=120) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != dep['sha256']:
                raise SystemExit('Checksum mismatch: ' + dep['name'])
            archive.write_bytes(data)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != dep['sha256']:
            raise SystemExit('Checksum mismatch: ' + dep['name'])
        extract_verified(archive, dep['sha256'], dest / (prefix + dep['name']))
        print(dep['name'], dep['version'], dep['sha256'])
      for patch in lock.get('patches',[]):
        path=dest/'tiger-patches'/patch['name']/patch['path']
        if not path.exists():
          if args.offline and not patch.get('local_file'): raise SystemExit('Missing patch: '+str(path))
          path.parent.mkdir(parents=True,exist_ok=True)
          path.write_bytes((ROOT/patch['local_file']).read_bytes() if patch.get('local_file') else urllib.request.urlopen(patch['url'],timeout=60).read())
        if hashlib.sha256(path.read_bytes()).hexdigest()!=patch['sha256']:
          raise SystemExit('Checksum mismatch: '+str(path))

if __name__ == '__main__':
    main()
