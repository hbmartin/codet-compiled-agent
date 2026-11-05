"""Failure-oriented tests for captured validation evidence and process inspection."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import validation_evidence as evidence


class ValidationEvidence(unittest.TestCase):
    def setUp(self):
        import os
        # A captured CTest run carries its own producer identity. Unit fixtures
        # create nested captures or explicit standalone runs, never that producer.
        clean = dict(os.environ)
        clean.pop("CODET_EVIDENCE_CONTEXT", None)
        context = patch.dict(os.environ, clean, clear=True)
        context.start()
        self.addCleanup(context.stop)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "src").mkdir()
        (self.root / "src/core.cpp").write_text("original")
        (self.root / "CMakeLists.txt").write_text("project(test)")
        self.manifest = self.root / "manifest.json"
        self.folder = self.root / "capture"
        self.output = self.root / "published.json"
        self.output.write_text("previous artifact")
        self.checks = [dict(id="test", argv=[sys.executable, "-c", "print('passed')"])]

    def setup_manifest(self):
        self.manifest.write_text(json.dumps(dict(root=str(self.root), checks=self.checks)))

    def capture(self):
        self.setup_manifest()
        return evidence.capture(self.manifest, self.folder)

    def publish(self, benchmark=None):
        return evidence.publish(self.manifest, self.folder, self.output, benchmark)

    def rejected(self, message, exception=ValueError):
        previous = self.output.read_bytes()
        with self.assertRaisesRegex(exception, message):
            self.publish()
        self.assertEqual(self.output.read_bytes(), previous)

    def test_success_captures_actual_command_log_and_sources(self):
        result = self.capture()["test"]
        self.assertEqual(result["exit"], 0)
        self.assertEqual(result["argv"], self.checks[0]["argv"])
        self.assertEqual(result["cwd"], str(self.root.resolve()))
        value = self.publish(dict(production_hashes=evidence.production_hashes(self.root)))
        self.assertFalse(value["measurements_apply_to_current_source"])
        self.assertTrue(value["source_equivalence"]["production_files_unchanged_since_correctness_benchmark"])
        self.assertTrue(value["source_equivalence"]["historical_unlinked_reference"])
        self.assertIn("passed", value["validation"][0]["log_tail"])

    def test_missing_result_rejected(self):
        self.capture()
        (self.folder / "test.json").unlink()
        self.rejected('test.json', FileNotFoundError)

    def test_nonzero_cannot_be_overridden_by_success_log(self):
        self.checks[0]["argv"] = [sys.executable, "-c", "print('All tests passed');raise SystemExit(7)"]
        result = self.capture()["test"]
        self.assertEqual(result["exit"], 7)
        self.rejected('Required check did not pass')

    def test_launch_failure(self):
        self.checks[0]["argv"] = [str(self.root / "missing-command")]
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "launch-error")
        self.assertIsNone(result["exit"])
        self.rejected('Command did not complete: test \\(launch-error\\)')

    def test_timeout(self):
        self.checks[0].update(argv=[sys.executable, "-c", "import time;time.sleep(60)"], timeout_seconds=0.05)
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "timeout")
        self.assertNotEqual(result["exit"], 0)
        self.rejected('Command did not complete: test \\(timeout\\)')

    def test_failed_build_never_runs_existing_test_binary(self):
        self.checks = [dict(id="build", argv=[sys.executable, "-c", "raise SystemExit(3)"]),
                       dict(id="test", requires=["build"], argv=[sys.executable, "-c", "open('launched','w').close()"])]
        results = self.capture()
        self.assertEqual(results["test"]["outcome"], "blocked")
        self.assertFalse((self.root / "launched").exists())
        self.rejected('Required check did not pass: build')

    def test_stale_sources_and_changed_log_rejected(self):
        for mutation in ("source", "log"):
            with self.subTest(mutation=mutation):
                self.capture()
                if mutation == "source":
                    (self.root / "src/core.cpp").write_text("changed")
                else:
                    (self.folder / "test.log").write_text("replaced")
                self.rejected('Stale or mismatched evidence')

    def test_source_mutation_during_capture_rejected(self):
        self.checks[0]["argv"] = [sys.executable, "-c", "open('src/core.cpp','w').write('new')"]
        self.capture()
        self.rejected('Stale or mismatched evidence')

    def test_changed_manifest_and_run_rejected(self):
        self.capture()
        record = json.loads((self.folder / "test.json").read_text())
        record["run_id"] = "previous capture"
        evidence.atomic_json(self.folder / "test.json", record)
        self.rejected('Stale or mismatched evidence')
        self.capture()
        self.checks[0]["argv"].append("another argument")
        self.setup_manifest()
        self.rejected('Capture manifest has changed')

    def test_changed_benchmark_sources_are_explicit(self):
        benchmark = dict(production_hashes=evidence.production_hashes(self.root), ref="recorded")
        (self.root / "src/core.cpp").write_text("changed after benchmark")
        self.capture()
        value = self.publish(benchmark)
        self.assertFalse(value["measurements_apply_to_current_source"])
        self.assertFalse(value["source_equivalence"]["production_files_unchanged_since_correctness_benchmark"])
        self.assertEqual(value["source_equivalence"]["benchmark"], benchmark)

    def test_analyzer_observations_keep_their_failure_status(self):
        self.checks[0].update(policy="observation", argv=[sys.executable, "-c", "raise SystemExit(1)"])
        self.capture()
        value = self.publish()
        self.assertEqual(value["validation"][0]["exit"], 1)
        self.assertEqual(value["validation"][0]["outcome"], "failed")

    def test_atomic_publication_failure_leaves_previous_artifact(self):
        self.capture()
        with patch.object(evidence.os, "replace", side_effect=OSError("publish failed")), self.assertRaises(OSError):
            self.publish()
        self.assertEqual(self.output.read_text(), "previous artifact")

    def test_invalid_manifest(self):
        self.checks += self.checks
        self.setup_manifest()
        with self.assertRaises(ValueError):
            evidence.capture(self.manifest, self.folder)


class ProcessInspection(unittest.TestCase):
    def test_executables_and_exact_script_arguments(self):
        processes = [dict(executable="/bin/zsh", argv=["zsh", "-c", "ctest --test-dir build/host"]),
                     dict(executable="/usr/bin/python3", argv=["python3", "-c", "analyze-modern.py"]),
                     dict(executable="/usr/bin/ctest", argv=["ctest", "--test-dir", "build/host"]),
                     dict(executable="/usr/bin/python3", argv=["python3", "-u", "scripts/analyze-modern.py"]),
                     dict(executable="build/host/codet_tests", argv=["codet_tests", "--gtest_filter=x"]),
                     dict(executable="/usr/bin/python3", argv=["python3", "-X", "utf8", "scripts/gtest_isolation.py", "--report", "result.json", "binary"])]
        self.assertEqual(evidence.blocking_processes(processes), processes[2:])

    def test_failed_or_empty_inspection_is_not_clean(self):
        with patch.object(evidence.subprocess, "check_output", side_effect=subprocess.CalledProcessError(1, "ps")), self.assertRaises(subprocess.CalledProcessError):
            evidence.require_quiet_processes()
        with patch.object(evidence.subprocess, "check_output", return_value=""), self.assertRaises(ValueError):
            evidence.require_quiet_processes()


class ProvenanceGuards(unittest.TestCase):
    setUp = ValidationEvidence.setUp
    setup_manifest = ValidationEvidence.setup_manifest
    capture = ValidationEvidence.capture
    publish = ValidationEvidence.publish
    rejected = ValidationEvidence.rejected

    def producer(self):
        script = self.root / "scripts/producer.py"
        script.parent.mkdir()
        script.write_text("import sys\nsys.path.insert(0," + repr(str(Path(evidence.__file__).parent)) + ")\n"
                          "from validation_evidence import write_artifact\n"
                          "from pathlib import Path\nwrite_artifact(Path('artifact.json'),'test',{'value':1},iter(['src/core.cpp']))\n")
        self.checks[0].update(argv=[sys.executable, str(script)], artifacts=[dict(
            path="artifact.json", kind="test", inputs=["src/core.cpp"])])

    def test_validation_changes_invalidate_capture(self):
        for mode in ("addition", "modification", "deletion"):
            with self.subTest(mode=mode):
                p = self.root / "tests/source.py"
                p.parent.mkdir(exist_ok=True)
                if mode != "addition":
                    p.write_text("before")
                elif p.exists():
                    p.unlink()
                self.capture()
                if mode == "deletion":
                    p.unlink()
                else:
                    p.write_text("after")
                self.rejected("Stale or mismatched evidence")

    def test_reserved_run_rejected_before_capture(self):
        self.checks[0]["id"] = "run"
        self.setup_manifest()
        with self.assertRaisesRegex(ValueError, "reserved check ID"):
            evidence.capture(self.manifest, self.folder)
        self.assertFalse(self.folder.exists())

    def test_legacy_capture_is_historical_only(self):
        self.capture()
        path = self.folder / "run.json"
        value = json.loads(path.read_text())
        value.pop("schema_version")
        evidence.atomic_json(path, value)
        self.rejected("Historical capture")

    def test_artifact_ownership_and_input_hashes(self):
        self.producer()
        self.capture()
        value = self.publish()
        self.assertEqual(value["derived_artifacts"][0]["payload"], {"value": 1})
        for key in ("capture_run_id", "campaign_id", "check_id", "production_hashes", "validation_hashes", "input_hashes"):
            with self.subTest(key=key):
                self.capture()
                path = self.root / "artifact.json"
                artifact = json.loads(path.read_text())
                artifact[key] = "stale"
                evidence.atomic_json(path, artifact)
                record_path = self.folder / "test.json"
                record = json.loads(record_path.read_text())
                record["artifacts"][0]["sha256"] = evidence.digest(path)
                evidence.atomic_json(record_path, record)
                self.rejected("Stale artifact|Artifact input mismatch")

    def test_old_artifact_cannot_be_readmitted(self):
        self.producer()
        self.capture()
        self.checks[0]["argv"] = [sys.executable, "-c", "pass"]
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "artifact-error")
        self.rejected("Command did not complete.*artifact-error")

    def test_python_version_is_verified(self):
        self.checks[0]["python_version"] = list(sys.version_info[:2])
        self.assertEqual(self.capture()["test"]["interpreter"]["version"][:2], list(sys.version_info[:2]))
        self.checks[0]["python_version"] = [0, 1]
        self.assertEqual(self.capture()["test"]["outcome"], "unavailable")
        self.rejected("Command did not complete.*unavailable")

    def test_bundle_is_immutable_and_complete(self):
        self.producer()
        self.capture()
        target = self.root / "public/run"
        value = evidence.publish_bundle(self.manifest, self.folder, target)
        self.assertEqual(json.loads((target / "report.json").read_text()), value)
        self.assertEqual(evidence.digest(target / "artifacts/test/artifact.json"), evidence.digest(self.root / "artifact.json"))
        from evidence_bundle import verify_bundle
        for name in ("manifest.json", "capture/run.json", "capture/test.json", "capture/test.log", "inventory.json", "inputs.json"):
            self.assertTrue((target / name).is_file(), name)
        self.assertTrue(verify_bundle(target)["verified"])
        with self.assertRaisesRegex(ValueError, "already exists"):
            evidence.publish_bundle(self.manifest, self.folder, target)

    def test_staging_failures_leave_previous_bundle_intact(self):
        self.producer()
        self.capture()
        parent = self.root / "public"
        previous = parent / "previous/report.json"
        previous.parent.mkdir(parents=True)
        previous.write_text("previous")
        import evidence_bundle
        original = evidence_bundle.shutil.copyfile
        for position in (1, 2, 3):
            calls = []
            def failure(*args, _calls=calls, _position=position, **kwargs):
                _calls.append(args)
                if len(_calls) == _position:
                    raise OSError("staging failed")
                return original(*args, **kwargs)
            with self.subTest(position=position), patch.object(evidence_bundle.shutil, "copyfile", side_effect=failure):
                with self.assertRaisesRegex(OSError, "staging failed"):
                    evidence.publish_bundle(self.manifest, self.folder, parent / "new")
            self.assertFalse((parent / "new").exists())
            self.assertEqual(previous.read_text(), "previous")
        with patch.object(evidence_bundle, "rename_exclusive", side_effect=OSError("rename failed")), self.assertRaisesRegex(OSError, "rename failed"):
            evidence.publish_bundle(self.manifest, self.folder, parent / "new")
        self.assertFalse((parent / "new").exists())

    def test_source_change_during_staging_rejects_bundle(self):
        self.capture()
        import evidence_bundle
        original = evidence_bundle.shutil.copyfile
        def change(*args, **kwargs):
            result = original(*args, **kwargs)
            (self.root / "src/core.cpp").write_text("changed during staging")
            return result
        target = self.root / "public/run"
        with patch.object(evidence_bundle.shutil, "copyfile", side_effect=change), self.assertRaisesRegex(ValueError, "Stale or mismatched|Audit input changed"):
            evidence.publish_bundle(self.manifest, self.folder, target)
        self.assertFalse(target.exists())

    def test_failed_observation_keeps_its_produced_artifact(self):
        self.producer()
        script = self.root / "scripts/producer.py"
        script.write_text(script.read_text() + "raise SystemExit(1)\n")
        self.checks[0]["policy"] = "observation"
        record = self.capture()["test"]
        self.assertEqual((record["outcome"], record["exit"], len(record["artifacts"])), ("failed", 1, 1))
        self.assertEqual(self.publish()["validation"][0]["exit"], 1)

    def test_environment_override_changes_invalidate_capture(self):
        self.checks[0]["env"] = {"EVIDENCE_TEST": "value"}
        self.capture()
        record = json.loads((self.folder / "test.json").read_text())
        self.assertEqual(record["env"], {"EVIDENCE_TEST": "value"})
        self.checks[0]["env"]["EVIDENCE_TEST"] = "changed"
        self.setup_manifest()
        self.rejected("Capture manifest has changed")

    def test_archived_outputs_are_protected(self):
        self.capture()
        for name in ("prompt-recovery-evidence-followup.json", "diagnostic-record-lifecycle-followup.json"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Historical evidence"):
                evidence.publish(self.manifest, self.folder, self.root / name)


class ComparisonGuards(unittest.TestCase):
    def analysis(self, root, tool="cppcheck"):
        return dict(tool=tool, versions=[dict(stdout="tool 1.0", stderr="", exit_code=0)], expected_sources=[str(root / "src/a.cpp")],
                    completed_sources=[str(root / "src/a.cpp")], passed=True, findings=0,
                    canaries=[dict(passed=True)], results=[dict(source=str(root / "src/a.cpp"),
                    completed_sources=[str(root / "src/a.cpp")], status="passed", failure=None, findings=[])])

    def test_analyzer_completeness_and_canaries(self):
        import copy

        from evidence_analysis import compare_analysis
        root = Path("/project")
        source = self.analysis(root)
        for change in ("incomplete", "duplicate", "canary", "failure", "count", "results"):
            value = copy.deepcopy(source)
            if change == "incomplete":
                value["completed_sources"] = []
            elif change == "duplicate":
                value["completed_sources"] *= 2
            elif change == "canary":
                value["canaries"][0]["passed"] = False
            elif change == "failure":
                value["results"][0]["failure"] = "parser failure"
            elif change == "count":
                value["findings"] = 1
            else:
                value["results"] = []
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, "coverage|canary|failure|count"):
                compare_analysis(source, value, root, root, ["src/a.cpp"])

    def test_cppcheck_locations_and_msg_are_preserved(self):
        import copy

        from evidence_analysis import compare_analysis
        root = Path("/project")
        source = self.analysis(root)
        source.update(findings=1, passed=False)
        source["results"][0].update(status="findings", findings=[dict(id="same", msg="message", locations=[dict(file="src/a.cpp", line="1", column="2")])])
        self.assertFalse(compare_analysis(source, source, root, root, ["src/a.cpp"])["baseline_passed"])
        for field in ("msg", "line", "file", "column"):
            other = copy.deepcopy(source)
            target = other["results"][0]["findings"][0]
            if field == "msg":
                target[field] = "different"
            else:
                target["locations"][0][field] = "different"
            self.assertFalse(compare_analysis(source, other, root, root, ["src/a.cpp"])["equal"])

    def samples(self):
        rows, summary = [], []
        for s in (1, 2):
            for variant in ("baseline", "changed"):
                for sample in range(6):
                    rows.append(dict(set=s, harness="test", workload="case", variant=variant, sample=sample,
                                     warmup=sample == 0, milliseconds=float(sample), allocations=0, allocated_bytes=0))
                summary.append(dict(set=s, harness="test", workload="case", variant=variant,
                                    milliseconds=3.0, allocations=0, allocated_bytes=0))
        return rows, summary

    def test_raw_samples_define_medians_and_counts(self):
        from evidence_benchmarks import verify_samples
        rows, summary = self.samples()
        value = verify_samples(rows, summary, dict(test=["case"]))
        self.assertEqual((value["samples"], value["measured_samples"], value["workloads"]), (24, 20, 1))
        self.assertTrue(value["all_measured_work_equal"])
        rows[3]["milliseconds"] = 99
        with self.assertRaisesRegex(ValueError, "Raw samples"):
            verify_samples(rows, summary, dict(test=["case"]))

    def test_missing_duplicate_or_malformed_samples_rejected(self):
        from evidence_benchmarks import verify_samples
        for mode in ("missing", "duplicate", "warmup", "nan"):
            rows, summary = self.samples()
            if mode == "missing":
                rows.pop()
            elif mode == "duplicate":
                rows.append(dict(rows[0]))
            elif mode == "warmup":
                rows[0]["warmup"] = False
            else:
                rows[2]["milliseconds"] = float("nan")
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, "samples|sample|warmup|metrics"):
                verify_samples(rows, summary, dict(test=["case"]))

    def test_work_difference_is_not_called_equal(self):
        from evidence_benchmarks import verify_samples
        rows, summary = self.samples()
        for r in rows + summary:
            if r["variant"] == "changed":
                r["allocations"] = 1
        value = verify_samples(rows, summary, dict(test=["case"]))
        self.assertFalse(value["all_measured_work_equal"])
        self.assertIn("allocations", value["comparisons"][0]["work_differences"])

    def test_mutants_require_passing_baseline_and_assertion_failure(self):
        from evidence_benchmarks import replace_once, verify_unittest_result
        baseline = dict(tests_run=1, errors=0, skipped=0, failures=0, exit=0)
        mutant = dict(baseline, failures=1, exit=1, assertion_failure=True)
        verify_unittest_result(baseline, mutant)
        for bad in (dict(mutant, errors=1), dict(mutant, tests_run=0), dict(mutant, assertion_failure=False)):
            with self.assertRaisesRegex(ValueError, "assertion|exactly one"):
                verify_unittest_result(baseline, bad)
        with self.assertRaisesRegex(ValueError, "baseline failed"):
            verify_unittest_result(mutant, mutant)
        with self.assertRaisesRegex(ValueError, "exactly once"):
            replace_once("aa", "a", "b")


class ExactProcessArguments(unittest.TestCase):
    def test_darwin_decoder_preserves_spaces(self):
        import ctypes

        from evidence_processes import darwin_arguments
        data = bytes(ctypes.c_int(3)) + b"/usr/bin/python3\0\0python3\0/tmp/with spaces/analyze-modern.py\0--tool\0ENV=value\0"
        self.assertEqual(darwin_arguments(data), ["python3", "/tmp/with spaces/analyze-modern.py", "--tool"])
        with self.assertRaisesRegex(ValueError, "Malformed"):
            darwin_arguments(b"x")

    def test_real_spaced_script_arguments(self):
        from evidence_processes import blocking_processes, read_argv
        if sys.platform != "darwin" and not sys.platform.startswith("linux"):
            self.skipTest("Exact argv inspection requires Darwin or Linux")
        with tempfile.TemporaryDirectory(prefix="argv space ") as d:
            script = Path(d) / "analyze-modern.py"
            script.write_text("import sys;print('ready',flush=True);sys.stdin.read()")
            process = subprocess.Popen([sys.executable, str(script)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            try:
                import select
                self.assertTrue(select.select([process.stdout], [], [], 5)[0], "child readiness deadline")
                self.assertEqual(process.stdout.readline(), b"ready\n")
                argv = read_argv(process.pid)
                self.assertEqual(argv[1], str(script))
                self.assertEqual(len(blocking_processes([dict(executable=sys.executable, argv=argv)])), 1)
            finally:
                process.stdin.close()
                process.stdout.close()
                process.wait(timeout=5)

    def test_inspection_timeout_is_not_clean(self):
        with patch.object(evidence.subprocess, "check_output", side_effect=subprocess.TimeoutExpired("inspection", 5)), self.assertRaises(subprocess.TimeoutExpired):
            evidence.require_quiet_processes()


    def test_vanished_process_is_skipped_but_denial_is_not_clean(self):
        import errno

        import evidence_processes as processes
        with patch.object(processes.subprocess, "check_output", return_value="123 S /usr/bin/python3\n"), patch.object(processes, "read_argv", side_effect=OSError(errno.ESRCH, "gone")):
            with self.assertRaisesRegex(ValueError, "no usable records"):
                processes.process_snapshot()
        with patch.object(processes.subprocess, "check_output", return_value="123 S /usr/bin/python3\n"), patch.object(processes, "read_argv", side_effect=PermissionError(errno.EPERM, "denied")):
            with self.assertRaises(PermissionError):
                processes.process_snapshot()


class ReplayAndGitGuards(unittest.TestCase):
    def git_fixture(self, root):
        (root / "src").mkdir(exist_ok=True)
        (root / "src/a.cpp").write_text("original")
        (root / "CMakeLists.txt").write_text("project(test)")
        for args in (["init", "-q"], ["add", "."], ["-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"]):
            subprocess.run(["git", "-C", str(root)] + args, check=True, capture_output=True, timeout=5)

    def test_wrong_git_baseline_rejected(self):
        from evidence_benchmarks import verify_git_source
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            self.git_fixture(root)
            value = verify_git_source(root, root, "HEAD")
            self.assertEqual(len(value["production_hashes"]), 2)
            (root / "src/a.cpp").write_text("changed")
            with self.assertRaisesRegex(ValueError, "does not match"):
                verify_git_source(root, root, "HEAD")

    def test_ci_validation_inputs_must_match_commit(self):
        from evidence_benchmarks import verify_git_validation
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "tests").mkdir()
            (root / "tests/input.py").write_text("original")
            self.git_fixture(root)
            self.assertEqual(len(verify_git_validation(root, root, "HEAD")), 1)
            (root / "tests/input.py").write_text("changed")
            with self.assertRaisesRegex(ValueError, "Validation inputs do not match"):
                verify_git_validation(root, root, "HEAD")

    def test_portable_manifest_has_no_shell_path_and_checks_python(self):
        from evidence_campaign import prepare
        with tempfile.TemporaryDirectory(prefix="replay space ") as d:
            root = Path(d)
            self.git_fixture(root)
            alias = root / "python-venv"
            alias.symlink_to(sys.executable)
            manifest = json.loads(prepare(root, Path("build/campaign"), "HEAD", str(alias), "python3.9", sys.executable,
                                        clang_tidy=sys.executable, cppcheck=sys.executable).read_text())
            config = json.loads((root / "build/campaign/config.json").read_text())
            self.assertEqual(config["check_python"], str(alias))
            self.assertEqual(manifest["root"], str(root.resolve()))
            for c in manifest["checks"]:
                self.assertFalse(any(s.startswith("PATH=") for s in c["argv"]))
                if c["id"].startswith("python39"):
                    self.assertEqual(c["python_version"], [3, 9])

    def test_optimized_python_keeps_publication_guard(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "src").mkdir()
            (root / "src/a.cpp").write_text("original")
            (root / "CMakeLists.txt").write_text("project(test)")
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps(dict(root=str(root), checks=[dict(id="test", argv=[sys.executable, "-c", "pass"])])))
            evidence.capture(manifest, root / "capture")
            (root / "src/a.cpp").write_text("changed")
            output = root / "report.json"
            output.write_text("previous")
            process = subprocess.run([sys.executable, "-O", evidence.__file__, "publish", str(manifest), str(root / "capture"), "--output", str(output)], capture_output=True, text=True, timeout=5)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn("Stale or mismatched evidence", process.stderr)
            self.assertEqual(output.read_text(), "previous")


class ReplayIntegrity(unittest.TestCase):
    setUp = ValidationEvidence.setUp
    setup_manifest = ValidationEvidence.setup_manifest
    capture = ValidationEvidence.capture
    publish = ValidationEvidence.publish
    rejected = ValidationEvidence.rejected
    producer = ProvenanceGuards.producer

    def test_blocked_check_does_not_inspect_missing_inputs(self):
        self.checks = [dict(id="build", argv=[sys.executable, "-c", "raise SystemExit(1)"]),
                       dict(id="test", requires=["build"], argv=[sys.executable, "-c", "raise SystemExit(77)"], file_inputs=["absent"])]
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "blocked")
        self.assertIsNone(result["exit"])

    def test_missing_input_and_output_are_artifact_errors(self):
        for field in ("file_inputs", "outputs"):
            self.checks = [dict(id="test", argv=[sys.executable, "-c", "pass"], **{field: [dict(path="absent")]})]
            with self.subTest(field=field):
                record = self.capture()["test"]
                self.assertEqual(record["outcome"], "artifact-error")
                self.assertIn("not produced", record["error"])

    def test_input_read_failure_is_not_a_launch_failure(self):
        with patch.object(evidence, "input_hashes", side_effect=PermissionError("declared input unreadable")), patch.object(evidence.subprocess, "Popen") as launch:
            record = self.capture()["test"]
        self.assertEqual(record["outcome"], "artifact-error")
        self.assertIn("declared input unreadable", record["error"])
        launch.assert_not_called()

    def test_missing_python_is_unavailable(self):
        self.checks[0].update(argv=[str(self.root / "missing-python")], python_version=[3, 9])
        self.assertEqual(self.capture()["test"]["outcome"], "unavailable")

    def test_consumer_rejects_replaced_prerequisite_executable(self):
        self.checks = [dict(id="build", argv=[sys.executable, "-c", "open('binary','w').write('built')"], outputs=[dict(path="binary", archive=False)]),
                       dict(id="replace", argv=[sys.executable, "-c", "open('binary','w').write('stale')"], requires=["build"]),
                       dict(id="test", requires=["build", "replace"], argv=[sys.executable, "-c", "open('launched','w').close()"])]
        results = self.capture()
        self.assertEqual(results["test"]["outcome"], "artifact-error")
        self.assertIn("Prerequisite output changed", results["test"]["error"])
        self.assertFalse((self.root / "launched").exists())

    def test_baseline_source_and_generated_inputs_are_bound(self):
        source = self.root / "baseline"
        (source / "prompts").mkdir(parents=True)
        (source / "CMakeLists.txt").write_text("baseline")
        (source / "prompts/manifest.json").write_text("prompt")
        self.checks[0]["source_inputs"] = [dict(path="baseline")]
        self.capture()
        (source / "prompts/manifest.json").write_text("changed generated input")
        self.rejected("Build source inputs changed")

    def test_effective_path_is_not_ambient_and_uses_check_override(self):
        import os
        for name in ("first", "second"):
            directory = self.root / name
            directory.mkdir()
            path = directory / "probe"
            path.write_text("#!/bin/sh\necho " + name + "\n")
            path.chmod(0o755)
        self.checks[0].update(argv=["probe"], env=dict(PATH=str(self.root / "first")))
        with patch.dict(os.environ, dict(PATH=str(self.root / "second"), CXX="unrecorded")):
            record = self.capture()["test"]
        self.assertEqual(record["resolved_executable"], str(self.root / "first/probe"))
        self.assertEqual((self.folder / "test.log").read_text(), "first\n")
        self.assertNotIn("CXX", record["effective_environment"])

    def test_executable_changes_invalidate_publication(self):
        tool = self.root / "probe"
        tool.write_text("#!/bin/sh\nexit 0\n")
        tool.chmod(0o755)
        self.checks[0]["argv"] = [str(tool)]
        self.capture()
        tool.write_text("#!/bin/sh\nexit 7\n")
        self.rejected("Check executable changed")

    def test_archive_excludes_stale_records_and_survives_relocation(self):
        import shutil

        from evidence_bundle import verify_bundle
        self.producer()
        self.capture()
        (self.folder / "old.log").write_text("old run")
        target = self.root / "bundle"
        evidence.publish_bundle(self.manifest, self.folder, target)
        self.assertFalse((target / "capture/old.log").exists())
        index = json.loads((target / "inputs.json").read_text())
        self.assertTrue(any(i["role"] == "input" and i["path"] == "src/core.cpp" and i["storage"] for i in index))
        with tempfile.TemporaryDirectory() as d:
            relocated = Path(d) / "audit"
            shutil.copytree(target, relocated)
            shutil.rmtree(self.root / "src")
            self.assertTrue(verify_bundle(relocated)["verified"])
            item = next(i for i in index if i["storage"])
            (relocated / item["storage"]).write_text("damaged")
            with self.assertRaisesRegex(ValueError, "inventory mismatch"):
                verify_bundle(relocated)

    def test_measurement_artifacts_have_distinct_complete_sidecars(self):
        from evidence_bundle import verify_bundle
        rows, summary = ComparisonGuards.samples(self)
        config = self.root / "config.json"
        evidence.atomic_json(config, dict(cases=dict(test=["case"]), sets=2, measured=5))
        script = self.root / "producer.py"
        script.write_text("import sys\nfrom pathlib import Path\nsys.path.insert(0," + repr(str(Path(evidence.__file__).parent)) + ")\nfrom validation_evidence import write_artifact\nimport json\npayload=json.loads(" + repr(json.dumps(dict(rows=rows, summary=summary))) + ")\nfor name in sys.argv[1:]:\n write_artifact(Path(name),'measure',payload,['config.json'])\n")
        self.checks = [dict(id=n, argv=[sys.executable, str(script), n + ".json"], artifacts=[dict(path=n + ".json", kind="measure", inputs=["config.json"])]) for n in ("first", "second")]
        self.checks.append(dict(id="same-stem", argv=[sys.executable, str(script), "left.json", "left.jsonl"], artifacts=[dict(path=n, kind="measure", inputs=["config.json"]) for n in ("left.json", "left.jsonl")]))
        self.capture()
        target = self.root / "bundle"
        try:
            evidence.publish_bundle(self.manifest, self.folder, target)
        except ValueError as error:
            if "Measurement sidecar namespace mismatch" not in str(error):
                raise
            self.fail("Artifact-specific sidecar identity was not preserved: " + str(error))
        self.assertTrue(verify_bundle(target)["verified"])
        self.assertEqual(len(list((target / "measurements").rglob("samples.csv"))), 4)
        self.assertEqual(len(json.loads((target / "measurements.json").read_text())), 4)

    def test_all_required_sidecars_are_verified(self):
        import shutil

        from evidence_bundle import verify_bundle
        self.capture()
        target = self.root / "bundle"
        evidence.publish_bundle(self.manifest, self.folder, target)
        for name in ("manifest.json", "capture/run.json", "capture/test.json", "capture/test.log", "inputs.json", "report.json", "measurements.json"):
            path = target / name
            data = path.read_bytes()
            path.unlink()
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "inventory mismatch"):
                verify_bundle(target)
            path.write_bytes(data)
        shutil.rmtree(target / "inputs")
        with self.assertRaisesRegex(ValueError, "inventory mismatch"):
            verify_bundle(target)

    def test_every_atomic_staging_boundary_preserves_previous_publication(self):
        self.capture()
        previous = self.root / "previous/report.json"
        previous.parent.mkdir()
        previous.write_text("previous")
        original = evidence.atomic_json
        for position in (1, 2, 3, 4):
            calls = []
            def fail(path, value, _position=position, _calls=calls):
                _calls.append(path)
                if len(_calls) == _position:
                    raise OSError("intentional staging failure")
                return original(path, value)
            target = self.root / ("bundle-" + str(position))
            with self.subTest(position=position), patch.object(evidence, "atomic_json", side_effect=fail), self.assertRaisesRegex(OSError, "intentional staging failure"):
                evidence.publish_bundle(self.manifest, self.folder, target)
            self.assertFalse(target.exists())
            self.assertEqual(previous.read_text(), "previous")

    def test_standalone_relative_config_produces_unbound_result(self):
        import evidence_campaign as campaign
        config = self.root / "config.json"
        evidence.atomic_json(config, dict(root=str(self.root), folder=str(self.root)))
        for variant in ("baseline", "changed"):
            cache = self.root / (variant + "-release/CMakeCache.txt")
            cache.parent.mkdir()
            cache.write_text("cache")
        (self.root / "harness.json").write_text("harness")
        result = self.root / "standalone.json"
        relative = Path(__import__('os').path.relpath(config))
        with patch.object(campaign, "measure", return_value={"completed": True}):
            campaign.produce(relative, "measure", output=result)
        value = json.loads(result.read_text())
        self.assertEqual(value["binding"], "standalone-unbound")
        self.assertNotIn("capture_run_id", value)
        with patch.object(campaign, "measure") as work, self.assertRaisesRegex(ValueError, "explicit --output"):
            campaign.produce(relative, "measure")
        work.assert_not_called()

    def test_process_gate_matches_versioned_and_truncated_names(self):
        from evidence_processes import blocking_processes
        for name in ("clang-tidy-18", "clang-tidy-23.1", "codet_publicatio", "analyze-modern."):
            with self.subTest(name=name):
                self.assertEqual(len(blocking_processes([dict(executable=name, argv=[name])])), 1)
        self.assertEqual(blocking_processes([dict(executable="sh", argv=["sh", "-c", "echo clang-tidy-18"])]), [])

    def test_process_snapshot_inspects_native_candidate_argv(self):
        import evidence_processes as processes
        with patch.object(processes.subprocess, "check_output", return_value="123 S clang-tidy-18\n"), patch.object(processes, "read_argv", return_value=["/bin/clang-tidy-18"]) as argv, patch.object(processes.os, "readlink", return_value="/bin/clang-tidy-18"):
            values = processes.process_snapshot()
        argv.assert_called_once_with("123")
        self.assertEqual(values[0]["argv"], ["/bin/clang-tidy-18"])


    def test_external_installation_identity_changes_are_detected(self):
        dependency = self.root / "dependency"
        dependency.mkdir()
        (dependency / "header.h").write_text("pinned installation")
        self.checks[0]["directory_inputs"] = ["dependency"]
        self.capture()
        (dependency / "header.h").write_text("changed installation")
        self.rejected("External build inputs changed")

    def test_source_commit_binding_rejects_wrong_baseline(self):
        import shutil

        from evidence_inputs import source_inputs
        ReplayAndGitGuards.git_fixture(self, self.root)
        baseline = self.root / "baseline"
        baseline.mkdir()
        shutil.copyfile(self.root / "CMakeLists.txt", baseline / "CMakeLists.txt")
        shutil.copytree(self.root / "src", baseline / "src")
        self.assertEqual(set(source_inputs(self.root, [dict(path="baseline", git_ref="HEAD")])), {"baseline"})
        self.checks[0]["source_inputs"] = [dict(path="baseline", git_ref="HEAD")]
        self.capture()
        from evidence_bundle import verify_bundle
        target = self.root / "git-bound-bundle"
        evidence.publish_bundle(self.manifest, self.folder, target)
        self.assertTrue(verify_bundle(target)["verified"])
        (baseline / "src/core.cpp").write_text("wrong baseline")
        with self.assertRaisesRegex(ValueError, "does not match Git commit"):
            source_inputs(self.root, [dict(path="baseline", git_ref="HEAD")])

    def test_build_info_uses_each_cache_compiler_and_declares_caches(self):
        import evidence_campaign as campaign
        builds = {"debug": str(self.root / "debug"), "cross": str(self.root / "cross")}
        caches = []
        for directory in list(builds.values()) + [str(self.root / (v + "-release")) for v in ("baseline", "changed")]:
            path = Path(directory) / "CMakeCache.txt"
            path.parent.mkdir()
            path.write_text("CMAKE_CXX_COMPILER:FILEPATH=" + sys.executable + "\n")
            caches.append(path)
        config = self.root / "config.json"
        evidence.atomic_json(config, dict(root=str(self.root), folder=str(self.root), builds=builds))
        output = self.root / "build-info.json"
        campaign.produce(config, "build-info", output=output)
        value = json.loads(output.read_text())
        self.assertEqual(set(value["input_hashes"]), {"config.json"} | {str(p.relative_to(self.root)) for p in caches})
        self.assertEqual({v["compiler_path"] for v in value["payload"]}, {sys.executable})

    def test_unbound_payload_cannot_satisfy_capture(self):
        self.producer()
        self.capture()
        path = self.root / "artifact.json"
        value = json.loads(path.read_text())
        value.pop("capture_run_id")
        value["binding"] = "standalone-unbound"
        evidence.atomic_json(path, value)
        record_path = self.folder / "test.json"
        record = json.loads(record_path.read_text())
        record["artifacts"][0]["sha256"] = evidence.digest(path)
        evidence.atomic_json(record_path, record)
        self.rejected("Stale artifact.*capture_run_id")

    def test_every_compatibility_producer_has_explicit_output_and_correct_action(self):
        import runpy

        import evidence_campaign
        root = Path(__file__).resolve().parents[1]
        for suffix, action in (("measure", "measure"), ("compare-static", "analysis"),
                               ("negative-controls", "native-negative-controls"), ("python-negative-controls", "negative-controls")):
            path = root / ("docs/validation/prompt-recovery-evidence-followup-" + suffix + ".py")
            with self.subTest(suffix=suffix), patch.object(sys, "argv", [str(path), "relative config.json", "--output", "explicit output.json"]), patch.object(evidence_campaign, "produce", return_value=None) as producer:
                with self.assertRaises(SystemExit) as ended:
                    runpy.run_path(str(path), run_name="__main__")
                self.assertIsNone(ended.exception.code)
                producer.assert_called_once_with(Path("relative config.json"), action, output=Path("explicit output.json"))

    def test_template_defaults_and_cli_overrides_are_effective(self):
        import evidence_campaign as campaign
        ReplayAndGitGuards.git_fixture(self, self.root)
        template = self.root / "template.json"
        evidence.atomic_json(template, dict(schema_version=5, baseline="HEAD", python39="unavailable-python39", builds=dict(debug="custom-debug", asan="custom-asan", tsan="custom-tsan", cross="custom-cross"), benchmark=dict(sets=3, measured_per_set=4)))
        manifest = campaign.prepare(self.root, Path("build/fresh"), check_python=sys.executable, analysis_python=sys.executable,
                                    template_path=template, clang_tidy=sys.executable, cppcheck=sys.executable)
        checks = json.loads(manifest.read_text())["checks"]
        self.assertIn("custom-debug", next(c for c in checks if c["id"] == "debug-configure")["argv"][4])
        config = json.loads((self.root / "build/fresh/config.json").read_text())
        self.assertNotIn("SSL_CERT_FILE", config["environment"])
        self.assertEqual((config["sets"], config["measured"], config["python39"]), (3, 4, campaign.discover_python39(preferred="unavailable-python39")))
        other = campaign.prepare(self.root, Path("build/other"), "HEAD", sys.executable, sys.executable, sys.executable,
                                 template_path=template, clang_tidy=sys.executable, cppcheck=sys.executable)
        self.assertEqual(json.loads((other.parent / "config.json").read_text())["python39"], sys.executable)


    def test_every_compatibility_wrapper_preserves_captured_output(self):
        import os
        import runpy

        import evidence_campaign as campaign
        import evidence_native_controls
        import evidence_negative_controls
        root = Path(__file__).resolve().parents[1]
        config = self.root / "config.json"
        debug = self.root / "debug"
        debug.mkdir()
        for name in ("compile_commands.json", "build.ninja"):
            (debug / name).write_text("fixture")
        for variant in ("baseline", "changed"):
            cache = self.root / (variant + "-release/CMakeCache.txt")
            cache.parent.mkdir()
            cache.write_text("fixture")
            evidence.atomic_json(self.root / ("analysis-" + variant + ".json"), dict(payload=dict(manifests={t: {} for t in campaign.TOOLS})))
        (self.root / "harness.json").write_text("harness")
        (self.root / "checks").mkdir()
        (self.root / "checks/analyzer-relocations.json").write_text('{"entries":[]}')
        evidence.atomic_json(config, dict(root=str(self.root), folder=str(self.root), builds=dict(debug=str(debug)), tools=dict(ninja=dict(path="ninja"))))
        for suffix, action, extras in (
            ("measure", "measure", ["harness.json", "baseline-release/CMakeCache.txt", "changed-release/CMakeCache.txt"]),
            ("compare-static", "analysis", ["analysis-baseline.json", "analysis-changed.json", "checks/analyzer-relocations.json"]),
            ("negative-controls", "native-negative-controls", ["debug/compile_commands.json", "debug/build.ninja"]),
            ("python-negative-controls", "negative-controls", []),
        ):
            wrapper = root / ("docs/validation/prompt-recovery-evidence-followup-" + suffix + ".py")
            for explicit in (False, True):
                output = self.root / (("explicit-" + suffix) if explicit else action)
                output = output.with_suffix(".json")
                spec = dict(path=str(output.relative_to(self.root)), kind=action, inputs=["config.json"] + extras)
                context = evidence._context(self.root, dict(id="run", campaign_id="campaign"), "captured-wrapper", evidence.production_hashes(self.root), evidence.validation_hashes(self.root))
                context["declared_artifacts"] = [spec]
                argv = [str(wrapper), str(config)] + (["--output", str(output)] if explicit else [])
                with self.subTest(suffix=suffix, explicit=explicit), patch.dict(os.environ, dict(CODET_EVIDENCE_CONTEXT=json.dumps(context))), patch.object(sys, "argv", argv), patch.object(campaign, "measure", return_value={"computed": True}), patch.object(campaign, "compare_analysis", return_value={"equal": True}), patch.object(evidence_native_controls, "run_controls", return_value=[]), patch.object(evidence_negative_controls, "run_controls", return_value=[]):
                    with self.assertRaises(SystemExit) as ended:
                        runpy.run_path(str(wrapper), run_name="__main__")
                    self.assertIsNone(ended.exception.code)
                    self.assertEqual(json.loads(output.read_text()).get("capture_run_id"), "run")
                    value = evidence.validate_artifact(output, spec, context)
                    self.assertEqual(value["capture_run_id"], "run")
                    self.assertNotIn("binding", value)

    def test_undeclared_captured_wrapper_output_fails_before_work(self):
        import os

        import evidence_campaign as campaign
        config = self.root / "config.json"
        evidence.atomic_json(config, dict(root=str(self.root), folder=str(self.root)))
        with patch.dict(os.environ, dict(CODET_EVIDENCE_CONTEXT=json.dumps(dict(declared_artifacts=[])))), patch.object(campaign, "measure") as work, self.assertRaisesRegex(ValueError, "not declared"):
            campaign.produce(config, "measure", output=self.root / "wrong.json")
        work.assert_not_called()



class ProcessCleanup(unittest.TestCase):
    def test_repeated_interruptions_do_not_skip_escalation_or_replace_timeout(self):
        import contextlib
        import signal

        import process_supervision as supervisor
        clock, signals, sleeps, scans, waits = [0.0], [], [0], [0], [0]
        original = subprocess.TimeoutExpired(['owned'], 1)
        class Process:
            pid, returncode = 123456, -9
            def communicate(self, **kwargs): raise original
            def wait(self, timeout):
                waits[0] += 1
                if waits[0] == 1: raise KeyboardInterrupt('reap')
                return -9
            def kill(self): raise AssertionError('Unexpected direct kill')
        process = Process()
        def kill(group, action):
            signals.append(action)
            if len(signals) == 1: raise KeyboardInterrupt('signal')
        def inspect(group, **kwargs):
            scans[0] += 1
            if scans[0] == 1: raise KeyboardInterrupt('inspection')
            return False, ['S']
        def sleep(delay):
            clock[0] += delay
            sleeps[0] += 1
            if sleeps[0] == 1: raise KeyboardInterrupt('grace')
        with patch.object(supervisor, 'interruption', contextlib.nullcontext), \
             patch.object(supervisor, 'cleanup_interruptions', lambda _: contextlib.nullcontext()), \
             patch.object(supervisor.subprocess, 'Popen', return_value=process), \
             patch.object(supervisor.os, 'killpg', side_effect=kill), \
             patch.object(supervisor, 'group_inactive', side_effect=inspect), \
             patch.object(supervisor.time, 'sleep', side_effect=sleep), \
             patch.object(supervisor.time, 'monotonic', side_effect=lambda:clock[0]):
            with self.assertRaises(subprocess.TimeoutExpired) as caught:
                supervisor.run(['owned'], timeout=1, cleanup_grace=.1)
        self.assertIs(caught.exception, original, 'Cleanup replaced the original timeout')
        self.assertIn(signal.SIGKILL, signals, 'Repeated interruption skipped escalation')
        self.assertTrue(original.process_cleanup['reaped'])
        self.assertEqual(len(original.process_cleanup['interruptions']), 4)
        self.assertEqual(original.returncode, -9)

    def test_cleanup_exception_cannot_replace_original_failure(self):
        import contextlib

        import process_supervision as supervisor
        original = subprocess.TimeoutExpired(['owned'], 1)
        class Process:
            pid, returncode = 123456, -9
            group_cleanup = dict(reaped=True)
            def communicate(self, **kwargs): raise original
        with patch.object(supervisor, 'interruption', contextlib.nullcontext), \
             patch.object(supervisor.subprocess, 'Popen', return_value=Process()), \
             patch.object(supervisor, 'stop_group', side_effect=KeyboardInterrupt('restoring handlers')):
            try:
                with self.assertRaises(subprocess.TimeoutExpired) as caught:
                    supervisor.run(['owned'], timeout=1)
            except KeyboardInterrupt:
                self.fail('Cleanup restoration replaced the original timeout')
        self.assertIs(caught.exception, original, 'Cleanup restoration replaced the original timeout')
        self.assertEqual(original.cleanup_failure, 'restoring handlers')
        self.assertTrue(original.process_cleanup['reaped'])

    def test_inactive_group_returns_before_the_remaining_grace(self):
        import contextlib
        import signal

        import process_supervision as supervisor
        clock, signals, scans = [0.0], [], [0]
        class Process:
            pid = 123456
            def wait(self, timeout):
                self_outer.assertEqual(scans[0], 2, 'Leader was reaped before group ownership ended')
                return 0
        self_outer = self
        process = Process()
        def inspect(group, **kwargs):
            scans[0] += 1
            return scans[0] > 1, ['Z'] if scans[0] > 1 else ['S']
        with patch.object(supervisor, 'cleanup_interruptions', lambda _: contextlib.nullcontext()), \
             patch.object(supervisor.os, 'killpg', side_effect=lambda group, action:signals.append(action)), \
             patch.object(supervisor, 'group_inactive', side_effect=inspect), \
             patch.object(supervisor.time, 'sleep', side_effect=lambda delay:clock.__setitem__(0,clock[0]+delay)), \
             patch.object(supervisor.time, 'monotonic', side_effect=lambda:clock[0]):
            self.assertEqual(supervisor.stop_group(process, grace=2), 0)
        self.assertEqual(clock[0], .05, 'Inactive group consumed the complete grace period')
        self.assertEqual(signals, [signal.SIGTERM])

    def test_second_signal_is_deferred_and_real_descendants_are_reaped(self):
        import os
        import select
        import signal
        import time
        with tempfile.TemporaryDirectory() as folder:
            marker = Path(folder)/'escaped'
            grandchild = 'import signal,time;from pathlib import Path;signal.signal(signal.SIGTERM,signal.SIG_IGN);print("grandchild",flush=True);time.sleep(1);Path('+repr(str(marker))+').write_text("escaped")'
            child = 'import signal,subprocess,sys,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);p=subprocess.Popen([sys.executable,"-c",'+repr(grandchild)+'],stdout=subprocess.PIPE);p.stdout.readline();print("ready",flush=True);time.sleep(2)'
            program = '''import json,signal,sys
import process_supervision as s
previous={n:signal.getsignal(n) for n in (signal.SIGINT,signal.SIGTERM)}
original=s.group_inactive
announced=False
def inspect(*a,**k):
 global announced
 if not announced:print("cleanup-ready",flush=True);announced=True
 return original(*a,**k)
s.group_inactive=inspect
try:s.run([sys.executable,"-c",CHILD],timeout=60,cleanup_grace=.3)
except KeyboardInterrupt as error:
 print(json.dumps(dict(message=str(error),cleanup=error.process_cleanup,restored=all(signal.getsignal(n)==h for n,h in previous.items()))),flush=True)
 raise SystemExit(17)
'''.replace('CHILD',repr(child))
            process = subprocess.Popen([sys.executable,'-c',program],stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                env=dict(os.environ,PYTHONPATH=str(Path(__file__).resolve().parents[1]/'scripts')))
            def line():
                self.assertTrue(select.select([process.stdout],[],[],5)[0], 'Supervisor readiness gate was not reached')
                return process.stdout.readline()
            try:
                self.assertEqual(line(),b'ready\n')
                process.send_signal(signal.SIGTERM)
                self.assertEqual(line(),b'cleanup-ready\n')
                process.send_signal(signal.SIGTERM)
                process.send_signal(signal.SIGINT)
                result=json.loads(line())
                self.assertEqual(process.wait(timeout=5),17)
                self.assertEqual(result['message'],'Interrupted by SIGTERM')
                self.assertTrue(result['cleanup']['reaped'])
                self.assertTrue(result['restored'])
                self.assertGreaterEqual(len(result['cleanup']['interruptions']),1)
                self.assertIn(int(signal.SIGKILL),[s['signal'] for s in result['cleanup']['signals']])
                time.sleep(1.1)
                self.assertFalse(marker.exists(),'Repeated interruption left a live descendant')
            finally:
                if process.poll() is None:process.kill();process.wait(timeout=5)
                process.stdout.close();process.stderr.close()

    def test_transient_inspection_failure_can_be_followed_by_confirmed_cleanup(self):
        import process_supervision as supervisor
        class Process:
            pid=123456
            def wait(self, **kwargs):return 0
        process=Process()
        with patch.object(supervisor.os,'killpg'),patch.object(supervisor,'group_inactive',
                side_effect=[subprocess.TimeoutExpired(['ps'],.01),(True,[])]):
            self.assertEqual(supervisor.stop_group(process,grace=.2),0)
        self.assertTrue(process.group_cleanup['reaped'])
        self.assertEqual(process.group_cleanup['failures'],[])
        self.assertEqual(len(process.group_cleanup['inspections']),1)


class ReviewedCppcheckLocations(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(); self.addCleanup(temporary.cleanup)
        root = Path(temporary.name); self.old, self.new = root/'old', root/'new'
        for tree in (self.old, self.new):
            (tree/'src').mkdir(parents=True)
        def contents(first, second):
            lines = ['filler']*45
            lines[first-1], lines[second-1] = 'first context', 'second context'
            return '\n'.join(lines)+'\n'
        (self.old/'src/a.cpp').write_text(contents(10,20))
        (self.new/'src/a.cpp').write_text(contents(30,20))

    def comparison(self, repeated=False, strings=False):
        from evidence_analysis import finding_key
        def finding(first, second):
            return dict(id='warning',msg='same diagnostic',locations=[
                dict(file='src/a.cpp',line=str(first) if strings else first,column='1' if strings else 1,info='first'),
                dict(file='src/a.cpp',line=str(second) if strings else second,column='1' if strings else 1,info='first' if repeated else 'second')])
        def manifest(finding):
            return dict(tool='cppcheck',expected_sources=['src/a.cpp'],completed_sources=['src/a.cpp'],
                canaries=[dict(passed=True)],versions=[dict(stdout='same cppcheck',exit_code=0)],
                results=[dict(status='findings',completed_sources=['src/a.cpp'],findings=[finding])],findings=1,passed=False)
        old, new = finding(10,20), finding(30,20)
        old_key, new_key = finding_key('cppcheck',old,self.old), finding_key('cppcheck',new,self.new)
        mappings=[]
        for previous in old_key[2]:
            current=next(location for location in new_key[2] if int(location[1]) == (30 if int(previous[1])==10 else 20))
            mappings.append(dict(old=list(previous),new=list(current),context='first context' if int(previous[1])==10 else 'second context'))
        return manifest(old),manifest(new),dict(old=old_key,new=new_key,rationale='reviewed same-location identities',location_contexts=mappings)

    def compare(self, a, b, entry):
        from evidence_analysis import compare_analysis
        return compare_analysis(a,b,self.old,self.new,['src/a.cpp'],relocations=[entry])

    def test_same_file_locations_can_reorder_without_changing_identity(self):
        for repeated,strings in ((False,False),(True,False),(False,True),(True,True)):
            with self.subTest(repeated=repeated,strings=strings):
                a,b,entry=self.comparison(repeated,strings)
                try:
                    result=self.compare(a,b,entry)
                except ValueError as error:
                    self.fail('Valid reordered Cppcheck relocation was rejected: '+str(error))
                self.assertTrue(result['equal'])
                self.assertEqual(result['reviewed_relocations'],1)

    def test_every_location_requires_unique_same_identity_context(self):
        import copy
        a,b,entry=self.comparison()
        for change in ('missing','duplicate','context','file','info','repeated'):
            with self.subTest(change=change):
                invalid=copy.deepcopy(entry)
                if change=='missing':invalid['location_contexts'].pop()
                elif change=='duplicate':invalid['location_contexts'][1]=copy.deepcopy(invalid['location_contexts'][0])
                elif change=='context':invalid['location_contexts'][1]['context']='different'
                elif change=='file':invalid['location_contexts'][1]['new'][0]='src/other.cpp'
                elif change=='info':invalid['location_contexts'][1]['new'][3]='unrelated'
                else:invalid['location_contexts'].append(copy.deepcopy(invalid['location_contexts'][0]))
                with self.assertRaisesRegex(ValueError,'relocation'):
                    self.compare(a,b,invalid)

    def test_same_context_cannot_hide_message_or_location_multiplicity_changes(self):
        import copy
        a,b,entry=self.comparison()
        for message in (True,False):
            changed=copy.deepcopy(b)
            if message:changed['results'][0]['findings'][0]['msg']='new diagnostic'
            else:changed['results'][0]['findings'][0]['locations'].append(copy.deepcopy(changed['results'][0]['findings'][0]['locations'][0]))
            with self.assertRaisesRegex(ValueError,'relocation'):
                self.compare(a,changed,entry)

    def test_duplicate_coordinate_occurrences_match_with_exact_multiplicity(self):
        from evidence_analysis import finding_key, cppcheck_contexts_match
        old=dict(id='warning',msg='same',locations=[dict(file='src/a.cpp',line=10,column=1,info='same')]*2)
        new=dict(id='warning',msg='same',locations=[dict(file='src/a.cpp',line=30,column=1,info='same')]*2)
        a,b=finding_key('cppcheck',old,self.old),finding_key('cppcheck',new,self.new)
        entry=dict(location_contexts=[dict(old=list(x),new=list(y),context='first context') for x,y in zip(a[2],b[2])])
        self.assertTrue(cppcheck_contexts_match(entry,a,b,self.old,self.new), 'Duplicate occurrences lost multiplicity')
        entry['location_contexts'].pop()
        self.assertFalse(cppcheck_contexts_match(entry,a,b,self.old,self.new))


class SanitizerIsolation(unittest.TestCase):
    def payload(self):
        xml='<testsuites tests="2"><testsuite name="Suite"><testcase name="A"/><testcase name="DISABLED_Probe"/></testsuite></testsuites>'
        result='<testsuites tests="1"><testsuite name="Suite"><testcase classname="Suite" name="A" status="run"/></testsuite></testsuites>'
        return dict(binary='/original/opaque/binary',binary_sha256='a'*64,
                    inventory_argv=['/original/opaque/binary','--gtest_list_tests','--gtest_output=xml:/opaque/list.xml'],
                    inventory_xml=xml,enabled=1,disabled=['Suite.DISABLED_Probe'],skipped=[],failures=0,
                    results=[dict(test='Suite.A',exit=0,xml=result,argv=['/original/opaque/binary','--gtest_filter=Suite.A','--gtest_output=xml:/opaque/result.xml'])])

    def test_captured_launcher_binds_its_report_to_the_declared_executable(self):
        import hashlib
        import os

        import gtest_isolation as isolated
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);binary=root/'binary';binary.write_bytes(b'executable identity');report=root/'report.json'
            value=self.payload();value.update(binary=str(binary),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
            value['inventory_argv'][0]=str(binary);value['results'][0]['argv'][0]=str(binary)
            context=dict(schema_version=5,campaign_id='campaign',capture_run_id='run',check_id='asan-ctest',root=str(root),
                         production_hashes={},validation_hashes={},declared_artifacts=[dict(path=str(report),kind='gtest-isolation',inputs=['binary'])])
            argv=['launcher','--report',str(report),'--inputs',json.dumps(['binary']),str(binary)]
            with patch.object(sys,'argv',argv),patch.dict(os.environ,CODET_EVIDENCE_CONTEXT=json.dumps(context)),patch.object(isolated,'run',return_value=value):
                self.assertEqual(isolated.main(),0)
            envelope=json.loads(report.read_text())
            self.assertEqual(envelope['capture_run_id'],'run')
            self.assertEqual(envelope['input_hashes'],{'binary':value['binary_sha256']})
            self.assertEqual(isolated.verify_payload(envelope['payload'],envelope['input_hashes'])['failures'],0)

    def test_executable_binding_is_checked_without_resolving_host_paths(self):
        from gtest_isolation import verify_payload
        value=self.payload();value.update(binary='/original/opaque/binary',binary_sha256='a'*64)
        verify_payload(value,{'/original/opaque/binary':'a'*64})
        for hashes in ({},{'/original/opaque/binary':'b'*64}):
            with self.assertRaisesRegex(ValueError,'executable binding mismatch'):verify_payload(value,hashes)
        value.pop('binary_sha256')
        with self.assertRaisesRegex(ValueError,'executable binding mismatch'):verify_payload(value,{})

    def test_full_coverage_and_normal_disabled_policy(self):
        from gtest_isolation import verify_payload
        self.assertEqual(verify_payload(self.payload()),dict(tests=1,disabled=1,skipped=0,failures=0))

    def test_missing_duplicate_and_substituted_tests_are_rejected(self):
        import copy

        from gtest_isolation import verify_payload
        for change in ('missing','duplicate','substitute'):
            with self.subTest(change=change):
                value=copy.deepcopy(self.payload())
                if change=='missing':value['results']=[]
                elif change=='duplicate':value['results']*=2
                else:
                    value['results'][0]['test']='Suite.Other'
                    value['results'][0]['argv'][1]='--gtest_filter=Suite.Other'
                    value['results'][0]['xml']=value['results'][0]['xml'].replace('name="A"','name="Other"')
                with self.assertRaisesRegex(ValueError,'coverage mismatch'):
                    verify_payload(value)

    def test_malformed_xml_and_inventory_counts_are_rejected(self):
        from gtest_isolation import inventory, verify_payload
        for raw in ('<invalid', '<testsuites tests="0"/>', '<testsuites tests="not-a-number"/>'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):inventory(raw)
        value=self.payload();value['results'][0]['xml']='<invalid'
        with self.assertRaisesRegex(ValueError,'Malformed GoogleTest XML'):verify_payload(value)

    def test_status_exit_xml_and_skip_accounting_are_checked(self):
        import copy

        from gtest_isolation import verify_payload
        for change in ('status','exit','name','skip','inventory','command'):
            with self.subTest(change=change):
                value=copy.deepcopy(self.payload());result=value['results'][0]
                if change=='status':result['xml']=result['xml'].replace('status="run"','status="notrun"')
                elif change=='exit':result['exit']=1
                elif change=='name':result['xml']=result['xml'].replace('name="A"','name="Other"')
                elif change=='skip':value['skipped']=['Suite.A']
                elif change=='command':result['argv'][1]='--gtest_filter=Suite.Other'
                else:value['enabled']=2
                with self.assertRaises(ValueError):verify_payload(value)

    def test_multiple_assertions_count_one_failed_case_and_process_exit_is_independent(self):
        from gtest_isolation import verify_payload
        value=self.payload();value.update(payload_version=2,failures=1)
        record=value['results'][0];record['exit']=1
        record['xml']='<testsuites tests="1" failures="1"><testsuite name="Suite"><testcase classname="Suite" name="A" status="run"><failure/><failure/></testcase></testsuite></testsuites>'
        self.assertEqual(verify_payload(value)['failures'],1)
        record['xml']=self.payload()['results'][0]['xml'];record['exit']=23
        self.assertEqual(verify_payload(value)['failures'],1)

    def test_missing_xml_preserves_exit_and_later_cases_are_checkpointed(self):
        import gtest_isolation as isolated
        with tempfile.TemporaryDirectory() as folder:
            binary=Path(folder)/'binary';binary.write_bytes(b'identity')
            checkpoints=[]
            def perform(argv, **kwargs):
                xml=Path(argv[-1].split('xml:',1)[1])
                if '--gtest_list_tests' in argv:
                    xml.write_text('<testsuites tests="2"><testsuite name="Suite"><testcase name="A"/><testcase name="B"/></testsuite></testsuites>')
                    return subprocess.CompletedProcess(argv,0,'','')
                if '--gtest_filter=Suite.A' in argv:
                    return subprocess.CompletedProcess(argv,7,'process output','process error')
                xml.write_text('<testsuites tests="1"><testsuite name="Suite"><testcase classname="Suite" name="B" status="run"/></testsuite></testsuites>')
                return subprocess.CompletedProcess(argv,23,'leak output','leak details')
            with patch.object(isolated,'supervised_run',side_effect=perform):
                result=isolated.run(binary,lambda value:checkpoints.append(len(value['results'])))
            self.assertEqual(checkpoints,[1,2])
            self.assertEqual([r['exit'] for r in result['results']],[7,23])
            self.assertEqual(result['failures'],2)
            self.assertIn('error',result['results'][0])
            self.assertEqual(isolated.verify_payload(result)['failures'],2)

    def test_standalone_report_roots_allow_repeat_runs_without_replacement(self):
        import gtest_isolation as isolated
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);binary=root/'binary';binary.write_bytes(b'identity')
            with patch.object(isolated,'run',side_effect=lambda *args:self.payload()):
                for _ in range(2):
                    with patch.object(sys,'argv',['launcher','--report-root',str(root/'reports'),str(binary)]):
                        self.assertEqual(isolated.main(),0)
            self.assertEqual(len(list((root/'reports').glob('run-*/report.json'))),2)


class HashMetricScope(unittest.TestCase):
    def test_tooling_commands_and_memory_have_distinct_verified_scopes(self):
        from evidence_benchmarks import verify_tooling_metrics
        row=dict(harness='tooling',workload='supervision',metric_version=2,files=0,owned_processes=5,
            check_output_calls=5,git_commands=0,process_inspection_commands=5,
            retained_traced_blocks=1,retained_traced_bytes=10,peak_traced_bytes=20)
        verify_tooling_metrics(row)
        for change in ('missing','classification','git','memory','version','ambiguous','files','owned'):
            with self.subTest(change=change):
                value=dict(row)
                if change=='missing':value.pop('process_inspection_commands')
                elif change=='classification':value['check_output_calls']=7
                elif change=='git':value.update(check_output_calls=6,git_commands=1)
                elif change=='memory':value['peak_traced_bytes']=1
                elif change=='ambiguous':value['allocations']=1
                elif change=='files':value['files']=1
                elif change=='owned':value['owned_processes']=0
                else:value['metric_version']=3
                with self.assertRaisesRegex(ValueError,'[Tt]ooling|Supervision'):
                    verify_tooling_metrics(value)
        verify_tooling_metrics(dict(harness='tooling',git_commands=5)) # Historical, not relabeled.

    def payload(self):
        from evidence_hash_experiment import METRIC_SCOPE, ROLES
        rows, summary, profiles = [], [], []
        for number in (1,2):
            for role in ROLES:
                for variant,value in (('baseline',2),('changed',1)):
                    metrics=dict(milliseconds=value,files=1,peak_traced_bytes=20*value,
                        retained_traced_bytes=10*value,retained_traced_blocks=value,
                        explicit_stat_calls=value,direntry_is_file_calls=value,direntry_is_dir_calls=value)
                    group=dict(set=number,workload=role,harness='hash-snapshot',variant=variant)
                    summary.append(dict(group,**metrics))
                    for sample in range(6):
                        rows.append(dict(group,**metrics,sample=sample,warmup=sample==0,identity='same'))
                        functions=[dict(name=name,calls=value) for name in
                            ('<built-in method posix.stat>',"<method 'is_file' of 'posix.DirEntry' objects>","<method 'is_dir' of 'posix.DirEntry' objects>")]
                        profiles.append(dict(set=number,sample=sample,variant=variant,role=role,functions=functions,
                            positive_controls=dict(explicit_stat_calls=1,direntry_is_file_calls=1,direntry_is_dir_calls=1,live_traced_bytes=4096)))
        return dict(metrics_version=2,metric_scope=METRIC_SCOPE,rows=rows,summary=summary,profiles=profiles,
            code_hashes={v:{n:'0'*64 for n in ('evidence_inputs.py','verified_sources.py')} for v in ('baseline','changed')},latency_gate_passed=True)

    def test_v2_labels_counters_without_claiming_total_allocation_work(self):
        from evidence_hash_experiment import verify_experiment
        payload=self.payload()
        self.assertTrue(verify_experiment(payload))
        self.assertNotIn('work_gate_passed',payload)
        self.assertNotIn('allocations',payload['rows'][0])
        self.assertNotIn('metadata_queries',payload['rows'][0])

    def test_missing_versions_bad_controls_and_ambiguous_names_are_rejected(self):
        import copy

        from evidence_hash_experiment import verify_experiment
        for change in ('missing-version','future-version','control','count','scope','old-claim'):
            with self.subTest(change=change):
                value=copy.deepcopy(self.payload())
                if change=='missing-version':value.pop('metrics_version')
                elif change=='future-version':value['metrics_version']=3
                elif change=='control':value['profiles'][0]['positive_controls']['direntry_is_file_calls']=0
                elif change=='count':value['profiles'][0]['functions'][0]['calls']=0
                elif change=='scope':value['metric_scope']={}
                else:value['work_gate_passed']=True
                with self.assertRaisesRegex(ValueError,'[Hh]ash'):
                    verify_experiment(value)

    def test_production_program_observes_call_and_live_memory_controls(self):
        from evidence_hash_experiment import PROGRAM
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'src').mkdir();(root/'src/a.cpp').write_text('source')
            result=subprocess.run([sys.executable,'-c',PROGRAM,str(Path(__file__).resolve().parents[1]/'scripts'),str(root),'production'],
                check=True,text=True,capture_output=True,timeout=10)
            value=json.loads(result.stdout)
            for name in ('explicit_stat_calls','direntry_is_file_calls','direntry_is_dir_calls'):
                self.assertEqual(value['positive_controls'][name],1)
            self.assertGreaterEqual(value['positive_controls']['live_traced_bytes'],4096)
            self.assertGreaterEqual(value['peak_traced_bytes'],value['retained_traced_bytes'])
            self.assertGreater(value['retained_traced_blocks'],0)

if __name__ == "__main__":
    unittest.main()

class EvidenceV4(ValidationEvidence):
    def test_string_vector_is_rejected_at_load(self):
        self.checks[0]['argv'] = 'python3'
        self.setup_manifest()
        with self.assertRaisesRegex(ValueError, 'argument vector'):
            evidence.load_manifest(self.manifest)

    def test_cwd_and_relative_executable_are_manifest_root_relative(self):
        work = self.root / 'space work'
        work.mkdir()
        program = work / 'run.sh'
        program.write_text('#!/bin/sh\npwd\n')
        program.chmod(0o700)
        self.checks[0].update(cwd='space work', argv=['./run.sh'])
        result = self.capture()['test']
        self.assertEqual(result['exit'], 0)
        self.assertEqual(result['cwd'], str(work.resolve()))
        self.assertEqual(result['executable']['path'], str(work.resolve() / program.name))
        self.publish()

    def test_environment_is_exact_and_does_not_inherit_shell(self):
        import os
        with patch.dict(os.environ, {'CPATH': '/undeclared', 'LD_PRELOAD': '/undeclared', 'ASAN_OPTIONS': 'bad'}):
            record = self.capture()['test']
        self.assertEqual(set(record['effective_environment']), {'PATH', 'CODET_EVIDENCE_CONTEXT'})
        self.publish()

    def test_ruff_is_validation_input_and_finder_metadata_is_ignored(self):
        (self.root / 'ruff.toml').write_text('line-length = 100')
        self.capture()
        (self.root / 'src/.DS_Store').write_text('finder')
        self.publish()
        (self.root / 'ruff.toml').write_text('line-length = 80')
        self.rejected('Stale or mismatched')

    def test_preflight_timeout_is_artifact_error_and_independent_check_runs(self):
        self.checks += [dict(id='dependent', argv=[sys.executable, '-c', 'pass'], requires=['test']),
                        dict(id='independent', argv=[sys.executable, '-c', 'pass'])]
        original = evidence.source_inputs
        calls = 0
        def once(*args):
            nonlocal calls
            calls += 1
            if calls == 1:
                raise subprocess.TimeoutExpired(['git', 'ls-tree'], 30)
            return original(*args)
        with patch.object(evidence, 'source_inputs', side_effect=once):
            results = self.capture()
        self.assertEqual(results['test']['outcome'], 'artifact-error')
        self.assertEqual(results['dependent']['outcome'], 'blocked')
        self.assertEqual(results['independent']['outcome'], 'passed')

    def test_audit_lookup_validation_precedes_reads(self):
        from evidence_bundle import safe_member, valid_digest
        for name in ('../outside', '/outside', 'capture/../outside', 'capture\\outside'):
            with self.assertRaisesRegex(ValueError, 'lookup path'):
                safe_member(self.root, name)
        (self.root / 'alias').symlink_to(self.root / 'src', target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            safe_member(self.root, 'alias/core.cpp')
        with self.assertRaisesRegex(ValueError, 'digest'):
            valid_digest('../not-a-digest')

    def test_python_switch_operands_do_not_hide_analyzers(self):
        from evidence_processes import blocking_processes
        for flags in (['-X', 'utf8'], ['-Xutf8'], ['-OO'], ['-W', 'ignore'], ['-uB'], ['-OOXutf8'], ['-uWignore'], ['-OX', 'utf8'], ['--']):
            self.assertTrue(blocking_processes([dict(executable='python3', argv=['python3'] + flags + ['/path with spaces/analyze-modern.py'])]))
        self.assertFalse(blocking_processes([dict(executable='python3', argv=['python3', '-c', 'analyze-modern.py'])]))

    def test_analyzers_require_versions_and_their_own_source_sets(self):
        from evidence_analysis import compare_analysis
        helper = ComparisonGuards()
        baseline = helper.analysis(Path('/baseline'))
        changed = helper.analysis(Path('/changed'))
        changed['expected_sources'] = changed['completed_sources'] = ['/changed/src/new.cpp']
        changed['results'][0]['source'] = '/changed/src/new.cpp'
        changed['results'][0]['completed_sources'] = ['/changed/src/new.cpp']
        self.assertTrue(compare_analysis(baseline, changed, Path('/baseline'), Path('/changed'), ['src/a.cpp'], ['src/new.cpp'])['equal'])
        baseline['versions'] = []
        changed['versions'] = []
        with self.assertRaisesRegex(ValueError, 'version'):
            compare_analysis(baseline, changed, Path('/baseline'), Path('/changed'), ['src/a.cpp'], ['src/new.cpp'])

    def test_relocations_require_full_identity_and_real_unchanged_source_context(self):
        from evidence_analysis import compare_analysis, finding_key
        old = self.root / 'old'
        new = self.root / 'new'
        for path, text in ((old, 'original\n'), (new, '// heading\noriginal\n')):
            (path / 'src').mkdir(parents=True)
            (path / 'src/a.cpp').write_text(text)
        helper = ComparisonGuards()
        a, b = helper.analysis(old, 'clang-tidy'), helper.analysis(new, 'clang-tidy')
        for manifest, root, offset in ((a, old, 0), (b, new, 11)):
            f = dict(DiagnosticName='known', DiagnosticMessage=dict(FilePath=str(root / 'src/a.cpp'), FileOffset=offset, Message='known diagnostic'))
            manifest.update(passed=False, findings=1)
            manifest['results'][0].update(status='findings', findings=[f])
        entry = dict(old=finding_key('clang-tidy', a['results'][0]['findings'][0], old),
                     new=finding_key('clang-tidy', b['results'][0]['findings'][0], new),
                     context='original', rationale='Unchanged diagnostic statement shifted by an inserted comment.')
        self.assertFalse(compare_analysis(a, b, old, new, ['src/a.cpp'])['equal'])
        self.assertTrue(compare_analysis(a, b, old, new, ['src/a.cpp'], relocations=[entry])['equal'])
        for entries in ([dict(entry, context='invented')], [entry, entry]):
            with self.assertRaisesRegex(ValueError, 'relocation'):
                compare_analysis(a, b, old, new, ['src/a.cpp'], relocations=entries)

    def test_managed_campaign_cannot_reuse_a_directory_without_config(self):
        import evidence_campaign as campaign
        folder = self.root / 'previous'
        folder.mkdir()
        (folder / 'CMakeCache.txt').write_text('leftover')
        with self.assertRaisesRegex(ValueError, 'directory already exists'):
            campaign.prepare(self.root, folder)

    def test_atomic_bundle_install_refuses_existing_empty_directory(self):
        from evidence_bundle import rename_exclusive
        source, target = self.root / 'source', self.root / 'target'
        source.mkdir()
        target.mkdir()
        (source / 'report').write_text('new')
        with self.assertRaises(OSError):
            rename_exclusive(source, target)
        self.assertTrue(source.is_dir())
        self.assertEqual(list(target.iterdir()), [])

    def test_multiple_prior_attempts_keep_their_original_capture_files(self):
        from evidence_campaign import history_files
        folders = [self.root / 'attempt-one', self.root / 'attempt-two']
        for folder in folders:
            (folder / 'capture').mkdir(parents=True)
            for name in ('config.json', 'manifest.json', 'capture-driver.log', 'capture/run.json'):
                (folder / name).write_text('original ' + folder.name + ' ' + name)
        paths = history_files(folders)
        self.assertEqual(len(paths), 8)
        self.assertEqual({p.parent for p in paths if p.name == 'run.json'}, {f / 'capture' for f in folders})
        self.assertTrue(all(p.read_text().startswith('original ') for p in paths))
        (folders[1] / 'capture/run.json').unlink()
        with self.assertRaisesRegex(ValueError, 'Prior attempt metadata'):
            history_files(folders)

    def test_tooling_benchmark_uses_repository_identity_for_nested_archives(self):
        import shutil
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True, timeout=30)
        subprocess.run(['git', '-C', str(self.root), 'add', 'src', 'CMakeLists.txt'], check=True, timeout=30)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Evidence Test',
                        '-c', 'user.email=evidence@example.invalid', 'commit', '-qm', 'fixture'], check=True, timeout=30)
        ref = subprocess.check_output(['git', '-C', str(self.root), 'rev-parse', 'HEAD'], text=True, timeout=30).strip()
        archive = self.root / 'build/baseline-src'
        shutil.copytree(self.root / 'src', archive / 'src')
        shutil.copyfile(self.root / 'CMakeLists.txt', archive / 'CMakeLists.txt')
        output = self.root / 'measurement.json'
        harness = Path(__file__).resolve().parents[1] / 'scripts/evidence_tool_bench.py'
        subprocess.run([sys.executable, str(harness), str(archive), ref, 'git-inspection', str(output), str(self.root)],
                       check=True, timeout=30)
        result = json.loads(output.read_text())
        self.assertEqual(result['files'], 2)
        self.assertEqual(result['git_commands'], 3)

    def test_v5_audit_rejects_jointly_changed_cwd_fields(self):
        from evidence_bundle import publish_audit_bundle, verify_bundle
        self.capture()
        bundle = self.root / 'bundle'
        publish_audit_bundle(self.manifest, self.folder, bundle)
        record_path = bundle / 'capture/test.json'
        record = json.loads(record_path.read_text())
        report_path = bundle / 'report.json'
        report = json.loads(report_path.read_text())
        for value in (record, report['validation'][0]):
            value['cwd'] = value['canonical_cwd'] = '/unrelated/nonexistent/check-folder'
        evidence.atomic_json(record_path, record)
        evidence.atomic_json(report_path, report)
        inventory = json.loads((bundle / 'inventory.json').read_text())
        for name in ('capture/test.json', 'report.json'):
            inventory[name] = evidence.digest(bundle / name)
        evidence.atomic_json(bundle / 'inventory.json', inventory)
        with self.assertRaisesRegex(ValueError, 'Working-directory provenance'):
            verify_bundle(bundle)

    def test_v5_resolution_chains_are_offline_and_rechecked(self):
        from evidence_paths import verify_working_provenance, working_provenance
        target = self.root / 'target'
        target.mkdir()
        link = self.root / 'link'
        link.symlink_to('target')
        manifest = dict(root=str(self.root))
        provenance = working_provenance(manifest, dict(cwd='link'))
        run = dict(canonical_root=str(self.root.resolve()), root_resolution=provenance['root'])
        record = dict(path_before=provenance, path_after=provenance, cwd_spec='link',
                      cwd=str(target.resolve()), canonical_cwd=str(target.resolve()))
        link.unlink()  # Auditing must not query the original machine's links.
        self.assertEqual(verify_working_provenance(manifest, run, record), str(target.resolve()))
        record['cwd_spec'] = 'other'
        with self.assertRaisesRegex(ValueError, 'specification'):
            verify_working_provenance(manifest, run, record)

    def test_v5_relocations_cannot_substitute_another_finding(self):
        from evidence_analysis import compare_analysis, finding_key
        old, new = self.root / 'old', self.root / 'new'
        for root in (old, new):
            (root / 'src').mkdir(parents=True)
            (root / 'src/a.cpp').write_text('same statement\n')
        helper = ComparisonGuards()
        a, b = helper.analysis(old, 'clang-tidy'), helper.analysis(new, 'clang-tidy')
        for manifest, root, name in ((a, old, 'removed-rule'), (b, new, 'new-rule')):
            finding = dict(DiagnosticName=name, DiagnosticMessage=dict(FilePath=str(root / 'src/a.cpp'), FileOffset=0, Message=name))
            manifest.update(passed=False, findings=1)
            manifest['results'][0].update(status='findings', findings=[finding])
        entry = dict(old=finding_key('clang-tidy', a['results'][0]['findings'][0], old),
                     new=finding_key('clang-tidy', b['results'][0]['findings'][0], new),
                     context='same statement', rationale='An unrelated finding must never cancel another.')
        with self.assertRaisesRegex(ValueError, 'relocation'):
            compare_analysis(a, b, old, new, ['src/a.cpp'], relocations=[entry])

    def test_v5_dependency_mutation_is_retained_even_if_restored(self):
        dep = self.root / '.deps/json'
        dep.mkdir(parents=True)
        header = dep / 'header.hpp'
        header.write_text('before')
        self.checks[0].update(directory_inputs=['.deps/json'], argv=[sys.executable, '-c',
            'from pathlib import Path;Path(".deps/json/header.hpp").write_text("during")'])
        result = self.capture()['test']
        self.assertNotEqual(result['external_inputs_before'], result['external_inputs_after'])
        header.write_text('before')
        self.rejected('External build inputs changed')


class PortableV5(unittest.TestCase):
    def test_public_ci_pagination_and_identity(self):
        from evidence_ci import read_public_ci
        run = dict(id=42, repository=dict(full_name='owner/repo'), head_sha='head', run_attempt=2,
                   status='completed', conclusion='success')
        calls = []
        def request(url):
            calls.append(url)
            if '/jobs?' not in url:
                return run, dict(url=url, status=200)
            page = int(url.rsplit('=', 1)[1])
            jobs = [dict(id=n, run_id=42, head_sha='head', status='completed', conclusion='success')
                    for n in (range(1,101) if page == 1 else [101])]
            return dict(total_count=101, jobs=jobs), dict(url=url, status=200)
        result = read_public_ci('owner/repo', 42, 'head', request=request)
        self.assertEqual(len(result['jobs']), 101)
        self.assertEqual(len(calls), 4)
        with self.assertRaisesRegex(ValueError, 'identity/head'):
            read_public_ci('owner/repo', 42, 'other', request=request)
        count = 0
        def changed(url):
            nonlocal count
            value, provenance = request(url)
            if '/jobs?' not in url:
                count += 1
                value = dict(value, run_attempt=count)
            return value, provenance
        with self.assertRaisesRegex(ValueError, 'attempt changed'):
            read_public_ci('owner/repo', 42, 'head', request=changed)

    def test_public_ci_unavailable_and_poll_deadlines(self):
        from evidence_ci import read_public_ci
        with self.assertRaisesRegex(ValueError, 'request unavailable'):
            read_public_ci('owner/repo', 42, 'head', request=lambda url: (_ for _ in ()).throw(TimeoutError('deadline')))
        now = [0]
        sleeps = []
        def pause(seconds):
            sleeps.append(seconds)
            now[0] += seconds
        def request(url):
            return dict(id=42, repository=dict(full_name='owner/repo'), head_sha='head', run_attempt=1, status='queued'), dict(url=url)
        with self.assertRaisesRegex(TimeoutError, 'deadline|did not complete'):
            read_public_ci('owner/repo', 42, 'head', request=request, pause=pause, clock=lambda: now[0])
        self.assertTrue(sleeps)
        self.assertLessEqual(max(sleeps), 60)

    def test_python39_discovery_and_override(self):
        import evidence_campaign as campaign
        with patch.object(campaign.shutil, 'which', side_effect=lambda name: '/tools/' + name), patch.object(campaign, 'command', side_effect=['3.12', '3.9']):
            self.assertEqual(campaign.discover_python39(preferred='python3'), '/tools/python3.9')
        with patch.object(campaign.shutil, 'which', return_value=None):
            self.assertEqual(campaign.discover_python39(), 'python3.9-unavailable')
            self.assertEqual(campaign.discover_python39('/explicit/python'), '/explicit/python')

    def test_missing_ca_is_rejected_before_campaign_creation(self):
        import evidence_campaign as campaign
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            with self.assertRaisesRegex(ValueError, 'CA bundle does not exist'):
                campaign.prepare(root, root / 'new', ca_bundle=root / 'missing')
            self.assertFalse((root / 'new').exists())

    def test_checked_clang_file_resolution(self):
        from clang_plist import resolve_files
        self.assertEqual(resolve_files([dict(file=0)], ['src/a.cpp']), [dict(file='src/a.cpp')])
        for index in (-1, 1, True):
            with self.assertRaisesRegex(ValueError, 'file index'):
                resolve_files(dict(file=index), ['src/a.cpp'])

    def test_supervisor_terminates_descendants_on_timeout(self):
        import os
        import time

        from process_supervision import run
        with tempfile.TemporaryDirectory() as folder:
            marker = Path(folder) / 'child-survived'
            program = 'import subprocess,sys,time;subprocess.Popen([sys.executable,"-c",' + repr('import time;from pathlib import Path;time.sleep(1);Path(' + repr(str(marker)) + ').write_text("survived")') + ']);time.sleep(60)'
            with self.assertRaises(subprocess.TimeoutExpired):
                run([sys.executable, '-c', program], timeout=.2, cleanup_grace=.1)
            time.sleep(1.1)
            self.assertFalse(marker.exists())
            self.assertGreater(os.getpid(), 0)

    def test_public_ci_rejects_rate_limits_and_failed_jobs(self):
        import io
        import urllib.error

        from evidence_ci import read_public_ci
        def denied(url):
            error = urllib.error.HTTPError(url, 403, 'rate limit', {}, io.BytesIO())
            error.close()
            raise error
        with self.assertRaisesRegex(ValueError, '403.*rate limit'):
            read_public_ci('owner/repo', 42, 'head', request=denied)
        def failed(url):
            run = dict(id=42, repository=dict(full_name='owner/repo'), head_sha='head', run_attempt=1,
                       status='completed', conclusion='success')
            jobs = dict(total_count=1, jobs=[dict(id=1, run_id=42, head_sha='head', status='completed', conclusion='failure')])
            return (jobs if '/jobs?' in url else run), dict(url=url)
        with self.assertRaisesRegex(ValueError, 'did not succeed'):
            read_public_ci('owner/repo', 42, 'head', request=failed)

    def test_historical_replacements_require_explicit_counts(self):
        import ast
        path = Path(__file__).resolve().parents[1] / 'docs/validation/diagnostic-record-lifecycle-followup-prepare-candidate.py'
        tree = ast.parse(path.read_text())
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'checked_replace')
        scope = dict(transformations=[])
        exec(compile(ast.Module(body=[function], type_ignores=[]), str(path), 'exec'), scope)
        with self.assertRaisesRegex(ValueError, 'count mismatch'):
            scope['checked_replace']('two two', 'two', 'one')
        self.assertEqual(scope['checked_replace']('two two', 'two', 'one', expected=2), 'one one')

    def test_hash_snapshot_reuses_only_local_storage_and_preserves_selection(self):
        import hashlib
        import io

        from evidence_inputs import role_hashes, selected
        from verified_sources import file_hash, stream_hash
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            file = root / 'src/a.cpp'
            file.write_bytes(b'bytes\0' * 20000)
            (root / 'src/.DS_Store').write_text('Finder metadata')
            first = role_hashes(root, 'production')
            self.assertEqual(first, {'src/a.cpp': hashlib.sha256(file.read_bytes()).hexdigest()})
            file.write_text('changed after the logical snapshot')
            self.assertNotEqual(role_hashes(root, 'production'), first)
            self.assertEqual(file_hash(file, bytearray(65536)), hashlib.sha256(file.read_bytes()).hexdigest())
            self.assertEqual(stream_hash(io.BytesIO(b'bytes')), hashlib.sha256(b'bytes').hexdigest())
            self.assertFalse(selected('src/.DS_Store'))
            self.assertTrue(selected('prompts/a.md'))

    def test_hash_experiment_recomputes_gate_and_samples(self):
        from evidence_hash_experiment import ROLES, verify_experiment
        rows, summary = [], []
        for number in (1, 2):
            for role in ROLES:
                for variant, value in (('baseline', 2), ('changed', 1)):
                    metrics = dict(milliseconds=value, files=1, peak_bytes=value, allocations=value, metadata_queries=value)
                    group = dict(set=number, workload=role, harness='hash-snapshot', variant=variant)
                    summary.append(dict(group, **metrics))
                    for sample in range(6):
                        rows.append(dict(group, **metrics, sample=sample, warmup=sample == 0, identity='same'))
        payload = dict(rows=rows, summary=summary, latency_gate_passed=True, work_gate_passed=True)
        self.assertTrue(verify_experiment(payload))
        payload['work_gate_passed'] = False
        with self.assertRaisesRegex(ValueError, 'work acceptance'):
            verify_experiment(payload)

    def test_supervisor_preserves_signal_interruption_after_descendant_cleanup(self):
        import os
        import select
        import signal
        import time

        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / 'escaped'
            grandchild = 'import time,signal;from pathlib import Path;signal.signal(signal.SIGTERM,signal.SIG_IGN);print("grandchild-ready",flush=True);time.sleep(1);Path(' + repr(str(marker)) + ').write_text("escaped")'
            child = 'import subprocess,sys,time;p=subprocess.Popen([sys.executable,"-c",' + repr(grandchild) + '],stdout=subprocess.PIPE);p.stdout.readline();print("ready",flush=True);time.sleep(60)'
            script = 'from process_supervision import run\nimport sys\ntry:run([sys.executable,"-c",' + repr(child) + '],timeout=60,cleanup_grace=.1)\nexcept KeyboardInterrupt:raise SystemExit(17)'
            process = subprocess.Popen([sys.executable, '-c', script], stdout=subprocess.PIPE,
                                       env=dict(PATH='/bin:/usr/bin', PYTHONPATH=str(Path(__file__).resolve().parents[1] / 'scripts')))
            try:
                self.assertTrue(select.select([process.stdout], [], [], 5)[0])
                self.assertEqual(process.stdout.readline(), b'ready\n')
                process.send_signal(signal.SIGTERM)
                self.assertEqual(process.wait(timeout=5), 17)
                time.sleep(1.1)
                self.assertFalse(marker.exists())
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
                process.stdout.close()
            self.assertGreater(os.getpid(), 0)

    def test_real_popen_failure_keeps_launch_classification(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/a.cpp').write_text('source')
            (root / 'not-directory').write_text('file')
            manifest = root / 'manifest.json'
            evidence.atomic_json(manifest, dict(root=str(root), checks=[dict(id='bad-cwd', cwd='not-directory',
                argv=[sys.executable, '-c', 'print("must not execute")'])]))
            result = evidence.capture(manifest, root / 'capture')['bad-cwd']
            self.assertEqual(result['outcome'], 'launch-error')
            self.assertIsNone(result['exit'])
            self.assertIn('not-directory', result['error'])
