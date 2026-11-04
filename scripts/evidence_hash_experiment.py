"""Measure snapshot hashing implementations against one immutable input set."""
import json
import statistics
import subprocess
import sys
from pathlib import Path

from evidence_benchmarks import verify_samples
from evidence_inputs import sha256
from evidence_processes import require_quiet_processes

ROLES = ('production', 'validation', 'build')
PROGRAM = r'''
import cProfile,hashlib,json,os,sys,time,tracemalloc
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from evidence_inputs import role_hashes
root,role=Path(sys.argv[2]),sys.argv[3]
start=time.perf_counter()
for _ in range(5): hashes=role_hashes(root,role)
elapsed=(time.perf_counter()-start)*1000
tracemalloc.start()
control=bytearray(4096)
if tracemalloc.get_traced_memory()[0]<4096:raise ValueError('Allocation positive control failed')
del control
tracemalloc.reset_peak()
counter=cProfile.Profile();counter.enable();os.stat(root);counter.disable()
if sum(s.callcount for s in counter.getstats() if isinstance(s.code,str) and 'posix.stat' in s.code)!=1:
 raise ValueError('Metadata counter positive control failed')
profile=cProfile.Profile();profile.enable(); measured=role_hashes(root,role);profile.disable()
retained,peak=tracemalloc.get_traced_memory();allocations=sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"));tracemalloc.stop()
if not hashes or measured!=hashes:raise ValueError('Hash snapshot changed during sample')
functions=[]
for entry in profile.getstats():
 code=entry.code
 functions.append(dict(name=code if isinstance(code,str) else code.co_name,calls=entry.callcount,
                      own_seconds=entry.inlinetime,total_seconds=entry.totaltime))
print(json.dumps(dict(milliseconds=elapsed,files=len(hashes),peak_bytes=peak,allocations=allocations,
                     metadata_queries=sum(f["calls"] for f in functions if "posix.stat" in f["name"]),
                     identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest(),
                     functions=functions)))
'''


def verify_experiment(payload):
    verify_samples(payload['rows'], payload['summary'], {'hash-snapshot': list(ROLES)})
    for role in ROLES:
        identities = {row['identity'] for row in payload['rows'] if row['workload'] == role}
        if len(identities) != 1:
            raise ValueError('Hashing experiment digests differ')
    accepted = all(
        next(row['milliseconds'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <
        next(row['milliseconds'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline')
        for number in (1, 2) for role in ROLES)
    less_work = all(
        next(row['metadata_queries'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <
        next(row['metadata_queries'] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline') and
        all(next(row[key] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'changed') <=
            next(row[key] for row in payload['summary'] if row['set'] == number and row['workload'] == role and row['variant'] == 'baseline')
            for key in ('peak_bytes', 'allocations'))
        for number in (1, 2) for role in ROLES)
    if payload.get('work_gate_passed') != less_work:
        raise ValueError('Hashing experiment work acceptance does not match raw samples')
    if payload['latency_gate_passed'] != accepted:
        raise ValueError('Hashing experiment acceptance does not match raw samples')
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
                    result = subprocess.run(argv, check=True, text=True, capture_output=True, timeout=180)
                    row = json.loads(result.stdout)
                    profiles.append(dict(set=number, sample=sample, variant=variant, role=role,
                                         functions=row.pop('functions'), argv=argv, exit=result.returncode))
                    row.update(set=number, sample=sample, variant=variant, harness='hash-snapshot',
                               workload=role, warmup=sample == 0)
                    rows.append(row)
    summary = []
    for number in (1, 2):
        for role in ROLES:
            for variant in variants:
                values = [r for r in rows if r['set'] == number and r['workload'] == role and r['variant'] == variant and not r['warmup']]
                summary.append(dict(set=number, harness='hash-snapshot', workload=role, variant=variant,
                    **{key: statistics.median(r[key] for r in values) for key in ('milliseconds', 'files', 'peak_bytes', 'allocations', 'metadata_queries')}))
    payload = dict(rows=rows, summary=summary, profiles=profiles, code_hashes=code, latency_gate_passed=True)
    payload['latency_gate_passed'] = all(
        next(r['milliseconds'] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'changed') <
        next(r['milliseconds'] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'baseline')
        for number in (1, 2) for role in ROLES)
    payload['work_gate_passed'] = all(
        next(r['metadata_queries'] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'changed') <
        next(r['metadata_queries'] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'baseline') and
        all(next(r[key] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'changed') <=
            next(r[key] for r in summary if r['set'] == number and r['workload'] == role and r['variant'] == 'baseline')
            for key in ('peak_bytes', 'allocations'))
        for number in (1, 2) for role in ROLES)
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
