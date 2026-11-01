import importlib.util,json,shlex,subprocess,sys
from pathlib import Path
root=Path.cwd();folder=root/'build/prompt-recovery-evidence-followup';build=root/'build/host'
db=json.loads((build/'compile_commands.json').read_text())
commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands','codet_tests'],text=True).splitlines()[-1]
link=shlex.split(commands);link=link[2:link.index('&&',2)]
results=[]
def invoke(name,source,entryfile,replaced,filter):
 p=folder/('negative-'+name);p.mkdir(exist_ok=True)
 entry=next(r for r in db if r['file'].endswith(entryfile));cmd=shlex.split(entry['command']);i=cmd.index('-o');cmd[i+1]=str(p/'mutant.o');cmd[-1]=str(source)
 if name=='deferred':cmd.insert(1,'-I'+str(p/'include'))
 r=subprocess.run(cmd,cwd=entry['directory'],capture_output=True,text=True);(p/'compile.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError((name,r.stderr))
 cmd=[x for x in link if x!=replaced];cmd.insert(1,str(p/'mutant.o'));cmd[cmd.index('-o')+1]=str(p/'tests')
 r=subprocess.run(cmd,cwd=build,capture_output=True,text=True);(p/'link.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError((name,r.stderr))
 r=subprocess.run([str(p/'tests'),'--gtest_filter='+filter],capture_output=True,text=True,timeout=30);(p/'result.log').write_text(r.stdout+r.stderr)
 if r.returncode!=1:raise RuntimeError((name,r.returncode,r.stdout,r.stderr))
 results.append(dict(name=name,filter=filter,exit=r.returncode,tail=r.stdout[-2200:]))
p=folder/'negative-deferred/include/codet/detail';p.mkdir(parents=True,exist_ok=True)
(p/'line_prompt.hpp').write_bytes(subprocess.check_output(['git','show','ef2155c:include/codet/detail/line_prompt.hpp']))
invoke('deferred',root/'tests/diagnostic_lifecycle_test.cpp','/tests/diagnostic_lifecycle_test.cpp','CMakeFiles/codet_tests.dir/tests/diagnostic_lifecycle_test.cpp.o','NativeRecordLifecycle.DeferredEraseRetainsWorkUntilDiagnosticCompletion')
p=folder/'negative-separator';p.mkdir(exist_ok=True);source=p/'platform.cpp';source.write_text((root/'src/modern/platform.cpp').read_text().replace('record.request_separator(io_->line_output->before_output(native, inspection.identity));','(void)io_->line_output->before_output(native, inspection.identity);'))
invoke('separator',source,'/src/modern/platform.cpp',None,'NativeRecordLifecycle.InjectedStdoutSharesSeparatorWithNativeStderr')
p=folder/'negative-printer';p.mkdir(exist_ok=True);source=p/'fixture.cpp';s=(root/'tests/diagnostic_lifecycle_test.cpp').read_text().replace('(void)::dup2(out, STDOUT_FILENO);','(void)out;').replace('(void)::dup2(error, STDERR_FILENO);','(void)error;');source.write_text(s)
# Original relative test headers still resolve in the repository tests directory.
entry=next(r for r in db if r['file'].endswith('/tests/diagnostic_lifecycle_test.cpp'));entry['command']=entry['command'].replace(' -o ',' -I'+str(root/'tests')+' -o ')
invoke('printer',source,'/tests/diagnostic_lifecycle_test.cpp','CMakeFiles/codet_tests.dir/tests/diagnostic_lifecycle_test.cpp.o','DiagnosticFixture.FailureDetailsReachParentLogAndDoNotContaminatePty')
(folder/'negative-controls.json').write_text(json.dumps(results,indent=2)+'\n')
print([(r['name'],r['exit']) for r in results])
