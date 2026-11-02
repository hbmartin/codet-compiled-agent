"""Fresh-process, assertion-classified negative controls for evidence guards."""

import json
import subprocess
import sys

from evidence_benchmarks import replace_once, verify_unittest_result

RUNNER = r'''
import importlib.util,json,sys,unittest
from pathlib import Path
root,module_path,module_name,test_name,output=sys.argv[1:]
sys.path[:0]=[str(Path(root)/'scripts'),str(Path(root)/'tests')]
spec=importlib.util.spec_from_file_location(module_name,module_path)
module=importlib.util.module_from_spec(spec);sys.modules[module_name]=module;spec.loader.exec_module(module)
class Result(unittest.TextTestResult):
    assertion_failure=False
    def addFailure(self,test,error):
        self.assertion_failure=self.assertion_failure or issubclass(error[0],AssertionError)
        super().addFailure(test,error)
    def addSubTest(self,test,subtest,error):
        if error is not None:
            self.assertion_failure=self.assertion_failure or issubclass(error[0],AssertionError)
        super().addSubTest(test,subtest,error)
result=unittest.TextTestRunner(verbosity=2,resultclass=Result).run(unittest.defaultTestLoader.loadTestsFromName(test_name))
exit_code=int(not result.wasSuccessful())
Path(output).write_text(json.dumps(dict(tests_run=result.testsRun,errors=len(result.errors),skipped=len(result.skipped),
 failures=len(result.failures),assertion_failure=result.assertion_failure,expected_failures=len(result.expectedFailures),
 unexpected_successes=len(result.unexpectedSuccesses),exit=exit_code)))
raise SystemExit(exit_code)
'''


def run_controls(root, folder):
    cases = [
        ("status", "validation_evidence", 'if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):',
         'if False:', "ValidationEvidence.test_nonzero_cannot_be_overridden_by_success_log"),
        ("equivalence", "validation_evidence", "equivalent = benchmark[\"production_hashes\"] == value[\"production_hashes\"]",
         "equivalent = True", "ValidationEvidence.test_changed_benchmark_sources_are_explicit"),
        ("process", "evidence_processes", "if executable in NATIVE_BLOCKERS:",
         'if "ctest" in repr(process) or executable in NATIVE_BLOCKERS:', "ProcessInspection.test_executables_and_exact_script_arguments"),
        ("validation-inputs", "validation_evidence", 'record.get("validation_before") != validation or record.get("validation_after") != validation',
         'False', "ProvenanceGuards.test_validation_changes_invalidate_capture"),
        ("raw-medians", "evidence_benchmarks", 'if summaries[key] != row:',
         'if False:', "ComparisonGuards.test_raw_samples_define_medians_and_counts"),
        ("cppcheck-location", "evidence_analysis", 'return finding["id"], finding["msg"], locations',
         'return finding["id"],', "ComparisonGuards.test_cppcheck_locations_and_msg_are_preserved"),
    ]
    results = []
    for name, module, before, after, target in cases:
        directory = folder / ("control-" + name)
        directory.mkdir(exist_ok=True)
        source = root / "scripts" / (module + ".py")
        mutant = directory / (module + ".py")
        mutant.write_text(replace_once(source.read_text(), before, after))
        runs = []
        for label, path in (("baseline", source), ("mutant", mutant)):
            output = directory / (label + ".json")
            output.unlink(missing_ok=True)
            argv = [sys.executable, "-c", RUNNER, str(root), str(path), module,
                    "test_validation_evidence." + target, str(output)]
            process = subprocess.run(argv, capture_output=True, text=True, timeout=30)
            value = json.loads(output.read_text())
            if value["exit"] != process.returncode:
                raise ValueError("Control exit metadata mismatch")
            value.update(argv=argv, log=process.stdout + process.stderr)
            runs.append(value)
        verify_unittest_result(*runs)
        results.append(dict(name=name, test=target, baseline=runs[0], mutant=runs[1]))
    return results
