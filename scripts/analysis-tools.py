#!/usr/bin/env python3
"""Fetch checksum-pinned host analysis sources, then build without network access."""
import argparse
import hashlib
import json
import subprocess
import urllib.request
from pathlib import Path

from verified_sources import extract_verified

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
        extract_verified(archive, item['sha256'], source)
        print(source)
        return
    if not (source / 'CMakeLists.txt').is_file():
        raise SystemExit('Missing pinned analysis sources; run make fetch-analysis-tools first')
    build = ROOT / 'build/analysis-tools/cppcheck'
    subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release',
                    '-DBUILD_GUI=OFF', '-DBUILD_TESTING=OFF', '-DHAVE_RULES=OFF'], check=True, timeout=600)
    subprocess.run(['cmake', '--build', str(build), '-j', str(args.jobs)], check=True, timeout=600)
    print(build / 'bin/cppcheck')

if __name__ == '__main__':
    main()
