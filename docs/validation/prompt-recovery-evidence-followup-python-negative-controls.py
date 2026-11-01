import json,subprocess
from pathlib import Path
root=Path.cwd();folder=root/'build/prompt-recovery-evidence-followup';source=(root/'scripts/validation_evidence.py').read_text();results=[]
for name,mutant,test in [
 ('unconditional-status',source.replace('if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):','if False:'),'test_nonzero_cannot_be_overridden_by_success_log'),
 ('unconditional-equivalence',source.replace('equivalent = expected == current','equivalent = True'),'test_changed_benchmark_sources_are_explicit'),
 ('process-substrings',source.replace('if executable in ("ctest", "codet_tests", "clang-tidy", "cppcheck", "ninja"):','if "ctest" in repr(process) or executable in ("ctest", "codet_tests", "clang-tidy", "cppcheck", "ninja"):'),'test_executables_and_exact_script_arguments')]:
 path=folder/(name+'.py');path.write_text(mutant)
 owner='ProcessInspection' if name=='process-substrings' else 'ValidationEvidence'
 runner="import importlib.util,sys,unittest;sys.path.insert(0,'tests');s=importlib.util.spec_from_file_location('validation_evidence',sys.argv[1]);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);sys.modules['validation_evidence']=m;import test_validation_evidence;t=unittest.defaultTestLoader.loadTestsFromName('test_validation_evidence.'+sys.argv[2]);r=unittest.TextTestRunner(verbosity=2).run(t);raise SystemExit(not r.wasSuccessful())"
 r=subprocess.run(['python3','-c',runner,str(path),owner+'.'+test],capture_output=True,text=True);(folder/(name+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode!=1:raise RuntimeError((name,r.returncode,r.stdout,r.stderr))
 results.append(dict(name=name,test=owner+'.'+test,exit=r.returncode))
(folder/'python-negative-controls.json').write_text(json.dumps(results,indent=2)+'\n');print(results)
