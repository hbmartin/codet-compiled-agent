# Count guards in an instrumented copy; never use these builds for latency comparisons.
from pathlib import Path
import subprocess,tarfile,io,json,hashlib
root=Path.cwd();folder=root/'build/prompt-boundary-ownership-followup';folder.mkdir(parents=True,exist_ok=True);source=folder/'lock-count-src';source.mkdir(exist_ok=True)
evidence=json.loads((root/'docs/validation/prompt-boundary-ownership-followup.json').read_text());ref=evidence['runtime']
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','archive',ref]))) as tar:tar.extractall(source,filter='data')
p=source/'src/modern/platform.cpp';s=p.read_text().replace('pthread_mutex_t native_metadata_mutex', 'std::atomic<size_t> native_lock_count{0};\npthread_mutex_t native_metadata_mutex').replace('bool locked = ::pthread_mutex_lock(&native_metadata_mutex) == 0;', 'bool locked = (++native_lock_count, ::pthread_mutex_lock(&native_metadata_mutex) == 0);');s+='\nnamespace codet::detail {\nsize_t benchmark_native_locks(bool reset) {\n return reset ? native_lock_count.exchange(0) : native_lock_count.load();\n}\n}\n';p.write_text(s)
p=source/'tests/line_input_bench.cpp';s=p.read_text().replace('namespace {\nbool observing', 'namespace codet::detail { size_t benchmark_native_locks(bool); }\nnamespace {\nbool observing');s=s.replace('  observing = test::descriptor_bench::observing = true;', '  (void)detail::benchmark_native_locks(true);\n  observing = test::descriptor_bench::observing = true;');s=s.replace('    } else if (mode == "allocation-positive") {', '    } else if (mode == "metadata-lock-positive") {\n      detail::reset_standard_diagnostics();\n    } else if (mode == "allocation-positive") {');s=s.replace('{{"case", mode},','{{"case", mode}, {"metadata_lock_acquisitions",detail::benchmark_native_locks(false)},');p.write_text(s)
row=evidence['performance']['variants'][1];command=[('-DCODET_DEPENDENCY_ROOT='+str(root/'.deps')) if x.startswith('-DCODET_DEPENDENCY_ROOT=') else x for x in row['configure']];command[command.index('-S')+1]=str(source);command[command.index('-B')+1]=str(folder/'lock-count-release')
with (folder/'lock-count-configure.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
with (folder/'lock-count-build.log').open('w') as log:subprocess.run(['cmake','--build',str(folder/'lock-count-release'),'--target','codet_line_input_bench','-j4'],stdout=log,stderr=subprocess.STDOUT,check=True)
rows=[]
for mode in ['metadata-lock-positive','editing','editing-basic','native-editing','native-busy','native-boundary','native-flush-boundary','native-rebind','native-unchanged','unchanged','warning-empty']:
 report=folder/'lock-count.json';subprocess.run([str(folder/'lock-count-release/codet_line_input_bench'),mode,str(report)],check=True);value=json.loads(report.read_text());rows.append(value)
 if mode=='metadata-lock-positive':assert value['metadata_lock_acquisitions']==1000,value
 print(mode,value['metadata_lock_acquisitions'],flush=True)
(folder/'lock-counts.json').write_text(json.dumps(dict(ref=ref,method='Separate instrumented copy counts each NativeMetadataGuard acquisition; timings from this build are not used.',instrumented_hashes={n:hashlib.sha256((source/n).read_bytes()).hexdigest() for n in ['src/modern/platform.cpp','tests/line_input_bench.cpp']},rows=rows),indent=2)+'\n')
