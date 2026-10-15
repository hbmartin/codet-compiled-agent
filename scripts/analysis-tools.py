#!/usr/bin/env python3
"""Fetch checksum-pinned host analysis sources, then build without network access."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / 'analysis-tools.lock.json'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('fetch', 'build'))
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    item = json.loads(LOCK.read_text())['cppcheck']
    source = ROOT / '.deps/analysis' / ('cppcheck-' + item['version'])
    archive = source.parent / ('cppcheck-' + item['version'] + '.tar.gz')
    if args.action == 'fetch':
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists():
            with urllib.request.urlopen(item['url'], timeout=60) as response:
                data = response.read(32 * 1024 * 1024)
            if hashlib.sha256(data).hexdigest() != item['sha256']:
                raise SystemExit('Cppcheck archive checksum mismatch')
            archive.write_bytes(data)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != item['sha256']:
            raise SystemExit('Cppcheck archive checksum mismatch')
        with tarfile.open(archive) as package:
            destination = archive.parent.resolve()
            for entry in package.getmembers():
                target = (destination / entry.name).resolve()
                if not target.is_relative_to(destination) or entry.issym() or entry.islnk():
                    raise SystemExit('Unsafe analysis archive member')
            package.extractall(destination, filter='data')
        print(source)
        return
    if not (source / 'CMakeLists.txt').is_file():
        raise SystemExit('Missing pinned analysis sources; run make fetch-analysis-tools first')
    build = ROOT / 'build/analysis-tools/cppcheck'
    subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release',
                    '-DBUILD_GUI=OFF', '-DBUILD_TESTING=OFF', '-DHAVE_RULES=OFF'], check=True)
    subprocess.run(['cmake', '--build', str(build), '-j', str(args.jobs)], check=True)
    print(build / 'bin/cppcheck')

if __name__ == '__main__':
    main()
