from pathlib import Path
from collections import Counter
import json
root=Path.cwd();folder=root/'build/prompt-recovery-evidence-followup';summary={}
for tool in ['clang-tidy','cppcheck','clang-analyzer']:
 variants={}
 for name,source,build in [('baseline',folder/'baseline-src',folder/'baseline-release'),('changed',root,root/'build/host')]:
  manifest=json.loads((build/'analysis'/tool/'manifest.json').read_text());values=[]
  for result in manifest['results']:
   for finding in result.get('findings',[]):
    if tool=='clang-tidy':
     message=finding['DiagnosticMessage'];filename=message['FilePath']
     for prefix in [str(source)+'/',str(root)+'/']:
      if filename.startswith(prefix):filename=filename[len(prefix):];break
     values.append((finding['DiagnosticName'],filename,message['Message']))
    elif tool=='clang-analyzer':
     values.append((finding['check_name'],finding['description'],finding.get('issue_context'),finding.get('issue_hash_content_of_line_in_context')))
    else:values.append((finding.get('id'),finding.get('message')))
  variants[name]=dict(count=manifest['findings'],passed=manifest['passed'],findings=values,expected_sources=len(manifest['expected_sources']),completed_sources=len(manifest['completed_sources']))
 summary[tool]=dict(**variants,equal=Counter(map(tuple,variants['baseline']['findings']))==Counter(map(tuple,variants['changed']['findings'])))
assert all(v['equal'] for v in summary.values()),summary
(folder/'static-comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
print([(name,v['baseline']['count'],v['changed']['count'],v['equal'])for name,v in summary.items()])
