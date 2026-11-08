"""Fresh-process, assertion-classified negative controls for evidence guards."""

import json
import sys

from evidence_benchmarks import replace_once, verify_unittest_result
from process_supervision import run as supervised_run

RUNNER = r'''
import importlib.util,json,sys,unittest
from pathlib import Path

root,module_path,module_name,test_name,output=sys.argv[1:]
sys.path[:0]=[str(Path(root)/'scripts'),str(Path(root)/'tests')]
temporary=None
if module_name=='dependency-root':
    import shutil,tempfile,test_tiger_recovery
    temporary=tempfile.TemporaryDirectory(prefix='codet configure control ')
    fixture=Path(temporary.name);(fixture/'cmake').mkdir()
    for name in ('compiler-identity.cmake','json.cmake','resolve-dependency-root.cmake'):
        shutil.copy2(Path(root)/'cmake'/name,fixture/'cmake'/name)
    shutil.copy2(module_path,fixture/'cmake/dependency-root.cmake')
    (fixture/'.deps').symlink_to((Path(root)/'.deps').resolve(),target_is_directory=True)
    test_tiger_recovery.ROOT=fixture
else:
    spec=importlib.util.spec_from_file_location(module_name,module_path)
    module=importlib.util.module_from_spec(spec);sys.modules[module_name]=module;spec.loader.exec_module(module)
if module_name=='package-tiger-portable':
    import test_tiger_packaging
    test_tiger_packaging.portable=module
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
 failure_details=[trace for _,trace in result.failures],failures=len(result.failures),assertion_failure=result.assertion_failure,expected_failures=len(result.expectedFailures),
 unexpected_successes=len(result.unexpectedSuccesses),exit=exit_code)))
if temporary:temporary.cleanup()
raise SystemExit(exit_code)
'''


def run_controls(root, folder):
    cases = [
        ("private-ownership", "dependency-root",
         '(NOT owned OR NOT cache_type STREQUAL "PATH" OR NOT managed)',
         '(NOT owned OR NOT cache_type STREQUAL "PATH" OR NOT managed OR NOT public_token STREQUAL "$CACHE{_CODET_DEPENDENCY_TOKEN}")',
         "test_tiger_recovery.DependencyReconfiguration.test_private_metadata_corruption_never_creates_explicit_intent"),
        ("missing-success", "dependency-root",
         'if((EXISTS "${pin}" AND NOT EXISTS "${witness}") OR\n       (EXISTS "${witness}" AND NOT EXISTS "${pin}"))',
         'if(FALSE)',
         "test_tiger_recovery.DependencyReconfiguration.test_missing_success_artifact_fails_and_explicit_replacement_recovers"),
        ("success-digest", "dependency-root",
         'if(NOT success_receipt_sha256 STREQUAL receipt_sha256)', 'if(FALSE)',
         "test_tiger_recovery.DependencyReconfiguration.test_success_witness_binds_receipt_bytes_and_identity"),
        ("legacy-help", "dependency-root",
         ')?Codet derived', ')Codet derived',
         "test_tiger_recovery.DependencyReconfiguration.test_inspection_preserves_unset_properties_and_readable_owned_help"),
        ("live-preparation-version", "evidence_benchmarks",
         'version not in ((1, 2) if historical else (METRIC_VERSION,))', 'version not in (1, 2)',
         "PreparationRecovery.test_live_metric_contract_cannot_be_downgraded_or_change_role"),
        ("live-advisory-checks", "evidence_benchmarks",
         'row["advisory_budget_checks"] != CHANGED_ADVISORY_CHECKS',
         'row["advisory_budget_checks"] not in (128, CHANGED_ADVISORY_CHECKS)',
         "PreparationRecovery.test_live_metric_contract_cannot_be_downgraded_or_change_role"),
        ("adapter-rewrite", "preparation_adapter",
         'if output.exists() and output.read_text() == adapter:', 'if output.exists():',
         "PreparationRecovery.test_adapter_driver_rewrites_only_after_production_excerpt_changes"),
        ("adapter-atomic", "preparation_adapter",
         'stream.write(adapter)', 'output.write_text(""); stream.write(adapter)',
         "PreparationRecovery.test_adapter_interruption_preserves_previous_complete_header"),
        ("orphan-receipt", "dependency-root",
         'set(owned "${managed}")',
         'set(owned "${managed}")\n  if(EXISTS "${pin}")\n    set(owned TRUE)\n  endif()',
         "test_tiger_recovery.DependencyReconfiguration.test_orphan_receipt_does_not_own_a_fresh_explicit_cache"),
        ("receipt-override", "dependency-root", 'if(explicit)\n    # A real public override', 'if(FALSE)\n    # A real public override',
         "test_tiger_recovery.DependencyReconfiguration.test_explicit_override_retires_a_bad_current_receipt_before_validation"),
        ("unset-properties", "dependency-root",
         'set_property(CACHE CODET_DEPENDENCY_ROOT PROPERTY ${property} "${saved_${property}}")\n      endif()',
         'set_property(CACHE CODET_DEPENDENCY_ROOT PROPERTY ${property} "${saved_${property}}")\n      else()\n        set_property(CACHE CODET_DEPENDENCY_ROOT PROPERTY ${property})\n      endif()',
         "test_tiger_recovery.DependencyReconfiguration.test_inspection_preserves_unset_properties_and_readable_owned_help"),
        ("preparation-workload", "evidence_benchmarks", 'workload = row.get("workload")',
         'workload = row.get("workload") or "saturated-count"',
         "PreparationRecovery.test_workload_validation_has_an_identifying_error"),
        ("preparation-allocations", "evidence_benchmarks",
         'if role == "changed" and workload.startswith("saturated-") and row["payload_sized_allocations"]:',
         'if False:', "PreparationRecovery.test_new_counter_contract_rejects_payload_allocations_and_duplicate_wrappers"),
        ("preparation-wrappers", "evidence_benchmarks",
         'if role == "changed" and workload == "cli-preparation" and row["distinct_command_wrappers"] != row["prepared_submissions"]:',
         'if False:', "PreparationRecovery.test_new_counter_contract_rejects_payload_allocations_and_duplicate_wrappers"),
        ("explicit-intent", "dependency-root",
         '(NOT owned OR NOT cache_type STREQUAL "PATH" OR NOT managed)',
         '(NOT owned)',
         "test_tiger_recovery.DependencyReconfiguration.test_equal_explicit_overrides_replace_derived_ownership_after_failure_or_success"),
        ("obsolete-pin", "dependency-root",
         'message(FATAL_ERROR "Obsolete dependency selection cache metadata in ${cache}; ${reset_hint}")',
         'set(owned FALSE)',
         "test_tiger_recovery.DependencyReconfiguration.test_obsolete_empty_markers_require_reset_and_do_not_rediscover"),
        ("literal-receipt", "dependency-root",
         'string(REPLACE "$" "\\\\u0024" json_${field} "${json_${field}}")',
         'set(json_${field} "${json_${field}}")',
         "test_tiger_recovery.DependencyReconfiguration.test_literal_generator_expressions_roundtrip_in_receipt_paths"),
        ("configure-regeneration", "dependency-root",
         'elseif(committed)',
         'elseif(FALSE)',
         "test_tiger_recovery.DependencyReconfiguration.test_successful_selection_survives_ninja_and_make_regeneration_until_reset"),
        ("configure-success", "dependency-root",
         'file(GENERATE OUTPUT "${relative_pin}" CONTENT "${record}")',
         'file(WRITE "${pin}" "${record}")',
         "test_tiger_recovery.DependencyReconfiguration.test_send_error_never_commits_a_first_selection"),
        ("configure-empty-reset", "dependency-root",
         'if(NOT cached OR (cache_value STREQUAL "" AND',
         'if(NOT cached OR (FALSE AND',
         "test_tiger_recovery.DependencyReconfiguration.test_empty_and_unset_overrides_reset_explicit_and_derived_selections"),
        ("configure-normal-scope", "dependency-root",
         'if(NOT origin STREQUAL "normal")', 'if(TRUE)',
         "test_tiger_recovery.DependencyReconfiguration.test_normal_variables_are_scoped_and_do_not_replace_a_derived_pin"),
        ("configuration-counters", "evidence_benchmarks",
         "row.get('configuration_metric_version') != 1 or cmake != 3 or row.get('configuration_checks') != 3 or row['files'] or row['owned_processes']",
         "False",
         "ConfigurationMeasurement.test_configuration_metric_requires_three_real_commands_and_checks"),
        ("history-preflight", "evidence_campaign",
         'raise ValueError("Prior attempt metadata is missing: " + str(path))',
         'raise FileNotFoundError("Prior attempt metadata is missing: " + str(path))',
         "EvidenceV4.test_history_preflight_does_not_leak_io_errors"),
        ("shared-fixture", "evidence_tool_bench",
         "import evidence_tiger_context_bench\n\n# Select each variant's tooling through the same harness, without modifying its tree.\nif __name__ == \"__main__\":\n    sys.path.insert(0, str(Path(sys.argv[1]) / \"scripts\"))",
         "if __name__ == \"__main__\":\n    sys.path.insert(0, str(Path(sys.argv[1]) / \"scripts\"))\nimport evidence_tiger_context_bench",
         "test_tiger_recovery.MeasurementExceptionOwnership.test_shared_fixture_cannot_be_shadowed_by_the_variant_support_module"),
        ("measure-constructor", "evidence_tiger_context_bench",
         "self.temporary.cleanup() # Constructor owns storage until startup returns.",
         "pass # Mutant leaves constructor storage behind.",
         "test_tiger_recovery.MeasurementExceptionOwnership.test_constructor_failure_releases_storage_before_the_exception_leaves"),
        ("recipe-admission", "tiger_provenance",
         "if value['recipe'] != recipe:",
         "if False:", "test_tiger_recovery.RecipeAdmission.test_current_recipe_must_match_before_any_build_directory_is_created"),
        ("root-environment", "dependency_paths",
         'os.environ.get("CODET_DEPENDENCY_ROOT") or root / ".deps"',
         'root / ".deps"', "test_tiger_recovery.DependencyReconfiguration.test_nonempty_environment_root_has_exact_precedence"),
        ("measure-cleanup", "evidence_tool_bench",
         "if failure is None:", "if sys.exc_info()[0] is None:",
         "test_tiger_recovery.MeasurementExceptionOwnership.test_caller_exception_does_not_hide_a_successful_measurements_cleanup_failure"),
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
        ("cwd-relationship", "evidence_paths", "if record.get('cwd') != cwd or record.get('canonical_cwd') != cwd:",
         'if False:', "EvidenceV4.test_v5_audit_rejects_jointly_changed_cwd_fields"),
        ("relocation-identity", "evidence_analysis", 'relocation_identity(tool, before) != relocation_identity(tool, after) or',
         'False or', "EvidenceV4.test_v5_relocations_cannot_substitute_another_finding"),
        ("ci-head", "evidence_ci", "run.get('head_sha') != head or",
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
        ("relocation-context", "evidence_analysis", 'not entry.get("rationale") or not context_matches or',
         'not entry.get("rationale") or False or', "EvidenceV4.test_relocations_require_full_identity_and_real_unchanged_source_context"),
        ("cppcheck-order", "evidence_analysis", 'tuple(sorted((loc[0], loc[3]) for loc in key[2]))',
         'tuple((loc[0], loc[3]) for loc in key[2])', "ReviewedCppcheckLocations.test_same_file_locations_can_reorder_without_changing_identity"),
        ("hash-version", "evidence_hash_experiment", "if payload.get('metric_scope') != METRIC_SCOPE or 'work_gate_passed' in payload:",
         'if False:', "HashMetricScope.test_missing_versions_bad_controls_and_ambiguous_names_are_rejected"),
        ("cleanup-original", "process_supervision", 'except BaseException as cleanup:',
         'except Exception as cleanup:', "ProcessCleanup.test_cleanup_exception_cannot_replace_original_failure"),
        ("cleanup-escalation", "process_supervision",
         "            except KeyboardInterrupt as error:\n                observation['interruptions'].append(dict(exception=type(error).__name__))\n            except (OSError, ValueError, subprocess.SubprocessError) as error:",
         '            except KeyboardInterrupt:\n                raise\n            except (OSError, ValueError, subprocess.SubprocessError) as error:',
         "ProcessCleanup.test_repeated_interruptions_do_not_skip_escalation_or_replace_timeout"),
        ("cleanup-inactive", "process_supervision", 'inactive = inspect(term_deadline)',
         'inactive = inspect(term_deadline) and False', "ProcessCleanup.test_inactive_group_returns_before_the_remaining_grace"),
        ("tooling-counts", "evidence_benchmarks",
         "if row['check_output_calls'] != row['git_commands'] + row['process_inspection_commands'] + cmake:",
         'if False:', "HashMetricScope.test_tooling_commands_and_memory_have_distinct_verified_scopes"),
        ("isolated-coverage", "gtest_isolation", 'if Counter(actual) != Counter(expected):',
         'if False:', "SanitizerIsolation.test_missing_duplicate_and_substituted_tests_are_rejected"),
        ("isolated-protocol", "gtest_isolation",
         "                failed = bool(list(root.iter('failure')) or list(root.iter('error')))\n                if failed and not completed.returncode:\n                    raise ValueError('Failed assertions with successful process exit')\n                if list(root.iter('skipped')):\n                    skipped.append(name)",
         "                if list(root.iter('skipped')):\n                    skipped.append(name)\n                failed = bool(list(root.iter('failure')) or list(root.iter('error')))\n                if failed and not completed.returncode:\n                    raise ValueError('Failed assertions with successful process exit')",
         "SanitizerIsolation.test_failed_and_skipped_cases_preserve_reports_with_inconsistent_zero_exit"),
        ("attempt-measurements", "evidence_attempt_audit",
         "        raise ValueError('Incomplete attempt must declare an empty measurement list and zero samples')",
         "        pass # Mutant accepts undeclared samples.",
         "test_attempt_evidence.AttemptPublication.test_measurements_and_reported_counts_must_both_be_empty"),
        ("inspection-binding", "evidence_processes",
         "value['inspector_sha256'] != inspector_digest", "False",
         "test_attempt_evidence.AttemptPublication.test_failed_process_inspection_remains_ineligible_and_source_bound"),
        ("tiger-manifest-prefix", "package-tiger-portable",
         '    manifest = context.manifest',
         '    manifest = json.loads((sysroot / "manifest.json").read_text())',
         "test_tiger_recovery.PinnedPackaging.test_transient_live_prefix_cannot_select_another_verified_terminfo_tree"),
        ("tiger-compiler-metadata", "tiger_provenance",
         '        value = json.loads((output / "codet-compiler-identity.json").read_text())',
         '        next(line for line in (output / "CMakeCache.txt").read_text().splitlines() if line.startswith("CMAKE_C_COMPILER:"))\n        value = json.loads((output / "codet-compiler-identity.json").read_text())',
         "test_tiger_recovery.CompilerIdentity.test_real_configure_with_toolchain_variables_and_spaces"),
        ("tiger-copied-input", "tiger_provenance",
         "    if file_hash(destination) != digest:", "    if False:",
         "test_tiger_recovery.PinnedPackaging.test_each_copied_sysroot_input_is_checked_even_if_source_is_restored"),
        ("tiger-pinned-context", "tiger_provenance",
         "def archived_inputs(root, context=None):\n    context = context or resolve(root)",
         "def archived_inputs(root, context=None):\n    context = resolve(root)",
         "test_tiger_recovery.PinnedPackaging.test_current_pointer_can_change_between_copy_stages_without_mixing_builds"),
    ]
    results = []
    for name, module, before, after, target in cases:
        print("Python control:", name, flush=True)
        directory = folder / ("control-" + name)
        directory.mkdir(exist_ok=True)
        suffix = ".cmake" if module == "dependency-root" else ".py"
        source = root / ("cmake" if module == "dependency-root" else "scripts") / (module + suffix)
        mutant = directory / (module + suffix)
        mutant.write_text(replace_once(source.read_text(), before, after))
        runs = []
        for label, path in (("baseline", source), ("mutant", mutant)):
            output = directory / (label + ".json")
            output.unlink(missing_ok=True)
            argv = [sys.executable, "-c", RUNNER, str(root), str(path), module,
                    target if target.startswith("test_") else "test_validation_evidence." + target, str(output)]
            process = supervised_run(argv, capture_output=True, text=True, timeout=30, cleanup_grace=4)
            value = json.loads(output.read_text())
            if value["exit"] != process.returncode:
                raise ValueError("Control exit metadata mismatch")
            value.update(argv=argv, log=process.stdout + process.stderr)
            runs.append(value)
        try:
            verify_unittest_result(*runs)
        except ValueError as error:
            raise ValueError("Control " + name + ": " + repr(runs)) from error
        markers = {"private-ownership": "Private corruption became explicit intent",
                   "missing-success": "Lost success artifact silently switched roots", "success-digest": "Mismatched success witness was accepted",
                   "legacy-help": "Legacy help spelling lost derived ownership", "live-preparation-version": "ValueError not raised",
                   "live-advisory-checks": "ValueError not raised", "adapter-rewrite": "Valid excerpt change did not rewrite the adapter",
                   "adapter-atomic": "Interrupted write truncated the adapter", "orphan-receipt": "Orphan receipt blocked explicit override",
                   "receipt-override": "Explicit replacement parsed retired receipt",
                   "unset-properties": "Inspection set formerly absent",
                   "preparation-workload": "ValueError not raised", "preparation-allocations": "ValueError not raised",
                   "preparation-wrappers": "ValueError not raised", "explicit-intent": "Equal explicit override lost", "obsolete-pin": "Obsolete pin silently rediscovered",
                   "literal-receipt": "Receipt evaluated literal", "configuration-counters": "ValueError not raised",
                   "configure-success": "Failed configuration committed a derived choice",
                   "configure-empty-reset": "Empty or unset override did not reset selection",
                   "configure-normal-scope": "Normal variable was promoted into cache",
                   "history-preflight": "Metadata validation leaked an I/O exception", "configure-regeneration": "Automatic regeneration changed dependency availability",
                   "shared-fixture": "Shared fixture inputs differ between variants", "measure-constructor": "Constructor failure retained owned temporary storage", "recipe-admission": "ValueError not raised", "root-environment": "Nonempty environment override was ignored", "measure-cleanup": "RuntimeError not raised",
                   "cleanup-original": "Cleanup restoration replaced the original timeout",
                   "cleanup-escalation": "Repeated interruption skipped escalation",
                   "cleanup-inactive": "Leader was reaped before group ownership ended",
                   "cppcheck-order": "Valid reordered Cppcheck relocation was rejected",
                   "hash-version": "ValueError not raised", "tooling-counts": "ValueError not raised", "isolated-coverage": "ValueError not raised",
                   "isolated-protocol": "Protocol error lost its failed report",
                   "inspection-binding": "ValueError not raised",
                   "tiger-manifest-prefix": "Transient live prefix selected a different terminfo tree",
                   "attempt-measurements": "ValueError not raised",
                   "tiger-compiler-metadata": "Valid configure metadata was rejected",
                   "tiger-pinned-context": "Packaging reloaded its mutable build pointer",
                   "tiger-copied-input": "ValueError not raised"}
        if name in markers and not any(markers[name] in text for text in runs[1]["failure_details"]):
            raise ValueError("Control missed its intended assertion: " + name)
        results.append(dict(name=name, test=target, baseline=runs[0], mutant=runs[1]))
    return results
