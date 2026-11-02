from pathlib import Path
import json,shlex,subprocess
root=Path.cwd();folder=root/'build/diagnostic-record-lifecycle-followup';build=root/'build/host'
rows=json.loads((build/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('src/modern/platform.cpp'));base=shlex.split(row['command']);base=base[:base.index('-o')]
line=shlex.split(subprocess.check_output(['ninja','-C',str(build),'-t','commands','codet_tests'],text=True).splitlines()[-1]);link=line[2:line.index('&&',3)]
source=(root/'src/modern/platform.cpp').read_text();header=(root/'include/codet/detail/output_destination.hpp').read_text();records=[]
controls=['own-failure','completion','separator','inspection']
for name in controls:
 out=folder/('negative-'+name);(out/'include/codet/detail').mkdir(parents=True,exist_ok=True);s=source;h=header
 if name=='own-failure':
  s=s.replace('    if (guard.locked && inspection_.epoch == native_epoch)\n      native_destinations.finish(reservation_, success, newline);','    if (guard.locked && inspection_.epoch == native_epoch) {\n      native_destinations.finish(reservation_, success, newline);\n      if (reservation_.presentation && !success) native_destinations.interrupt(native_);\n    }')
  test='NativeRecordLifecycle.FailedPromptDoesNotGenerateIdleRetryEvents'
 elif name=='completion':
  h=h.replace('const auto next = record.presentation ? generation_ : ++generation_;','const auto next = generation_;')
  test='DiagnosticReservations.CompletionPublishesAnEventButPresentationFailureDoesNot'
 elif name=='separator':
  before='''    if (!presentation && io_ && inspection.inspected && io_->line_output)
      (void)io_->line_output->before_output(native, inspection.identity);
'''
  assert before in s
  s=s.replace(before,'')
  s=s.replace('    NativeDiagnosticRecord record(native, inspection, attempt, presentation);','''    if (!presentation && io_ && inspection.inspected && io_->line_output) {
      { NativeMetadataGuard guard; if (guard.locked) (void)native_destinations.separate(native); }
      (void)io_->line_output->before_output(native, inspection.identity);
    }
    NativeDiagnosticRecord record(native, inspection, attempt, presentation);''')
  test='NativeRecordLifecycle.ObserverCannotExposeAnUnreservedSeparatorGap'
 else:
  s=s.replace('const bool current = result.epoch == native_epoch && observation >= owner.observation;','const bool current = true;')
  s=s.replace('      inspection_.epoch = native_epoch;','''      inspection_.epoch = native_epoch;
      if (inspection_.inspected && descriptor_state(native_).known &&
          !detail::same_destination(inspection_.identity, descriptor_state(native_).identity)) return;''')
  test='NativeRecordLifecycle.StaleInspectionCannotSuppressAnEmergencyOnTheCurrentDescriptor'
 (out/'platform.cpp').write_text(s);(out/'include/codet/detail/output_destination.hpp').write_text(h)
 with (out/'build.log').open('w') as log:
  subprocess.run(base[:1]+['-I'+str(out/'include')]+base[1:]+['-c',str(out/'platform.cpp'),'-o',str(out/'platform.o')],cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
  cmd=list(link)
  if name=='completion':
   test_row=next(r for r in rows if r['file'].endswith('tests/diagnostic_lifecycle_test.cpp'));test_args=shlex.split(test_row['command']);test_args=test_args[:test_args.index('-o')]
   subprocess.run(test_args[:1]+['-I'+str(out/'include')]+test_args[1:]+['-c',test_row['file'],'-o',str(out/'test.o')],cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
   cmd=[str(out/'test.o') if x.endswith('tests/diagnostic_lifecycle_test.cpp.o') else x for x in cmd]
  cmd.insert(cmd.index('libcodet_core.a'),str(out/'platform.o'));cmd[cmd.index('-o')+1]=str(out/'tests')
  subprocess.run(cmd,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 r=subprocess.run([str(out/'tests'),'--gtest_filter='+test],capture_output=True,text=True,timeout=20);(out/'result.log').write_text(r.stdout+r.stderr)
 assert r.returncode!=0 and '[  FAILED  ]' in r.stdout,(name,r.returncode,r.stdout,r.stderr)
 records.append(dict(name=name,test=test,exit=r.returncode,output=r.stdout+r.stderr));print(name,r.returncode,flush=True)
(folder/'negative-controls.json').write_text(json.dumps(records,indent=2)+'\n')
