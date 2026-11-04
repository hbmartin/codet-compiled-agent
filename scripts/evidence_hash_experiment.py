"""Measure snapshot hashing implementations against one immutable input set."""
import json
import statistics
import sys
from pathlib import Path

from evidence_benchmarks import verify_samples
from evidence_inputs import sha256
from evidence_processes import require_quiet_processes
from process_supervision import run as supervised_run

ROLES = ('production', 'validation', 'build')
METRICS_VERSION = 2
METRICS = ('milliseconds', 'files', 'peak_traced_bytes', 'retained_traced_bytes',
           'retained_traced_blocks', 'explicit_stat_calls', 'direntry_is_file_calls',
           'direntry_is_dir_calls')
METRIC_SCOPE = dict(operations='profiled-python-calls', memory='live-traced-memory')
PROGRAM = r'''
import cProfile,hashlib,json,os,sys,time,tracemalloc
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from evidence_inputs import role_hashes
root,role=Path(sys.argv[2]),sys.argv[3]
start=time.perf_counter()
for _ in range(5): hashes=role_hashes(root,role)
elapsed=(time.perf_counter()-start)*1000
counter=cProfile.Profile();counter.enable();os.stat(root);counter.disable()
stat_control=sum(s.callcount for s in counter.getstats() if isinstance(s.code,str) and 'posix.stat' in s.code)
with os.scandir(root) as entries: entry=next(entries)
counter=cProfile.Profile();counter.enable();entry.is_file();entry.is_dir();counter.disable()
file_control=sum(s.callcount for s in counter.getstats() if isinstance(s.code,str) and 'DirEntry' in s.code and 'is_file' in s.code)
dir_control=sum(s.callcount for s in counter.getstats() if isinstance(s.code,str) and 'DirEntry' in s.code and 'is_dir' in s.code)
if (stat_control,file_control,dir_control)!=(1,1,1):raise ValueError('Python call counter positive control failed')
tracemalloc.start();control=bytearray(4096);allocation_control=tracemalloc.get_traced_memory()[0]
if allocation_control<4096:raise ValueError('Live traced memory positive control failed')
del control;tracemalloc.stop()
tracemalloc.start()
profile=cProfile.Profile();profile.enable(); measured=role_hashes(root,role);profile.disable()
retained,peak=tracemalloc.get_traced_memory();blocks=sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"));tracemalloc.stop()
if not hashes or measured!=hashes:raise ValueError('Hash snapshot changed during sample')
functions=[]
for entry in profile.getstats():
 code=entry.code
 functions.append(dict(name=code if isinstance(code,str) else code.co_name,calls=entry.callcount,
                      own_seconds=entry.inlinetime,total_seconds=entry.totaltime))
print(json.dumps(dict(milliseconds=elapsed,files=len(hashes),peak_traced_bytes=peak,
 retained_traced_bytes=retained,retained_traced_blocks=blocks,
 explicit_stat_calls=sum(f["calls"] for f in functions if "posix.stat" in f["name"]),
 direntry_is_file_calls=sum(f["calls"] for f in functions if "DirEntry" in f["name"] and "is_file" in f["name"]),
 direntry_is_dir_calls=sum(f["calls"] for f in functions if "DirEntry" in f["name"] and "is_dir" in f["name"]),
 positive_controls=dict(explicit_stat_calls=stat_control,direntry_is_file_calls=file_control,
                        direntry_is_dir_calls=dir_control,live_traced_bytes=allocation_control),
 identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest(),functions=functions)))
'''


def latency_gate(payload):
    return all(
        next(row['milliseconds'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <
        next(row['milliseconds'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline')
        for number in (1, 2) for role in ROLES)


def verify_experiment(payload):
    version = payload.get('metrics_version', 1)
    if type(version) is not int or version not in (1, METRICS_VERSION):
        raise ValueError('Unsupported hashing metric version')
    verify_samples(payload['rows'], payload['summary'], {'hash-snapshot': list(ROLES)})
    for role in ROLES:
        identities = {row['identity'] for row in payload['rows'] if row['workload'] == role}
        if len(identities) != 1:
            raise ValueError('Hashing experiment digests differ')
    if 'code_hashes' in payload:
        identities = payload['code_hashes']
        names = {'evidence_inputs.py', 'verified_sources.py'}
        if set(identities) != {'baseline', 'changed'} or any(set(value) != names for value in identities.values()):
            raise ValueError('Hash experiment code identity incomplete')
    if version == 1 and any('explicit_stat_calls' in row for row in payload['rows']):
        raise ValueError('Hash experiment metric version is missing or inconsistent')
    if version == METRICS_VERSION:
        if payload.get('metric_scope') != METRIC_SCOPE or 'work_gate_passed' in payload:
            raise ValueError('Hash experiment must label observed metric scope')
        if not payload.get('profiles') or not payload.get('code_hashes'):
            raise ValueError('Versioned hash experiment profiling evidence missing')
        for row in payload['rows'] + payload['summary']:
            if set(METRICS) - set(row) or {'allocations', 'metadata_queries', 'peak_bytes'} & set(row):
                raise ValueError('Hash experiment uses ambiguous metric names')
            if row['retained_traced_bytes'] > row['peak_traced_bytes']:
                raise ValueError('Hash experiment live memory exceeds peak')
    if 'profiles' in payload:
        samples = {(row['set'], row['sample'], row['variant'], row['workload']): row for row in payload['rows']}
        seen = set()
        for profile in payload['profiles']:
            key = profile['set'], profile['sample'], profile['variant'], profile['role']
            if key in seen or key not in samples or not profile.get('functions'):
                raise ValueError('Hash experiment profiling coverage mismatch')
            seen.add(key)
            counts = dict(explicit_stat_calls=sum(f['calls'] for f in profile['functions'] if 'posix.stat' in f['name']))
            if version == 1:
                if counts['explicit_stat_calls'] != samples[key]['metadata_queries'] or counts['explicit_stat_calls'] <= 0:
                    raise ValueError('Hash experiment metadata counter mismatch')
            else:
                controls = profile.get('positive_controls', {})
                for metric, method in (('direntry_is_file_calls', 'is_file'), ('direntry_is_dir_calls', 'is_dir')):
                    counts[metric] = sum(f['calls'] for f in profile['functions'] if 'DirEntry' in f['name'] and method in f['name'])
                if (any(type(controls.get(name)) is not int or controls[name] != 1 for name in counts) or
                        type(controls.get('live_traced_bytes')) is not int or controls['live_traced_bytes'] < 4096):
                    raise ValueError('Hash experiment counter positive control failed')
                if any(count != samples[key][name] for name, count in counts.items()):
                    raise ValueError('Hash experiment Python call counter mismatch')
        if seen != set(samples):
            raise ValueError('Hash experiment profiling coverage incomplete')
    accepted = latency_gate(payload)
    if payload['latency_gate_passed'] != accepted:
        raise ValueError('Hashing experiment acceptance does not match raw samples')
    if version == METRICS_VERSION:
        return accepted  # Observed calls/live blocks are not total allocation or syscall work.
    # Historical v1 fields mean explicit Python stat calls and retained snapshot blocks.
    less_work = all(
        next(row['metadata_queries'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <
        next(row['metadata_queries'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline') and
        all(next(row[key] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <=
            next(row[key] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline')
            for key in ('peak_bytes', 'allocations'))
        for number in (1, 2) for role in ROLES)
    if payload.get('work_gate_passed') != less_work:
        raise ValueError('Hashing experiment work acceptance does not match raw samples')
    return accepted and less_work


def measure(root, correctness, python=sys.executable):
    variants = dict(baseline=correctness / 'scripts', changed=root / 'scripts')
    code = {variant: {name: sha256(path / name) for name in ('evidence_inputs.py', 'verified_sources.py')}
            for variant, path in variants.items()}
    rows, profiles = [], []
    for number in (1, 2):
        for role in ROLES:
            require_quiet_processes()
            for sample in range(6):
                order = ('baseline', 'changed') if (number + sample) % 2 else ('changed', 'baseline')
                for variant in order:
                    argv = [python, '-c', PROGRAM, str(variants[variant]), str(root), role]
                    result = supervised_run(argv, check=True, text=True, capture_output=True, timeout=180)
                    row = json.loads(result.stdout)
                    profiles.append(dict(set=number, sample=sample, variant=variant, role=role,
                                         functions=row.pop('functions'), positive_controls=row.pop('positive_controls'),
                                         argv=argv, exit=result.returncode))
                    row.update(set=number, sample=sample, variant=variant, harness='hash-snapshot',
                               workload=role, warmup=sample == 0)
                    rows.append(row)
    summary = []
    for number in (1, 2):
        for role in ROLES:
            for variant in variants:
                values = [r for r in rows if r['set'] == number and r['workload'] == role and r['variant'] == variant and not r['warmup']]
                summary.append(dict(set=number, harness='hash-snapshot', workload=role, variant=variant,
                    **{key: statistics.median(r[key] for r in values) for key in METRICS}))
    payload = dict(rows=rows, summary=summary, profiles=profiles, code_hashes=code,
                   metrics_version=METRICS_VERSION, metric_scope=METRIC_SCOPE)
    payload['latency_gate_passed'] = latency_gate(payload)
    verify_experiment(payload)
    return payload


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--correctness', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.output.open('x') as stream:
        json.dump(measure(args.root.resolve(), args.correctness.resolve()), stream, indent=2)
        stream.write('\n')
