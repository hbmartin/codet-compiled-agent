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
        ("process", "evidence_processes", 'if matches_native(executable) or (argv and matches_native(argv[0])) or script_identity(executable):',
         'if "ctest" in repr(process) or matches_native(executable) or (argv and matches_native(argv[0])) or script_identity(executable):', "ProcessInspection.test_executables_and_exact_script_arguments"),
        ("validation-inputs", "validation_evidence", 'record.get("validation_before") != validation or record.get("validation_after") != validation',
         'False', "ProvenanceGuards.test_validation_changes_invalidate_capture"),
        ("raw-medians", "evidence_benchmarks", 'if summaries[key] != row:',
         'if False:', "ComparisonGuards.test_raw_samples_define_medians_and_counts"),
        ("cppcheck-location", "evidence_analysis", 'return finding["id"], finding["msg"], locations',
         'return finding["id"],', "ComparisonGuards.test_cppcheck_locations_and_msg_are_preserved"),
        ("build-consumer", "validation_evidence", 'if not (root / output["path"]).is_file() or digest(root / output["path"]) != output["sha256"]:',
         'if False:', "ReplayIntegrity.test_consumer_rejects_replaced_prerequisite_executable"),
        ("capture-extras", "evidence_bundle", 'shutil.copyfile(folder / "run.json", stage / "capture/run.json")',
         'shutil.copytree(folder, stage / "capture", dirs_exist_ok=True)', "ReplayIntegrity.test_archive_excludes_stale_records_and_survives_relocation"),
        ("versioned-process", "evidence_processes", 'name in NATIVE_BLOCKERS or re.sub(r"-\\d+(?:\\.\\d+)*$", "", name) in NATIVE_BLOCKERS or',
         'name in NATIVE_BLOCKERS or False or', "ReplayIntegrity.test_process_gate_matches_versioned_and_truncated_names"),
        ("git-source-key", "evidence_inputs", 'if actual != expected:',
         'if False:', "ReplayIntegrity.test_source_commit_binding_rejects_wrong_baseline"),
        ("cwd-relationship", "evidence_paths", 'if record.get("cwd") != cwd or record.get("canonical_cwd") != cwd:',
         'if False:', "EvidenceV4.test_v5_audit_rejects_jointly_changed_cwd_fields"),
        ("relocation-identity", "evidence_analysis", 'relocation_identity(tool, before) != relocation_identity(tool, after) or',
         'False or', "EvidenceV4.test_v5_relocations_cannot_substitute_another_finding"),
        ("ci-head", "evidence_ci", 'run.get("head_sha") != head or',
         'False or', "PortableV5.test_public_ci_pagination_and_identity"),
        ("dependency-consumer", "validation_evidence", 'record.get("external_inputs_after") != record["external_inputs_before"]',
         'False', "EvidenceV4.test_v5_dependency_mutation_is_retained_even_if_restored"),
        ("io-classification", "validation_evidence", '"launch-error" if phase == "launch" else ("execution-error" if phase == "execution" else "artifact-error")',
         '"launch-error"', "ReplayIntegrity.test_input_read_failure_is_not_a_launch_failure"),
        ("captured-identity", "validation_evidence", 'declared = context.pop("declared_artifacts", None)',
         'declared = context.pop("declared_artifacts", None)\n    context.pop("capture_run_id", None)', "ReplayIntegrity.test_every_compatibility_wrapper_preserves_captured_output"),
        ("sidecar-identity", "evidence_bundle", 'relative = "measurements/" + name + "/" + Path(artifact["path"]).name + "/" + filename',
         'relative = "measurements/" + name + "/" + Path(artifact["path"]).stem + "/" + filename', "ReplayIntegrity.test_measurement_artifacts_have_distinct_complete_sidecars"),
        ("explicit-environment", "evidence_inputs", 'env = dict(manifest.get("environment", {"PATH": os.defpath}))',
         'env = dict(os.environ); env.update(manifest.get("environment", {"PATH": os.defpath}))', "EvidenceV4.test_environment_is_exact_and_does_not_inherit_shell"),
        ("argv-container", "validation_evidence", 'not isinstance(check["argv"], list) or ',
         '', "EvidenceV4.test_string_vector_is_rejected_at_load"),
        ("empty-analyzer-version", "evidence_analysis", 'not manifest["versions"] or ',
         '', "EvidenceV4.test_analyzers_require_versions_and_their_own_source_sets"),
        ("relocation-context", "evidence_analysis", 'diagnostic_context(tool, before, baseline_root, entry["context"]) != entry["context"] or\n                diagnostic_context(tool, after, changed_root, entry["context"]) != entry["context"] or',
         'False or', "EvidenceV4.test_relocations_require_full_identity_and_real_unchanged_source_context"),
    ]
    results = []
    for name, module, before, after, target in cases:
        print("Python control:", name, flush=True)
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
        try:
            verify_unittest_result(*runs)
        except ValueError as error:
            raise ValueError("Control " + name + ": " + repr(runs)) from error
        results.append(dict(name=name, test=target, baseline=runs[0], mutant=runs[1]))
    return results
