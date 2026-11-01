import csv,hashlib,json,statistics,subprocess,sys,time
from pathlib import Path
root=Path.cwd();folder=root/'build/prompt-recovery-evidence-followup';sys.path.insert(0,str(root/'scripts'))
from validation_evidence import require_quiet_processes,production_hashes
require_quiet_processes()
files=['tests/line_input_bench.cpp','tests/output_bench.cpp','tests/export_bench.cpp','tests/descriptor_bench_metrics.hpp','tests/native_stat_interposer.hpp','tests/native_symbol.hpp','tests/temp.hpp']
provenance={}
for variant,source in [('baseline',folder/'baseline-src'),('changed',root)]:
 provenance[variant]={name:hashlib.sha256((source/name).read_bytes()).hexdigest() for name in files}
assert provenance['baseline']==provenance['changed']
(folder/'benchmark-provenance.json').write_text(json.dumps(dict(baseline='ef2155c7be251ae37d13ead8c5d0a34f37dbc3e2',changed_ref=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),production_hashes=production_hashes(root),harness_hashes=provenance),indent=2)+'\n')
cases={'line':['allocation-positive','geometry-positive','busy','native-busy','editing','editing-basic','native-editing','deletion','unchanged','deferred-tail','native-boundary','native-flush-boundary','native-unchanged'],'output':['write-positive','stat-positive','flags-positive','bounds-positive','diagnostics-amplified','diagnostics-standalone-amplified','diagnostics-pipe-amplified'],'export':['small','large']}
rows=[];started=time.monotonic()
for number in [1,2]:
 for harness,workloads in cases.items():
  for case in workloads:
   for sample in range(6):
    variants=['baseline','changed']
    if (number+sample)%2:variants.reverse()
    for variant in variants:
     executable={'line':'codet_line_input_bench','output':'codet_output_bench','export':'codet_export_bench'}[harness]
     command=[str(folder/(variant+'-release')/executable),case,str(folder/'measurement.json')]+(['file'] if harness=='export' else [])
     p=subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,timeout=180)
     if p.returncode:raise RuntimeError((command,p.returncode,p.stderr.decode(errors='replace')))
     row=json.loads((folder/'measurement.json').read_text());row.update(set=number,sample=sample,variant=variant,harness=harness,workload=case,warmup=sample==0)
     if harness=='line':
      if case=='allocation-positive':assert row['allocations']==1000 and row['allocated_bytes']==32000,row
      if case=='geometry-positive':assert row['geometry_queries']==1000,row
      if case in ['unchanged','native-unchanged']:assert row['bytes']==row['output_operations']==row['geometry_queries']==row['allocations']==0,row
     if harness=='output' and case.endswith('-positive'):
      assert row[{'write-positive':'writes','stat-positive':'fstat','flags-positive':'fcntl','bounds-positive':'fpathconf'}[case]]==128,row
     rows.append(row)
   print('Measured',number,harness,case,round(time.monotonic()-started,1),flush=True)
summary=[]
for number in [1,2]:
 for harness,workloads in cases.items():
  for case in workloads:
   for variant in ['baseline','changed']:
    values=[r for r in rows if r['set']==number and r['harness']==harness and r['workload']==case and r['variant']==variant and not r['warmup']]
    metrics=[k for k in values[0] if type(values[0][k]) in (int,float) and k not in ['set','sample']]
    summary.append(dict(set=number,harness=harness,workload=case,variant=variant,**{k:statistics.median(r[k] for r in values) for k in metrics}))
for filename,records in [('performance-samples.csv',rows),('performance.csv',summary)]:
 with (folder/filename).open('w',newline='') as out:
  writer=csv.DictWriter(out,fieldnames=sorted(set().union(*(r.keys() for r in records))),lineterminator='\n');writer.writeheader();writer.writerows(records)
(folder/'performance.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Finished',len(rows),'samples',round(time.monotonic()-started,1),'seconds')
