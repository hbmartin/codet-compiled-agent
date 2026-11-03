# Reproduce the archived, identical-harness Release comparison. Run after fetching pinned deps.
import io,json,hashlib,subprocess,tarfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
prefix='diagnostic-record-lifecycle-followup'
evidence=json.loads((root/'docs/validation'/(prefix+'.json')).read_text())
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--folder',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
folder=args.folder.resolve()
folder.mkdir(parents=True)
if args.output.exists():raise ValueError('Output already exists')
for variant in evidence['performance']['variants']:
 name=variant['name'];src=folder/(name+'-src')
 if src.exists():raise SystemExit('Use a fresh build folder: '+str(src))
 src.mkdir()
 with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','archive',variant['ref']],cwd=root,timeout=30))) as archive:
  archive.extractall(src,filter='data')
 if name=='optimized':
  patch=(root/'docs/validation'/(prefix+'-omitted-optimization.patch')).read_text()
  lines=[]
  for line in patch.splitlines():
   for marker in ['correctness-src/','optimized-src/']:
    if marker in line and line.startswith(('diff --git','--- ','+++ ')):
     import re
     line=re.sub(r'[ab]/[^ ]*?'+marker,lambda m:m[0][0]+'/',line)
   lines.append(line)
  subprocess.run(['patch','-p1'],input='\n'.join(lines)+'\n',text=True,cwd=src,check=True,timeout=30)
 for name,expected in evidence['performance']['harness_hashes']['correctness'].items():
  content=(root/name).read_bytes()
  if hashlib.sha256(content).hexdigest()!=expected:raise SystemExit('Harness changed: '+name)
  (src/name).write_bytes(content)
 command=list(variant['configure'])
 command[command.index('-S')+1]=str(src)
 command[command.index('-B')+1]=str(folder/(variant['name']+'-release'))
 command=[('-DCODET_DEPENDENCY_ROOT='+str(root/'.deps')) if x.startswith('-DCODET_DEPENDENCY_ROOT=') else x for x in command]
 subprocess.run(command,cwd=root,check=True,timeout=600)
 subprocess.run(['cmake','--build',str(folder/(variant['name']+'-release')),'--target','codet_line_input_bench','codet_output_bench','codet_export_bench','-j4'],cwd=root,check=True,timeout=600)
subprocess.run(['python3',str(root/'docs/validation'/(prefix+'-measure.py')),'--folder',str(folder),'--output',str(args.output)],cwd=root,check=True,timeout=1800)
