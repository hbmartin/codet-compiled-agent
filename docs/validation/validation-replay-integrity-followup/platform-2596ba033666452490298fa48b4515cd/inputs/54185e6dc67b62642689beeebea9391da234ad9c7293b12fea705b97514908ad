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
                     dict(executable="build/host/codet_tests", argv=["codet_tests", "--gtest_filter=x"])]
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
                          "from pathlib import Path\nwrite_artifact(Path('artifact.json'),'test',{'value':1},['src/core.cpp'])\n")
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
        with patch.object(evidence.os, "rename", side_effect=OSError("rename failed")), self.assertRaisesRegex(OSError, "rename failed"):
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
        return dict(tool=tool, versions=[], expected_sources=[str(root / "src/a.cpp")],
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
        with patch.object(processes.subprocess, "check_output", return_value="123 /usr/bin/python3\n"), patch.object(processes, "read_argv", side_effect=OSError(errno.ESRCH, "gone")):
            with self.assertRaisesRegex(ValueError, "no usable records"):
                processes.process_snapshot()
        with patch.object(processes.subprocess, "check_output", return_value="123 /usr/bin/python3\n"), patch.object(processes, "read_argv", side_effect=PermissionError(errno.EPERM, "denied")):
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
        script.write_text("import sys\nfrom pathlib import Path\nsys.path.insert(0," + repr(str(Path(evidence.__file__).parent)) + ")\nfrom validation_evidence import write_artifact\nimport json\npayload=json.loads(" + repr(json.dumps(dict(rows=rows, summary=summary))) + ")\nwrite_artifact(Path(sys.argv[1]),'measure',payload,['config.json'])\n")
        self.checks = [dict(id=n, argv=[sys.executable, str(script), n + ".json"], artifacts=[dict(path=n + ".json", kind="measure", inputs=["config.json"])]) for n in ("first", "second")]
        self.capture()
        target = self.root / "bundle"
        evidence.publish_bundle(self.manifest, self.folder, target)
        self.assertTrue(verify_bundle(target)["verified"])
        self.assertEqual(len(list((target / "measurements").rglob("samples.csv"))), 2)
        self.assertEqual(len(json.loads((target / "measurements.json").read_text())), 2)

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
        with patch.object(processes.subprocess, "check_output", return_value="123 clang-tidy-18\n"), patch.object(processes, "read_argv", return_value=["/bin/clang-tidy-18"]) as argv, patch.object(processes.os, "readlink", return_value="/bin/clang-tidy-18"):
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
        self.assertTrue(source_inputs(self.root, [dict(path="baseline", git_ref="HEAD")]))
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
        evidence.atomic_json(template, dict(baseline="HEAD", python39="unavailable-python39", builds=dict(debug="custom-debug", asan="custom-asan", tsan="custom-tsan", cross="custom-cross"), benchmark=dict(sets=3, measured_per_set=4)))
        manifest = campaign.prepare(self.root, Path("build/fresh"), check_python=sys.executable, analysis_python=sys.executable,
                                    template_path=template, clang_tidy=sys.executable, cppcheck=sys.executable)
        checks = json.loads(manifest.read_text())["checks"]
        self.assertIn("custom-debug", next(c for c in checks if c["id"] == "debug-configure")["argv"][4])
        config = json.loads((self.root / "build/fresh/config.json").read_text())
        self.assertEqual((config["sets"], config["measured"], config["python39"]), (3, 4, "unavailable-python39"))
        other = campaign.prepare(self.root, Path("build/other"), "HEAD", sys.executable, sys.executable, sys.executable,
                                 template_path=template, clang_tidy=sys.executable, cppcheck=sys.executable)
        self.assertEqual(json.loads((other.parent / "config.json").read_text())["python39"], sys.executable)


if __name__ == "__main__":
    unittest.main()
