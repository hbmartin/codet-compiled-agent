"""Assemble derived evidence only after the source-bound publisher accepts each capture."""
from pathlib import Path
import json,shutil,subprocess,sys
root=Path.cwd();folder=root/'build/prompt-recovery-evidence-followup';destination=root/'docs/validation';prefix='prompt-recovery-evidence-followup';sys.path.insert(0,str(root/'scripts'))
from validation_evidence import publish,atomic_json,production_hashes
benchmark=json.loads((folder/'benchmark-provenance.json').read_text())
report=publish(folder/'manifest.json',folder/'checks',folder/'accepted.json',benchmark)
report['baseline']='ef2155c7be251ae37d13ead8c5d0a34f37dbc3e2'
report['release_variants']=json.loads((folder/'variants.json').read_text())
report['runtime_ref']=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
report['supplemental']=publish(folder/'supplemental-manifest.json',folder/'supplemental-checks',folder/'supplemental-evidence.json')
report['baseline_analysis']=publish(folder/'baseline-analysis-manifest.json',folder/'baseline-analysis-checks',folder/'baseline-analysis-evidence.json')
report['static_comparison']=json.loads((folder/'static-comparison.json').read_text())
report['negative_controls']=json.loads((folder/'negative-controls.json').read_text())+json.loads((folder/'python-negative-controls.json').read_text())
report['performance']=dict(method='Identical Release flags and seven byte-identical harness/support sources. One warm-up plus five alternating measured runs, two sets, with no concurrent local tests or analyzers.',samples=528,measured_samples=440,workloads=22,comparisons=json.loads((folder/'benchmark-comparison.json').read_text()),provenance=benchmark,optimization_retained=False,timing_caveat='Small-export medians increased approximately42–49% in these two sets. Allocations, allocated bytes, and measured output/metadata work matched. Other timings were mixed; no general speedup or causal timing claim is made.')
report['restricted_attempt']=[json.loads(p.read_text())for p in sorted((folder/'restricted-attempt').glob('*.json'))if p.name!='run.json']
report['restricted_attempt_note']='Local-server binding was denied by the execution sandbox. The interrupted capture is retained; the fresh unrestricted capture records the complete passing required run.'
report['ubuntu']=json.loads((folder/'ubuntu.json').read_text())
report['limitations']=['Local LLVM23.1.3 reports the same150 Clang-Tidy findings and one core.DivideZero analyzer finding as fetched main. Their actual nonzero outcomes remain observations; no check or baseline assertion was weakened.','Native PPC/Tiger compiler, SDK and runtime are unavailable. Host cross-selection build/link/runtime success is not native Tiger verification.','The blocking-TTY cancellation limitation and best-effort emergency delivery remain. Partial duplicates and conservative separators are accepted.','Native metadata reservations do not serialize complete concurrent byte streams; actual injected I/O and rdbuf replacement remain caller-synchronized.']
for source,suffix in [('performance.csv','performance.csv'),('performance-samples.csv','samples.csv'),('measure.py','measure.py'),('negative-controls.py','negative-controls.py'),('python-negative-controls.py','python-negative-controls.py'),('compare-static.py','compare-static.py'),('verify-benchmarks.py','verify-benchmarks.py'),('manifest.json','manifest.json')]:
 shutil.copyfile(folder/source,destination/(prefix+'-'+suffix))
assert report['production_hashes']==production_hashes(root)
atomic_json(destination/(prefix+'.json'),report)
print('Published',prefix,'from captured outcomes')
