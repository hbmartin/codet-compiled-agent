#!/usr/bin/env python3
"""Analyze modern sources with a dedicated Clang CMake configuration."""
import concurrent.futures
import hashlib
import json
import os
import pathlib
import shlex
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def clang_driver():
    requested = os.environ.get('CODET_CLANGXX')
    compiler = shutil.which(requested or 'clang++') or (None if requested else shutil.which('clang++-18'))
    if not compiler:
        raise SystemExit('Clang C++ compiler not found; install Clang or set CODET_CLANGXX')
    version = subprocess.run([compiler, '--version'], text=True, capture_output=True, check=True)
    if 'clang' not in version.stdout.lower():
        raise SystemExit('CODET_CLANGXX must select a Clang C++ driver')
    return compiler


def main():
    compiler = clang_driver()
    build = ROOT / 'build/clang-analysis'
    out = ROOT / 'build/modern-analysis'
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run(['cmake', '-S', str(ROOT), '-B', str(build),
                    '-DCMAKE_CXX_COMPILER=' + compiler, '-DCMAKE_BUILD_TYPE=Debug',
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCODET_BUILD_TESTS=OFF'], check=True)
    entries = [e for e in json.loads((build / 'compile_commands.json').read_text())
               if '/src/modern/' in e['file']]
    if not entries:
        raise SystemExit('No modern C++ sources in the Clang compilation database')

    def analyze(entry):
        command = entry.get('arguments') or shlex.split(entry['command'])
        args, i = [], 0
        while i < len(command):
            if command[i] == '-o':
                i += 2
                continue
            if command[i] == '-c':
                i += 1
                continue
            args.append(command[i])
            i += 1
        name = pathlib.Path(entry['file']).stem
        args += ['--analyze', '-Xanalyzer', '-analyzer-output=text', '-o', str(out / (name + '.plist'))]
        result = subprocess.run(args, cwd=entry['directory'], text=True, capture_output=True, check=False)
        diagnostics = result.stdout + result.stderr
        (out / (name + '.log')).write_text(diagnostics)
        return dict(source=entry['file'], sha256=hashlib.sha256(pathlib.Path(entry['file']).read_bytes()).hexdigest(),
                    command=args, exit_code=result.returncode, diagnostics=diagnostics)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(analyze, entries))
    (out / 'manifest.json').write_text(json.dumps(results, indent=2) + '\n')
    for result in results:
        print(pathlib.Path(result['source']).name + ': ' + str(result['exit_code']))
        if result['diagnostics']:
            print(result['diagnostics'])
    raise SystemExit(int(any(r['exit_code'] or 'warning:' in r['diagnostics'] for r in results)))


if __name__ == '__main__':
    main()
