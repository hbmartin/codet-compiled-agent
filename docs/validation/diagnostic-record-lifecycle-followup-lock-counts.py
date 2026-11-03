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
import json, shlex, subprocess
root = Path(__file__).resolve().parents[2]
folder = args.folder.resolve()
records = []
for variant in ['baseline', 'correctness', 'optimized']:
    build = folder / (variant + '-release')
    src = folder / (variant + '-src')
    out = folder / (variant + '-locks')
    out.mkdir()
    rows = json.loads((build / 'compile_commands.json').read_text())

    def args(file):
        row = next((r for r in rows if r['file'].endswith(file)))
        a = shlex.split(row['command'])
        return a[:a.index('-o')]
    s = checked_replace(checked_replace((src / 'src/modern/platform.cpp').read_text(), 'pthread_mutex_t native_metadata_mutex', 'std::atomic<size_t> native_lock_count{0};\npthread_mutex_t native_metadata_mutex'), 'bool locked = ::pthread_mutex_lock(&native_metadata_mutex) == 0;', 'bool locked = (++native_lock_count, ::pthread_mutex_lock(&native_metadata_mutex) == 0);')
    s += '\nnamespace codet::detail { size_t measured_locks(bool reset) { return reset ? native_lock_count.exchange(0) : native_lock_count.load(); } }\n'
    (out / 'platform.cpp').write_text(s)
    commands = [args('src/modern/platform.cpp') + ['-c', str(out / 'platform.cpp'), '-o', str(out / 'platform.o')]]
    s = checked_replace(checked_replace(checked_replace(checked_replace((src / 'tests/line_input_bench.cpp').read_text(), 'namespace {\nbool observing', 'namespace codet::detail { size_t measured_locks(bool); }\nnamespace {\nbool observing'), '  observing = test::descriptor_bench::observing = true;', '  (void)detail::measured_locks(true);\n  observing = test::descriptor_bench::observing = true;'), '    } else if (mode == "allocation-positive") {', '    } else if (mode == "lock-positive") {\n      detail::reset_standard_diagnostics();\n    } else if (mode == "allocation-positive") {'), '{{"case", mode},', '{{"case", mode}, {"locks",detail::measured_locks(false)},')
    (out / 'line.cpp').write_text(s)
    link = shlex.split(subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'codet_line_input_bench'], text=True, timeout=30).splitlines()[-1])
    libs = link[link.index('libcodet_core.a'):link.index('&&', 2)]
    commands.append(args('tests/line_input_bench.cpp') + ['-I' + str(src / 'tests'), str(out / 'line.cpp'), str(out / 'platform.o')] + libs + ['-o', str(out / 'line')])
    with (out / 'build.log').open('w') as log:
        for command in commands:
            subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    for mode in ['lock-positive', 'editing', 'editing-basic', 'native-editing', 'native-busy', 'native-flush-boundary', 'native-boundary', 'native-rebind', 'native-unchanged', 'warning-empty']:
        report = out / 'report.json'
        subprocess.run([str(out / 'line'), mode, str(report)], check=True, timeout=600)
        r = json.loads(report.read_text())
        r.update(variant=variant)
        records.append(r)
        if mode == 'lock-positive':
            if not r['locks'] == 1000:
                raise ValueError(r)
        print(variant, mode, r['locks'], flush=True)
(folder / 'lock-counts.json').write_text(json.dumps(records, indent=2) + '\n')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as result:
    json.dump(dict(label='historical replay; not current campaign evidence', folder=str(folder), source_ref=locals().get('ref'), produced=[str(p) for p in folder.glob('*.json')]), result, indent=2)
