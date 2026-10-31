# Count metadata guards in separate objects; never use instrumented timings as measurements.
from pathlib import Path
import json,shlex,subprocess
root=Path.cwd();folder=root/'build/diagnostic-record-lifecycle-followup';records=[]
for variant in ['baseline','correctness','optimized']:
 build=folder/(variant+'-release');src=folder/(variant+'-src');out=folder/(variant+'-locks');out.mkdir(exist_ok=True)
 rows=json.loads((build/'compile_commands.json').read_text())
 def args(file):
  row=next(r for r in rows if r['file'].endswith(file));a=shlex.split(row['command']);return a[:a.index('-o')]
 s=(src/'src/modern/platform.cpp').read_text().replace('pthread_mutex_t native_metadata_mutex','std::atomic<size_t> native_lock_count{0};\npthread_mutex_t native_metadata_mutex').replace('bool locked = ::pthread_mutex_lock(&native_metadata_mutex) == 0;', 'bool locked = (++native_lock_count, ::pthread_mutex_lock(&native_metadata_mutex) == 0);')
 s+='\nnamespace codet::detail { size_t measured_locks(bool reset) { return reset ? native_lock_count.exchange(0) : native_lock_count.load(); } }\n'
 (out/'platform.cpp').write_text(s)
 commands=[args('src/modern/platform.cpp')+['-c',str(out/'platform.cpp'),'-o',str(out/'platform.o')]]
 s=(src/'tests/line_input_bench.cpp').read_text().replace('namespace {\nbool observing','namespace codet::detail { size_t measured_locks(bool); }\nnamespace {\nbool observing').replace('  observing = test::descriptor_bench::observing = true;', '  (void)detail::measured_locks(true);\n  observing = test::descriptor_bench::observing = true;').replace('    } else if (mode == "allocation-positive") {','    } else if (mode == "lock-positive") {\n      detail::reset_standard_diagnostics();\n    } else if (mode == "allocation-positive") {').replace('{{"case", mode},','{{"case", mode}, {"locks",detail::measured_locks(false)},')
 (out/'line.cpp').write_text(s)
 link=shlex.split(subprocess.check_output(['ninja','-C',str(build),'-t','commands','codet_line_input_bench'],text=True).splitlines()[-1]);libs=link[link.index('libcodet_core.a'):link.index('&&',2)]
 commands.append(args('tests/line_input_bench.cpp')+['-I'+str(src/'tests'),str(out/'line.cpp'),str(out/'platform.o')]+libs+['-o',str(out/'line')])
 with (out/'build.log').open('w') as log:
  for command in commands:subprocess.run(command,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 for mode in ['lock-positive','editing','editing-basic','native-editing','native-busy','native-flush-boundary','native-boundary','native-rebind','native-unchanged','warning-empty']:
  report=out/'report.json';subprocess.run([str(out/'line'),mode,str(report)],check=True);r=json.loads(report.read_text());r.update(variant=variant);records.append(r)
  if mode=='lock-positive':assert r['locks']==1000,r
  print(variant,mode,r['locks'],flush=True)
(folder/'lock-counts.json').write_text(json.dumps(records,indent=2)+'\n')
