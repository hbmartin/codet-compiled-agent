import csv,json
from pathlib import Path
f=Path('build/prompt-recovery-evidence-followup');rows=list(csv.DictReader((f/'performance-samples.csv').open()));summary=json.loads((f/'performance.json').read_text())
assert len(rows)==528
comparisons=[]
for number in [1,2]:
 for case in sorted({r['workload'] for r in summary}):
  a=next(r for r in summary if r['set']==number and r['workload']==case and r['variant']=='baseline');b=next(r for r in summary if r['set']==number and r['workload']==case and r['variant']=='changed')
  different={k:[a[k],b[k]]for k in a if k not in ['variant','milliseconds']and a[k]!=b[k]}
  assert a['allocations']==b['allocations'] and a['allocated_bytes']==b['allocated_bytes'],(case,different)
  comparisons.append(dict(set=number,workload=case,baseline_ms=a['milliseconds'],changed_ms=b['milliseconds'],percent=100*(b['milliseconds']/a['milliseconds']-1),work_differences=different))
(f/'benchmark-comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n')
print('528 samples; unchanged allocation counts/bytes; all work differences:',[(r['workload'],r['work_differences'])for r in comparisons if r['work_differences']])
