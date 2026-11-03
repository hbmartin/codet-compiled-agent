import argparse
_parser = argparse.ArgumentParser(description=__doc__)
_parser.add_argument('--folder', type=__import__('pathlib').Path, required=True)
_parser.add_argument('--output', type=__import__('pathlib').Path, required=True)
args = _parser.parse_args()
if args.output.exists():
    _parser.error('Output already exists; choose a new historical replay output')
import csv, json, statistics, subprocess, time
from pathlib import Path
root = Path(__file__).resolve().parents[2]
folder = args.folder.resolve()
rows = []
if any(((folder / name).exists() for name in ('final-measurements.json', 'final-performance-summary.json', 'final-performance.csv', 'final-performance-samples.csv'))):
    raise ValueError('Historical measurement outputs already exist; use fresh builds/results')
print('Waiting for validation, analyzers and Release builds to finish', flush=True)
variants = ['baseline', 'correctness', 'optimized']
cases = {'line': ['allocation-positive', 'geometry-positive', 'busy', 'native-busy', 'editing', 'editing-basic', 'editing-patterned', 'native-editing', 'deletion', 'unchanged', 'deferred-tail', 'native-boundary', 'native-flush-boundary', 'native-rebind', 'native-unchanged', 'warning-empty', 'warning-new', 'warning-failed-idle'], 'output': ['write-positive', 'stat-positive', 'flags-positive', 'bounds-positive', 'diagnostics-amplified', 'diagnostics-standalone-amplified', 'diagnostics-pipe-amplified', 'budget-full', 'jsonl-small', 'pipe-jsonl-small', 'editor'], 'export': ['small', 'large']}
started = time.monotonic()
import sys
sys.path.insert(0, str(root / 'scripts'))
from validation_evidence import require_quiet_processes
require_quiet_processes()
for number in (1, 2):
    for harness, workloads in cases.items():
        for case in workloads:
            for sample in range(6):
                order = variants[(number + sample) % 3:] + variants[:(number + sample) % 3]
                for variant in order if sample % 2 else order[::-1]:
                    build = folder / (variant + '-release')
                    report = folder / 'measurement.json'
                    report.unlink(missing_ok=True)
                    executable = {'line': 'codet_line_input_bench', 'ui': 'codet_ui_render_bench', 'output': 'codet_output_bench', 'export': 'codet_export_bench'}[harness]
                    command = [str(build / executable), case, str(report)] + (['file'] if harness == 'export' else [])
                    result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=180)
                    if result.returncode:
                        raise RuntimeError((command, result.returncode, result.stderr.decode(errors='replace')))
                    row = json.loads(report.read_text())
                    row.update(set=number, sample=sample, variant=variant, harness=harness, workload=case, warmup=sample == 0)
                    if harness == 'line':
                        if case == 'allocation-positive':
                            if not (row['allocations'] == 1000 and row['allocated_bytes'] == 32000):
                                raise ValueError(row)
                        if case == 'geometry-positive':
                            if not row['geometry_queries'] == 1000:
                                raise ValueError(row)
                        if variant != 'baseline' and case in ('busy', 'native-busy', 'unchanged'):
                            if not row['allocations'] == row['allocated_bytes'] == row['prepared_input_bytes'] == 0:
                                raise ValueError(row)
                            if not row['bytes'] <= row['iterations'] * 256:
                                raise ValueError(row)
                        if case in ('unchanged', 'native-unchanged'):
                            if not row['bytes'] == row['output_operations'] == row['geometry_queries'] == 0:
                                raise ValueError(row)
                        if case in ('editing', 'editing-basic', 'editing-patterned', 'native-editing'):
                            if not row['prepared_input_bytes'] <= 40960:
                                raise ValueError(row)
                        if case == 'warning-empty':
                            if not row['warning_attempts'] == row['allocations'] == row['output_operations'] == 0:
                                raise ValueError(row)
                        if case == 'warning-failed-idle' and variant != 'baseline':
                            if not (row['warning_attempts'] == 1 and row['allocations'] == 0):
                                raise ValueError(row)
                    if harness == 'ui':
                        if case == 'width-positive':
                            if not row['locale_conversions'] == 128:
                                raise ValueError(row)
                        if case == 'allocation-positive':
                            if not (row['allocations'] == 1 and row['allocated_bytes'] == 4096):
                                raise ValueError(row)
                        if case == 'input-narrow':
                            if not row['locale_conversions'] == 0:
                                raise ValueError(row)
                        if case.startswith('presentation-'):
                            if not (row['history_budget'] == 262144 and row['retained_payload_bytes'] <= 262144):
                                raise ValueError(row)
                    if harness == 'output' and case.endswith('-positive'):
                        if not row[{'write-positive': 'writes', 'stat-positive': 'fstat', 'flags-positive': 'fcntl', 'bounds-positive': 'fpathconf'}[case]] == 128:
                            raise ValueError(row)
                    rows.append(row)
                (folder / 'final-measurements.json').write_text(json.dumps(rows, indent=2) + '\n')
            print('Measured', number, harness, case, round(time.monotonic() - started, 1), flush=True)
summary = []
for number in (1, 2):
    for harness, workloads in cases.items():
        for case in workloads:
            for variant in variants:
                samples = [r for r in rows if r['set'] == number and r['harness'] == harness and (r['workload'] == case) and (r['variant'] == variant) and (not r['warmup'])]
                metrics = [key for key in samples[0] if isinstance(samples[0][key], (int, float)) and (not isinstance(samples[0][key], bool)) and (key not in ('set', 'sample'))]
                summary.append(dict(set=number, harness=harness, workload=case, variant=variant, **{key: statistics.median((r[key] for r in samples)) for key in metrics}))
(folder / 'final-performance-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
for filename, records in [('final-performance.csv', summary), ('final-performance-samples.csv', rows)]:
    with (folder / filename).open('w', newline='') as output:
        fields = sorted(set().union(*(r.keys() for r in records)))
        writer = csv.DictWriter(output, fieldnames=fields, lineterminator='\n')
        writer.writeheader()
        writer.writerows(records)
print('Measurement complete', round(time.monotonic() - started, 1), flush=True)
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as result:
    json.dump(dict(label='historical replay; not current campaign evidence', folder=str(folder), source_ref=locals().get('ref'), produced=[str(p) for p in folder.glob('*.json')]), result, indent=2)
