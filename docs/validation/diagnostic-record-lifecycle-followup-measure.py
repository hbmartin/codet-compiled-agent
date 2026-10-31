import csv,json,statistics,subprocess,time
from pathlib import Path
root=Path.cwd();folder=root/'build/diagnostic-record-lifecycle-followup';rows=[]
print('Waiting for validation, analyzers and Release builds to finish',flush=True)
variants=['baseline','correctness','optimized']
cases={
 'line': ['allocation-positive','geometry-positive','busy','native-busy','editing','editing-basic','editing-patterned','native-editing','deletion','unchanged','deferred-tail','native-boundary','native-flush-boundary','native-rebind','native-unchanged','warning-empty','warning-new','warning-failed-idle'],
 'output': ['write-positive','stat-positive','flags-positive','bounds-positive','diagnostics-amplified','diagnostics-standalone-amplified','diagnostics-pipe-amplified','budget-full','jsonl-small','pipe-jsonl-small','editor'],
 'export': ['small','large']}
started=time.monotonic()
processes=subprocess.check_output(['ps','-axo','command'],text=True)
assert not any('scripts/analyze-modern.py' in line or 'ctest --test-dir' in line or 'codet_tests --gtest' in line for line in processes.splitlines()),processes[-6000:]
for number in (1,2):
 for harness,workloads in cases.items():
  for case in workloads:
   for sample in range(6):
    order = variants[(number+sample)%3:] + variants[:(number+sample)%3]
    for variant in (order if sample%2 else order[::-1]):
     build=folder/(variant+'-release');report=folder/'measurement.json'
     executable={'line':'codet_line_input_bench','ui':'codet_ui_render_bench','output':'codet_output_bench','export':'codet_export_bench'}[harness]
     command=[str(build/executable),case,str(report)]+(['file'] if harness=='export' else [])
     result=subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,timeout=180)
     if result.returncode:raise RuntimeError((command,result.returncode,result.stderr.decode(errors='replace')))
     row=json.loads(report.read_text());row.update(set=number,sample=sample,variant=variant,harness=harness,workload=case,warmup=sample==0)
     if harness=='line':
      if case=='allocation-positive':assert row['allocations']==1000 and row['allocated_bytes']==32000,row
      if case=='geometry-positive':assert row['geometry_queries']==1000,row
      if variant!='baseline' and case in ('busy','native-busy','unchanged'):
       assert row['allocations']==row['allocated_bytes']==row['prepared_input_bytes']==0,row
       assert row['bytes']<=row['iterations']*256,row
      if case in ('unchanged','native-unchanged'):assert row['bytes']==row['output_operations']==row['geometry_queries']==0,row
      if case in ('editing','editing-basic','editing-patterned','native-editing'):assert row['prepared_input_bytes']<=40960,row
      if case=='warning-empty':assert row['warning_attempts']==row['allocations']==row['output_operations']==0,row
      if case=='warning-failed-idle' and variant!='baseline':assert row['warning_attempts']==1 and row['allocations']==0,row
     if harness=='ui':
      if case=='width-positive':assert row['locale_conversions']==128,row
      if case=='allocation-positive':assert row['allocations']==1 and row['allocated_bytes']==4096,row
      if case=='input-narrow':assert row['locale_conversions']==0,row
      if case.startswith('presentation-'):assert row['history_budget']==262144 and row['retained_payload_bytes']<=262144,row
     if harness=='output' and case.endswith('-positive'):
      assert row[{'write-positive':'writes','stat-positive':'fstat','flags-positive':'fcntl','bounds-positive':'fpathconf'}[case]]==128,row
     rows.append(row)
    (folder/'final-measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
   print('Measured',number,harness,case,round(time.monotonic()-started,1),flush=True)
summary=[]
for number in (1,2):
 for harness,workloads in cases.items():
  for case in workloads:
   for variant in variants:
    samples=[r for r in rows if r['set']==number and r['harness']==harness and r['workload']==case and r['variant']==variant and not r['warmup']]
    metrics=[key for key in samples[0] if isinstance(samples[0][key],(int,float)) and not isinstance(samples[0][key],bool) and key not in ('set','sample')]
    summary.append(dict(set=number,harness=harness,workload=case,variant=variant,**{key:statistics.median(r[key] for r in samples) for key in metrics}))
(folder/'final-performance-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for filename,records in [('final-performance.csv',summary),('final-performance-samples.csv',rows)]:
 with (folder/filename).open('w',newline='') as output:
  fields=sorted(set().union(*(r.keys() for r in records)));writer=csv.DictWriter(output,fieldnames=fields,lineterminator='\n');writer.writeheader();writer.writerows(records)
print('Measurement complete',round(time.monotonic()-started,1),flush=True)
