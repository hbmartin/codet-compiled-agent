transformations = []

def checked_replace(text, before, after):
    count = text.count(before)
    if not count:
        raise ValueError('Historical transformation did not apply: ' + repr(before))
    transformations.append(dict(before=before, count=count))
    return text.replace(before, after)
import argparse
_parser = argparse.ArgumentParser(description=__doc__)
_parser.add_argument('--folder', type=__import__('pathlib').Path, required=True)
_parser.add_argument('--output', type=__import__('pathlib').Path, required=True)
args = _parser.parse_args()
if args.output.exists():
    _parser.error('Output already exists; choose a new historical replay output')
from pathlib import Path
import subprocess, tarfile, io, json, hashlib
root = Path(__file__).resolve().parents[2]
folder = args.folder.resolve()
folder.mkdir(parents=True, exist_ok=True)
source = folder / 'lock-count-src'
source.mkdir()
evidence = json.loads((root / 'docs/validation/prompt-boundary-ownership-followup.json').read_text())
ref = evidence['full_local_suite_ref']
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git', 'archive', ref], timeout=30))) as tar:
    tar.extractall(source, filter='data')
p = source / 'src/modern/platform.cpp'
s = checked_replace(checked_replace(p.read_text(), 'pthread_mutex_t native_metadata_mutex', 'std::atomic<size_t> native_lock_count{0};\npthread_mutex_t native_metadata_mutex'), 'bool locked = ::pthread_mutex_lock(&native_metadata_mutex) == 0;', 'bool locked = (++native_lock_count, ::pthread_mutex_lock(&native_metadata_mutex) == 0);')
s += '\nnamespace codet::detail {\nsize_t benchmark_native_locks(bool reset) {\n return reset ? native_lock_count.exchange(0) : native_lock_count.load();\n}\n}\n'
p.write_text(s)
p = source / 'tests/line_input_bench.cpp'
s = checked_replace(p.read_text(), 'namespace {\nbool observing', 'namespace codet::detail { size_t benchmark_native_locks(bool); }\nnamespace {\nbool observing')
s = checked_replace(s, '  observing = test::descriptor_bench::observing = true;', '  (void)detail::benchmark_native_locks(true);\n  observing = test::descriptor_bench::observing = true;')
s = checked_replace(s, '    } else if (mode == "allocation-positive") {', '    } else if (mode == "metadata-lock-positive") {\n      detail::reset_standard_diagnostics();\n    } else if (mode == "allocation-positive") {')
s = checked_replace(s, '{{"case", mode},', '{{"case", mode}, {"metadata_lock_acquisitions",detail::benchmark_native_locks(false)},')
p.write_text(s)
row = evidence['performance']['variants'][1]
command = ['-DCODET_DEPENDENCY_ROOT=' + str(root / '.deps') if x.startswith('-DCODET_DEPENDENCY_ROOT=') else x for x in row['configure']]
command[command.index('-S') + 1] = str(source)
command[command.index('-B') + 1] = str(folder / 'lock-count-release')
with (folder / 'lock-count-configure.log').open('w') as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
with (folder / 'lock-count-build.log').open('w') as log:
    subprocess.run(['cmake', '--build', str(folder / 'lock-count-release'), '--target', 'codet_line_input_bench', '-j4'], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
rows = []
for mode in ['metadata-lock-positive', 'editing', 'editing-basic', 'native-editing', 'native-busy', 'native-boundary', 'native-flush-boundary', 'native-rebind', 'native-unchanged', 'unchanged', 'warning-empty']:
    report = folder / 'lock-count.json'
    subprocess.run([str(folder / 'lock-count-release/codet_line_input_bench'), mode, str(report)], check=True, timeout=600)
    value = json.loads(report.read_text())
    rows.append(value)
    if mode == 'metadata-lock-positive':
        if not value['metadata_lock_acquisitions'] == 1000:
            raise ValueError(value)
    print(mode, value['metadata_lock_acquisitions'], flush=True)
(folder / 'lock-counts.json').write_text(json.dumps(dict(ref=ref, method='Separate instrumented copy counts each NativeMetadataGuard acquisition; timings from this build are not used.', instrumented_hashes={n: hashlib.sha256((source / n).read_bytes()).hexdigest() for n in ['src/modern/platform.cpp', 'tests/line_input_bench.cpp']}, rows=rows), indent=2) + '\n')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as result:
    json.dump(dict(label='historical replay; not current campaign evidence', folder=str(folder), source_ref=locals().get('ref'), produced=[str(p) for p in folder.glob('*.json')]), result, indent=2)
