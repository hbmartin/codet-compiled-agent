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
        from evidence_historical import PACKAGE
        carrier = root / PACKAGE; carrier.mkdir(parents=True)
        (carrier / 'audit-bundle.tar.gz').write_bytes(b'fixture archive')
        evidence.atomic_json(carrier / 'transport.json', dict(capture_run_id='fixture',
            archive=dict(path='audit-bundle.tar.gz', bytes=15, sha256=evidence.digest(carrier / 'audit-bundle.tar.gz')),
            expanded=dict(prefix='fixture', files=1, bytes=1, inventory_sha256='a'*64)))
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

    def historical_measurement_bundle(self, version=1, advisory=128):
        import statistics

        import evidence_bundle
        row = PreparationRecovery.row(preparation_metric_version=version, workload='preflight-checks',
            prepared_submissions=128, effective_content_bytes=640, advisory_budget_checks=advisory)
        if version == 1:
            del row['advisory_budget_checks']
        rows = [dict(row, set=number, sample=sample, variant=variant, warmup=sample == 0,
                     milliseconds=float(sample + 1)) for number in (1, 2) for variant in ('baseline', 'changed') for sample in range(6)]
        summaries = []
        for number in (1, 2):
            for variant in ('baseline', 'changed'):
                selected = [v for v in rows if v['set'] == number and v['variant'] == variant and not v['warmup']]
                fields = [k for k, value in selected[0].items() if type(value) in (int, float) and k not in ('set', 'sample')]
                summaries.append(dict(set=number, harness='preparation', workload='preflight-checks', variant=variant,
                    **{k: statistics.median(v[k] for v in selected) for k in fields}))
        script = self.root / 'producer.py'
        payload = dict(rows=rows, summary=summaries)
        script.write_text('import sys\nfrom pathlib import Path\nsys.path.insert(0,' + repr(str(Path(evidence.__file__).parent))
            + ')\nfrom validation_evidence import write_artifact\nimport json\nwrite_artifact(Path("measure.json"),"measure",json.loads('
            + repr(json.dumps(payload)) + '),["config.json"])\n')
        evidence.atomic_json(self.root / 'config.json', dict(cases=dict(preparation=['preflight-checks']), sets=2, measured=5))
        self.checks = [dict(id='measure', argv=[sys.executable, str(script)], artifacts=[dict(path='measure.json',kind='measure',inputs=['config.json'])])]
        self.capture()
        target = self.root / 'historical-bundle'
        # Simulate a bundle published by the then-current metric interpreter. The
        # actual frozen v1 campaign is also audited by a captured campaign check.
        verify = evidence_bundle.verify_samples
        with patch.object(evidence_bundle, 'verify_samples', side_effect=lambda *a, **kw: verify(*a, **dict(kw, historical=True))):
            evidence.publish_bundle(self.manifest, self.folder, target)
        return target

    def test_historical_bundle_audit_is_explicit_and_never_current_eligible(self):
        from evidence_bundle import verify_bundle
        target = self.historical_measurement_bundle()
        with self.assertRaisesRegex(ValueError, 'preparation measurement counters'):
            verify_bundle(target)
        try:
            value = verify_bundle(target, historical=True)
        except ValueError as error:
            self.fail('Historical metric interpretation was not exposed by the bundle auditor: ' + str(error))
        self.assertTrue(value['verified'], 'Historical metric interpretation was not exposed by the bundle auditor')
        self.assertTrue(value['historical'])
        self.assertFalse(value['eligible_current'], 'Historical auditing granted current campaign eligibility')
        with self.assertRaisesRegex(ValueError, 'preparation measurement counters'):
            evidence.publish_bundle(self.manifest, self.folder, self.root / 'cannot-publish-live')
        for wrong in (1, None, 'yes'):
            with self.assertRaisesRegex(ValueError, 'Historical audit mode'):
                verify_bundle(target, historical=wrong)

    def test_historical_cli_exit_and_publication_boundaries(self):
        from contextlib import redirect_stdout
        from io import StringIO
        target = self.historical_measurement_bundle()
        with patch.object(sys, 'argv', ['evidence', 'verify-bundle', str(target), '--historical']), redirect_stdout(StringIO()) as stream:
            self.assertEqual(evidence.main(), 0)
        value = json.loads(stream.getvalue())
        self.assertTrue(value['verified']); self.assertFalse(value['eligible_current'])
        with patch.object(sys, 'argv', ['evidence', 'verify-bundle', str(target)]), self.assertRaisesRegex(ValueError, 'preparation measurement counters'):
            evidence.main()
        with patch.object(sys, 'argv', ['evidence', 'bundle', str(self.manifest), str(self.folder), '--historical']), redirect_stdout(StringIO()):
            with self.assertRaises(SystemExit) as result:
                evidence.main()
        self.assertEqual(result.exception.code, 2)

    def test_historical_audit_keeps_provenance_sidecar_and_summary_checks(self):
        import shutil

        from evidence_bundle import verify_bundle
        target = self.historical_measurement_bundle()
        for fault in ('input', 'sidecar', 'summary', 'producer'):
            with self.subTest(fault=fault):
                changed = self.root / ('damaged-' + fault); shutil.copytree(target, changed)
                inventory_path = changed / 'inventory.json'
                inventory = json.loads(inventory_path.read_text())
                if fault == 'input':
                    path = changed / next(i['storage'] for i in json.loads((changed / 'inputs.json').read_text()) if i.get('storage'))
                    path.write_text('corrupt archived input')
                elif fault == 'sidecar':
                    path = next((changed / 'measurements').rglob('samples.csv')); path.write_text('corrupt sidecar')
                    inventory[str(path.relative_to(changed))] = evidence.digest(path)
                else:
                    path = changed / 'artifacts/measure/measure.json'; value = json.loads(path.read_text())
                    if fault == 'summary':
                        value['payload']['summary'][0]['milliseconds'] += 1
                    else:
                        value['check_id'] = 'another-producer'
                    evidence.atomic_json(path, value)
                    signature = evidence.digest(path)
                    inventory[str(path.relative_to(changed))] = signature
                    for record_path in (changed / 'capture/measure.json', changed / 'report.json'):
                        record = json.loads(record_path.read_text())
                        if record_path.name == 'report.json':
                            record['derived_artifacts'][0] = value
                            record['validation'][0]['artifacts'][0]['sha256'] = signature
                        else:
                            record['artifacts'][0]['sha256'] = signature
                        evidence.atomic_json(record_path, record)
                        inventory[str(record_path.relative_to(changed))] = evidence.digest(record_path)
                evidence.atomic_json(inventory_path, inventory)
                with self.assertRaisesRegex(ValueError, 'inventory|sidecar|summary|producer'):
                    verify_bundle(changed, historical=True)

    def test_historical_carrier_expansion_is_checked_before_auditing(self):
        import tarfile

        from evidence_historical import audit_package
        bundle = self.historical_measurement_bundle()
        package = self.root / 'package'; package.mkdir()
        archive = package / 'audit-bundle.tar.gz'
        run = json.loads((bundle / 'capture/run.json').read_text())['id']
        members = sorted(p for p in bundle.rglob('*') if p.is_file())
        with tarfile.open(archive, 'w:gz') as stream:
            for path in members:
                stream.add(path, arcname=run + '/' + str(path.relative_to(bundle)), recursive=False)
        metadata = dict(capture_run_id=run, archive=dict(path=archive.name, sha256=evidence.digest(archive), bytes=archive.stat().st_size),
            expanded=dict(prefix=run, files=len(members), bytes=sum(p.stat().st_size for p in members), inventory_sha256=evidence.digest(bundle / 'inventory.json')))
        evidence.atomic_json(package / 'transport.json', metadata)
        result = audit_package(package, self.root / 'expanded')
        self.assertTrue(result['strict_rejected'])
        self.assertTrue(result['historical_result']['verified'])
        self.assertFalse(result['historical_result']['eligible_current'])
        with self.assertRaises(FileExistsError):
            audit_package(package, self.root / 'expanded')
        metadata['archive']['sha256'] = '0' * 64
        evidence.atomic_json(package / 'transport.json', metadata)
        with self.assertRaisesRegex(ValueError, 'carrier digest'):
            audit_package(package, self.root / 'wrong-digest')
        self.assertFalse((self.root / 'wrong-digest').exists())
        metadata['archive']['path'] = '../outside.tar.gz'
        evidence.atomic_json(package / 'transport.json', metadata)
        with self.assertRaisesRegex(ValueError, 'lookup path'):
            audit_package(package, self.root / 'outside-path')


    def carrier_fixture(self):
        import tarfile
        bundle = self.historical_measurement_bundle()
        package = self.root / 'carrier'; package.mkdir()
        archive = package / 'audit-bundle.tar.gz'
        run = json.loads((bundle / 'capture/run.json').read_text())['id']
        members = sorted(p for p in bundle.rglob('*') if p.is_file())
        with tarfile.open(archive, 'w:gz') as stream:
            for path in members:
                stream.add(path, arcname=run + '/' + str(path.relative_to(bundle)), recursive=False)
        metadata = dict(capture_run_id=run, archive=dict(path=archive.name, sha256=evidence.digest(archive), bytes=archive.stat().st_size),
            expanded=dict(prefix=run, files=len(members), bytes=sum(p.stat().st_size for p in members), inventory_sha256=evidence.digest(bundle / 'inventory.json')))
        evidence.atomic_json(package / 'transport.json', metadata)
        return package, metadata

    def test_historical_extraction_failure_is_transactional_and_retryable(self):
        import evidence_historical as historical
        package, metadata = self.carrier_fixture()
        destination = self.root / 'expanded'
        native = historical.shutil.copyfileobj
        copied = []
        error = RuntimeError('injected extraction failure')
        def fail_second(source, output):
            if copied:
                raise error
            copied.append(True); return native(source, output)
        for _attempt in range(2):
            copied.clear()
            with patch.object(historical.shutil, 'copyfileobj', fail_second):
                with self.assertRaises(RuntimeError) as failed:
                    historical.audit_package(package, destination)
            self.assertIs(failed.exception, error)
            self.assertFalse(destination.exists(), 'Failed historical extraction left a final tree')
            self.assertFalse(list(self.root.glob('.historical-*')), 'Failed historical staging was not released')
        for error in (ValueError('injected verification failure'), KeyboardInterrupt('injected interruption')):
            with patch.object(historical, 'verify_bundle', side_effect=error):
                with self.assertRaises(type(error)) as failed:
                    historical.audit_package(package, destination)
            self.assertIs(failed.exception, error)
            self.assertFalse(destination.exists())
            self.assertFalse(list(self.root.glob('.historical-*')))
        result = historical.audit_package(package, destination)
        self.assertTrue(result['historical_result']['verified'])
        self.assertTrue(destination.is_dir())

    @staticmethod
    def historical_signal_program():
        return r"""import importlib.util,json,os,signal,sys,time
from pathlib import Path
sys.path.insert(0,sys.argv[1]);sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('evidence_historical',sys.argv[2])
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
if sys.argv[6]=='ignored':signal.signal(signal.SIGINT,signal.SIG_IGN)
signal.signal(signal.SIGINT,signal.default_int_handler)
prior={s:signal.getsignal(s) for s in (signal.SIGINT,signal.SIGTERM)}
def gate(release=False):
 print('READY:'+str(os.getpid()),flush=True)
 deadline=time.monotonic()+5
 while time.monotonic()<deadline:
  if release:
   import select
   if select.select([sys.stdin],[],[],.05)[0]:
    line=sys.stdin.readline()
    if not line:raise RuntimeError('Historical signal fixture input closed')
    if line.strip()=='go':return
  else:time.sleep(.05)
 raise RuntimeError('Historical signal fixture deadline expired')
native_copy=h.shutil.copyfileobj
native_verify=h.verify_bundle
native_rename=h.rename_exclusive
native_cleanup=h.tempfile.TemporaryDirectory.cleanup
cleanup_gated=False
def copy(source,output):
 gate();return native_copy(source,output)
def verify(*args,**kwargs):
 if kwargs.get('historical'):gate()
 return native_verify(*args,**kwargs)
def rename(stage,destination):
 if sys.argv[5]=='rename':gate(True)
 native_rename(stage,destination)
 if sys.argv[5]=='published':gate(True)
def cleanup(temporary):
 global cleanup_gated
 if not cleanup_gated:cleanup_gated=True;gate(True)
 return native_cleanup(temporary)
if sys.argv[5]=='extraction':h.shutil.copyfileobj=copy
elif sys.argv[5]=='verification':h.verify_bundle=verify
elif sys.argv[5]=='cleanup':h.tempfile.TemporaryDirectory.cleanup=cleanup
else:h.rename_exclusive=rename
try:
 result=h.audit_package(sys.argv[3],sys.argv[4]);value=dict(result=result)
except BaseException as error:value=dict(error=type(error).__name__,message=str(error))
value.update(restored=all(signal.getsignal(s)==handler for s,handler in prior.items()),
 staging=[p.name for p in Path(sys.argv[4]).parent.glob('.historical-*') if p.name!='.historical-active'],
 final=Path(sys.argv[4]).exists())
print('REPORT:'+json.dumps(value),flush=True)
"""

    def historical_signal_case(self, phase, sig=None, ignored=False, parent_exits=False, after_ready=None):
        import os
        import select

        import evidence_historical as historical
        package, _ = self.carrier_fixture() if not (self.root / 'carrier').exists() else (self.root / 'carrier', None)
        code = self.historical_signal_program()
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            parent = Path(directory)
            sibling = parent / '.historical-active'; sibling.mkdir()
            (sibling / 'owner').write_text('another invocation')
            destination = parent / 'result'
            argv = [sys.executable, '-c', code, str(Path(evidence.__file__).parent),
                    str(historical.__file__), str(package), str(destination), phase, 'ignored' if ignored else 'normal']
            child_argv = tuple(argv)
            if parent_exits:
                driver = 'import json,subprocess,sys;subprocess.Popen(json.loads(sys.argv[1]),stdin=subprocess.DEVNULL,start_new_session=True)'
                argv = [sys.executable, '-c', driver, json.dumps(argv)]
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       text=True, start_new_session=True)
            child = None
            primary = None
            try:
                self.assertTrue(select.select([process.stdout], [], [], 15)[0], 'Historical signal fixture never reached owned staging')
                ready = process.stdout.readline().strip()
                self.assertTrue(ready.startswith('READY:'), ready)
                child = int(ready.split(':', 1)[1])
                if after_ready is not None:after_ready(child, child_argv)
                if sig is not None:
                    os.kill(child, sig)
                    if phase in ('rename', 'published', 'cleanup'):
                        process.stdin.write('go\n'); process.stdin.flush()
                stdout, stderr = process.communicate(timeout=10)
            except BaseException as error:
                primary = error
                raise
            finally:
                self.cleanup_historical_fixture(process, child, child_argv, primary)
            self.assertIn('REPORT:', stdout, 'Cooperative extraction signal skipped cleanup: ' + stderr)
            report = json.loads(stdout.split('REPORT:', 1)[1])
            self.assertTrue(report['restored'], 'Historical extraction retained its signal handler')
            self.assertFalse(report['staging'], 'Cooperative extraction signal retained owned staging')
            self.assertEqual((sibling / 'owner').read_text(), 'another invocation')
            if report['final']:
                with self.assertRaises(FileExistsError):historical.audit_package(package, destination)
            else:
                result = historical.audit_package(package, destination)
                self.assertTrue(result['historical_result']['verified'])
            return report

    @staticmethod
    def cleanup_historical_fixture(process, child, child_argv, primary):
        from process_supervision import stop_group
        failures = []
        try:
            if child is not None and child != process.pid:
                ReplayIntegrity.stop_historical_signal_child(child, child_argv)
        except BaseException as error:failures.append(error)
        try:
            if process.poll() is None:stop_group(process, grace=.2)
        except BaseException as error:failures.append(error)
        finally:
            for stream in (process.stdin, process.stdout, process.stderr):stream.close()
        if failures:
            if primary is None:raise failures[0]
            primary.fixture_cleanup = repr(failures[0])
            primary.fixture_cleanup_failures = [repr(error) for error in failures]

    @staticmethod
    def stop_historical_signal_child(child, argv):
        import errno
        import os
        import signal
        import time

        from evidence_processes import read_argv
        from process_supervision import group_inactive
        # Verify the exact unique child command before each signal. A vanished
        # or recycled PID never grants authority to signal another group.
        if child <= 0:raise ValueError("Historical fixture requires a positive owned PID")
        expected = tuple(argv)
        deadline = time.monotonic() + 2
        for action in (signal.SIGTERM, signal.SIGKILL):
            try:
                if group_inactive(child, timeout=.2)[0]:return
                if tuple(read_argv(child)[1:]) != expected[1:] or os.getpgid(child) != child:
                    raise RuntimeError('Historical fixture child identity changed')
                os.killpg(child, action)
            except (OSError, ValueError) as error:
                if isinstance(error, OSError) and error.errno in (errno.ENOENT, errno.ESRCH):return
                # Darwin EINVAL / Linux empty cmdline may race with exit. Only a
                # positive group observation establishes that the child vanished.
                try:inactive = group_inactive(child, timeout=.2)[0]
                except BaseException as inspection:
                    error.fixture_inspection = repr(inspection)
                    raise error
                if inactive:return
                raise
            until = min(deadline, time.monotonic() + (.2 if action == signal.SIGTERM else 1))
            while time.monotonic() < until:
                if group_inactive(child, timeout=.2)[0]:return
                time.sleep(.05)
        raise RuntimeError('Historical fixture child cleanup deadline expired')

    def test_historical_parent_exit_failure_cleans_owned_child(self):
        import os

        from process_supervision import group_inactive
        observed = []
        original = AssertionError('injected failure after orphan readiness')
        def fail(child, argv):
            observed.append((child, argv))
            raise original
        with self.assertRaises(AssertionError) as failed:
            self.historical_signal_case('extraction', parent_exits=True, after_ready=fail)
        self.assertIs(failed.exception, original)
        self.assertEqual(len(observed), 1)
        try:
            self.assertTrue(group_inactive(observed[0][0], timeout=1)[0], 'Exited parent left its signal child running')
        finally:
            if not group_inactive(observed[0][0], timeout=1)[0]:
                self.stop_historical_signal_child(*observed[0])
        self.assertNotEqual(observed[0][0], os.getpid())

    def test_child_cleanup_rejects_recycled_identity_and_handles_verified_exit(self):
        import errno
        import signal
        from types import SimpleNamespace
        expected = ('python', '-c', 'immutable unique child')
        with patch('os.killpg') as kill, patch('os.getpgid', return_value=123):
            with patch('process_supervision.group_inactive', return_value=(False, ['S'])), patch('evidence_processes.read_argv', return_value=['python','-c','another process']):
                with self.assertRaisesRegex(RuntimeError, 'identity changed'):self.stop_historical_signal_child(123, expected)
            self.assertEqual(kill.call_count, 0, 'Recycled child identity authorized a signal')
            for error in (OSError(errno.EINVAL, 'vanished'), ValueError('empty argv')):
                with patch('process_supervision.group_inactive', side_effect=[(False,['S']),(True,[])]), patch('evidence_processes.read_argv', side_effect=error):
                    self.stop_historical_signal_child(123, expected)
                with patch('process_supervision.group_inactive', return_value=(False,['S'])), patch('evidence_processes.read_argv', side_effect=error):
                    with self.assertRaises(type(error)):self.stop_historical_signal_child(123, expected)
            with self.assertRaisesRegex(ValueError, 'positive'):self.stop_historical_signal_child(0, expected)
            self.assertFalse(kill.called)
            # The authentic identity can authorize only the recorded child group.
            with patch('process_supervision.group_inactive', side_effect=[(False,['S']),(True,[])]), patch('evidence_processes.read_argv', return_value=list(expected)):
                self.stop_historical_signal_child(123, expected)
            kill.assert_called_once_with(123, signal.SIGTERM)
        original = AssertionError('primary fixture assertion')
        errors = [OSError('orphan cleanup'), RuntimeError('leader cleanup')]
        streams = [SimpleNamespace(close=lambda:None) for _ in range(3)]
        process = SimpleNamespace(pid=123, poll=lambda:None, stdin=streams[0], stdout=streams[1], stderr=streams[2])
        with patch.object(ReplayIntegrity,'stop_historical_signal_child',side_effect=errors[0]), patch('process_supervision.stop_group',side_effect=errors[1]):
            self.cleanup_historical_fixture(process, 124, expected, original)
        self.assertEqual(original.fixture_cleanup_failures, [repr(e) for e in errors], 'Leader cleanup replaced the primary fixture failure')

    def test_scope_exit_fixture_restores_ignored_sigint(self):
        import signal
        previous = signal.getsignal(signal.SIGINT)
        try:
            signal.signal(signal.SIGINT, signal.SIG_IGN)
            self.test_historical_scope_exit_keeps_the_committed_receipt()
            self.assertIs(signal.getsignal(signal.SIGINT), signal.SIG_IGN, 'Scope-exit fixture did not restore ignored SIGINT')
        finally:signal.signal(signal.SIGINT, previous)

    def test_historical_release_gate_rejects_eof_without_spinning(self):
        import ast
        import io
        from types import SimpleNamespace
        tree = ast.parse(self.historical_signal_program())
        gate = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'gate')
        reads = []
        scope = dict(time=SimpleNamespace(monotonic=lambda:0), os=SimpleNamespace(getpid=lambda:1),
                     sys=SimpleNamespace(stdin=io.StringIO('')), print=lambda *a,**kw:None)
        def ready(*args):
            reads.append(True)
            self.assertEqual(len(reads), 1, 'Historical release gate spun after EOF')
            return ([scope['sys'].stdin], [], [])
        with patch('select.select', ready):
            exec(compile(ast.fix_missing_locations(ast.Module(body=[gate],type_ignores=[])), '<gate>', 'exec'),scope)
            with self.assertRaisesRegex(RuntimeError, 'Historical signal fixture input closed'):
                scope['gate'](True)

    def test_historical_extraction_and_audit_unwind_real_signals(self):
        import signal
        for ignored in (False, True):
            for phase in ('extraction', 'verification'):
                for sig in (signal.SIGTERM, signal.SIGINT):
                    with self.subTest(phase=phase, signal=sig, inherited_ignored=ignored):
                        report = self.historical_signal_case(phase, sig, ignored)
                        self.assertEqual(report.get('error'), 'KeyboardInterrupt', 'Child SIGINT disposition was not restored')
                        self.assertFalse(report['final'])

    def test_historical_signal_gate_has_an_independent_deadline(self):
        import ast
        from types import SimpleNamespace
        program = ast.parse(self.historical_signal_program())
        gate = next(node for node in program.body if isinstance(node, ast.FunctionDef) and node.name == 'gate')
        elapsed = [0.0]
        sleeps = []
        def sleep(delay):
            sleeps.append(delay)
            self.assertLessEqual(delay, .05)
            self.assertEqual(len(sleeps), 1, 'Unbounded historical gate attempted another sleep')
            elapsed[0] = 6.0
        scope=dict(time=SimpleNamespace(monotonic=lambda:elapsed[0], sleep=sleep),
                   print=lambda *a,**kw:None, os=SimpleNamespace(getpid=lambda:1))
        exec(compile(ast.fix_missing_locations(ast.Module(body=[gate],type_ignores=[])), '<gate>', 'exec'),scope)
        with self.assertRaisesRegex(RuntimeError,'Historical signal fixture deadline expired'):
            scope['gate']()
        self.assertEqual(sleeps,[.05])

    def test_historical_signal_gate_is_bounded_even_after_parent_exit(self):
        for parent_exits in (False, True):
            with self.subTest(parent_exits=parent_exits):
                report = self.historical_signal_case('extraction', parent_exits=parent_exits)
                self.assertEqual(report.get('error'), 'RuntimeError')
                self.assertEqual(report.get('message'), 'Historical signal fixture deadline expired', 'Historical gate lost its own deadline')
                self.assertFalse(report['final'])

    def test_historical_publication_defers_signals_through_commit_and_cleanup(self):
        import signal
        for phase in ('rename', 'published', 'cleanup'):
            for sig in (signal.SIGTERM, signal.SIGINT):
                with self.subTest(phase=phase, signal=sig):
                    report = self.historical_signal_case(phase, sig)
                    self.assertNotIn('error', report, 'Committed publication was reported as an interruption')
                    self.assertTrue(report['final'])
                    self.assertEqual(report['result']['publication']['interruptions'], [dict(signal=int(sig))],
                                     'Committed publication lost its interruption observation')

    def test_historical_scope_exit_keeps_the_committed_receipt(self):
        import contextlib
        import signal

        import evidence_historical as historical
        package, _ = self.carrier_fixture()
        native = historical.cleanup_interruptions
        # Inject just after the cleanup deferral has restored its handlers.
        for sig in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signal=sig):
                previous = signal.getsignal(signal.SIGINT)
                signal.signal(signal.SIGINT, signal.default_int_handler)
                try:
                    scopes = []
                    @contextlib.contextmanager
                    def after_scope(observation, scopes=scopes, sig=sig):
                        with native(observation):
                            yield
                        scopes.append(True)
                        if len(scopes) == 2:
                            import os
                            os.kill(os.getpid(), sig)
                    destination = self.root / ('exit-' + str(int(sig)))
                    with patch.object(historical, 'cleanup_interruptions', after_scope):
                        try:result = historical.audit_package(package, destination)
                        except KeyboardInterrupt:self.fail('Scope exit lost the committed signal receipt')
                    self.assertTrue(destination.exists())
                    self.assertTrue(result['publication']['committed'])
                    self.assertTrue(result['publication']['interruptions'], 'Scope exit lost the committed signal receipt')
                finally:signal.signal(signal.SIGINT, previous)

    def test_historical_handler_restoration_keeps_commitment(self):
        import contextlib

        import evidence_historical as historical
        package, _ = self.carrier_fixture()
        native = historical.interruption
        original = KeyboardInterrupt('interrupted while restoring the operation handler')
        @contextlib.contextmanager
        def restore():
            with native():
                yield
            raise original
        with patch.object(historical, 'interruption', restore):
            result = historical.audit_package(package, self.root / 'restored')
        self.assertTrue(result['publication']['committed'])
        self.assertIs(original.historical_publication, result)
        self.assertTrue(result['publication']['interruptions'], 'Handler restoration lost commitment')

    def test_historical_published_cleanup_failure_keeps_receipt_and_exception(self):
        import evidence_historical as historical
        package, _ = self.carrier_fixture()
        destination = self.root / 'published'
        native = historical.tempfile.TemporaryDirectory.cleanup
        error = OSError('genuine published cleanup failure')
        calls = []
        def fail_once(temporary):
            calls.append(temporary)
            if len(calls) == 1:raise error
            return native(temporary)
        with patch.object(historical.tempfile.TemporaryDirectory, 'cleanup', fail_once):
            with self.assertRaises(OSError) as caught:historical.audit_package(package, destination)
        self.assertIs(caught.exception, error)
        self.assertTrue(error.historical_publication['publication']['committed'])
        self.assertTrue(destination.exists())
        for temporary in calls:native(temporary)

    def test_historical_cleanup_preserves_primary_failure_and_restores_handlers(self):
        import signal

        import evidence_historical as historical
        primary = KeyboardInterrupt('primary extraction interruption')
        cleanup = OSError('secondary cleanup failure')
        native = historical.tempfile.TemporaryDirectory
        previous = signal.getsignal(signal.SIGTERM)
        with native() as folder:
            temporary = native(dir=folder)
            with patch.object(historical.tempfile, 'TemporaryDirectory', return_value=temporary):
                with patch.object(temporary, 'cleanup', side_effect=cleanup):
                    with self.assertRaises(KeyboardInterrupt) as caught:
                        with historical.historical_staging(folder):
                            raise primary
            self.assertIs(caught.exception, primary)
            self.assertIn('secondary cleanup failure', primary.historical_cleanup['failure'])
            self.assertEqual(signal.getsignal(signal.SIGTERM), previous)
            temporary.cleanup()
            temporary = native(dir=folder)
            with patch.object(historical.tempfile, 'TemporaryDirectory', return_value=temporary):
                with patch.object(temporary, 'cleanup', side_effect=cleanup):
                    with self.assertRaises(OSError) as caught:
                        with historical.historical_staging(folder):
                            pass
            self.assertIs(caught.exception, cleanup)
            self.assertEqual(signal.getsignal(signal.SIGTERM), previous)
            temporary.cleanup()

    def test_cleanup_failure_survives_an_interrupt_during_unwinding(self):
        import contextlib

        import evidence_historical as historical
        package, _ = self.carrier_fixture()
        native_cleanup = historical.tempfile.TemporaryDirectory.cleanup
        native_scope = historical.cleanup_interruptions
        error = OSError('owned cleanup failure before signal')
        owners = []
        def cleanup(temporary):
            owners.append(temporary)
            raise error
        @contextlib.contextmanager
        def interrupted(observation):
            try:
                with native_scope(observation):yield
            except OSError:
                raise KeyboardInterrupt('signal during cleanup unwinding')
        try:
            with patch.object(historical.tempfile.TemporaryDirectory, 'cleanup', cleanup), patch.object(historical, 'cleanup_interruptions', interrupted):
                try:historical.audit_package(package, self.root/'committed-cleanup-error')
                except OSError as caught:
                    self.assertIs(caught, error, 'Owned cleanup failure identity changed')
                else:self.fail('Owned cleanup failure was swallowed by an interrupt')
            self.assertTrue(error.historical_publication['publication']['committed'])
            self.assertTrue(error.historical_cleanup['interruptions'])
        finally:
            for temporary in owners:native_cleanup(temporary)

    def test_cleanup_and_teardown_preserve_cause_and_all_secondary_errors(self):
        import contextlib

        import evidence_historical as historical
        cause = LookupError('original explicit cause')
        primary = ValueError('original operation failure')
        primary.__cause__ = cause
        cleanup = OSError('first secondary cleanup')
        teardown = RuntimeError('second secondary teardown')
        @contextlib.contextmanager
        def interrupted():
            try:yield
            finally:raise teardown
        native = historical.tempfile.TemporaryDirectory
        with native() as folder:
            temporary = native(dir=folder)
            try:
                with patch.object(historical.tempfile, 'TemporaryDirectory', return_value=temporary), patch.object(temporary, 'cleanup', side_effect=cleanup), patch.object(historical, 'interruption', interrupted):
                    with self.assertRaises(ValueError) as caught:
                        with historical.historical_staging(folder):raise primary
                self.assertIs(caught.exception, primary)
                self.assertIs(primary.__cause__, cause, 'Cleanup replaced original exception cause')
                details = primary.historical_cleanup
                self.assertIn('first secondary cleanup', details['failure'])
                self.assertEqual([v['error'] for v in details['failures']], [repr(cleanup), repr(teardown)], 'Secondary cleanup failure was overwritten')
            finally:temporary.cleanup()

    def test_extraction_measurement_observes_completed_audit_work(self):
        import evidence_historical as historical
        from evidence_benchmarks import verify_tooling_metrics
        from evidence_tool_bench import measure
        package, metadata = self.carrier_fixture()
        variant = self.root / 'immutable variant'; variant.mkdir()
        self.assertFalse((variant / 'carrier').exists())
        from evidence_carrier import describe_carrier
        carrier = describe_carrier(self.root, 'carrier')
        with patch.object(historical, 'PACKAGE', 'ignored-variant-default'), patch.object(historical, 'audit_package', wraps=historical.audit_package) as observed:
            try:
                value = measure(variant, 'unused', 'historical-extraction', self.root, carrier)
            except (OSError, ValueError) as error:
                self.fail('Extraction benchmark ignored shared declared carrier: ' + str(error))
        self.assertEqual(observed.call_count, 1)
        self.assertEqual(observed.call_args.args[0], package)
        self.assertEqual(value['files'], metadata['expanded']['files'])
        self.assertEqual(value['carrier_archive_sha256'], carrier['archive']['sha256'])
        self.assertEqual(value['expanded_files'], metadata['expanded']['files'])
        self.assertEqual(value['expanded_bytes'], metadata['expanded']['bytes'])
        value.update(harness='tooling', workload='historical-extraction')
        verify_tooling_metrics(value)
        with self.assertRaisesRegex(ValueError, 'Extraction work counters'):
            verify_tooling_metrics(dict(value, historical_extractions=0))

    def extraction_measurement_bundle(self, wrong=False, wrong_size=False):
        import statistics

        from evidence_carrier import describe_carrier
        if not (self.root / 'carrier').exists():self.carrier_fixture()
        carrier=describe_carrier(self.root,'carrier')
        if wrong:carrier['archive']['sha256']='0'*64
        names=['config.json',carrier['archive']['path'],carrier['transport']['path']]
        archive_bytes=(self.root/carrier['archive']['path']).stat().st_size + int(wrong_size)
        row=dict(harness='tooling', workload='historical-extraction',metric_version=2, extraction_metric_version=4,
                 timing_scope='verified-operation', publication_interruptions=0, archive_bytes=archive_bytes, archive_hash_passes=3, archive_hash_read_bytes=archive_bytes*3,
                 historical_extractions=1,expanded_files=477,expanded_bytes=1024,files=477,owned_processes=0,
                 check_output_calls=0,git_commands=0,process_inspection_commands=0,retained_traced_blocks=1,
                 retained_traced_bytes=1,peak_traced_bytes=1,carrier_path='carrier',
                 carrier_archive_sha256=carrier['archive']['sha256'],carrier_transport_sha256=carrier['transport']['sha256'])
        rows=[dict(row,set=1,sample=n,variant=v,warmup=n==0,milliseconds=float(n+1)) for v in ('baseline','changed') for n in (0,1)]
        metrics=[k for k,v in rows[0].items() if type(v) in (int,float) and k not in ('set','sample')]
        summary=[dict(set=1,harness='tooling',workload='historical-extraction',variant=v,
                      **{k:statistics.median(r[k] for r in rows if r['variant']==v and not r['warmup']) for k in metrics})
                 for v in ('baseline','changed')]
        payload=dict(rows=rows,summary=summary,carrier=carrier)
        script=self.root/'producer.py'
        script.write_text('import sys,json\nfrom pathlib import Path\nsys.path.insert(0,'+repr(str(Path(evidence.__file__).parent))
            +')\nfrom validation_evidence import write_artifact\nwrite_artifact(Path("measure.json"),"measure",json.loads('
            +repr(json.dumps(payload))+'),'+repr(names)+')\n')
        evidence.atomic_json(self.root/'config.json',dict(cases=dict(tooling=['historical-extraction']),sets=1,measured=1,carrier=carrier))
        self.folder=self.root/('wrong-size-capture' if wrong_size else 'wrong-capture' if wrong else 'extraction-capture')
        self.checks=[dict(id='measure',argv=[sys.executable,str(script)],artifacts=[dict(path='measure.json',kind='measure',inputs=names)])]
        self.capture()
        target=self.root/('wrong-size-bundle' if wrong_size else 'wrong-extraction-bundle' if wrong else 'extraction-bundle')
        evidence.publish_bundle(self.manifest,self.folder,target)
        return target

    def test_offline_extraction_binds_descriptor_to_actual_archived_inputs(self):
        import shutil

        from evidence_bundle import verify_bundle
        target=self.extraction_measurement_bundle()
        moved=self.root/'moved extraction';shutil.copytree(target,moved)
        self.assertTrue(verify_bundle(moved)['verified'])
        with self.assertRaisesRegex(ValueError,'Shared carrier input missing, unarchived, or mismatched',
                                    msg='Offline extraction accepted a contradictory archived carrier identity'):
            self.extraction_measurement_bundle(wrong=True)
        self.assertFalse((self.root/'wrong-extraction-bundle').exists())

    def test_offline_archive_work_bytes_match_the_archived_carrier(self):
        with self.assertRaisesRegex(ValueError, 'archive byte counter differs'):
            self.extraction_measurement_bundle(wrong_size=True)
        self.assertFalse((self.root/'wrong-size-bundle').exists())

    def test_relocated_audit_receipt_names_only_its_guarded_apis(self):
        import shutil
        # Archive the verifier used by the child, rather than importing it from a checkout.
        shutil.copytree(Path(evidence.__file__).parent,self.root/'scripts',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
        self.capture()
        target=self.root/'offline-bundle';evidence.publish_bundle(self.manifest,self.folder,target)
        output=self.root/'offline-receipt.json'
        driver=Path(evidence.__file__).parent/'evidence_offline_audit.py'
        subprocess.run([sys.executable,'-B',str(driver),str(target),str(output),str(self.root.resolve())],
                       check=True,capture_output=True,text=True,timeout=30)
        receipt=json.loads(output.read_text())
        self.assertEqual(receipt['guarded_file_apis'],['builtins.open','io.open','os.open','os.stat','os.lstat'])
        self.assertEqual(receipt['guarded_process_apis'],['subprocess.Popen','subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call'])
        self.assertNotIn('original_checkout_io_forbidden',receipt)
        self.assertNotIn('process_launches_forbidden',receipt)
        import ast
        tree=ast.parse(receipt['driver_source'])
        loops=[node for node in ast.walk(tree) if isinstance(node,ast.For)]
        files=next(node for node in loops if isinstance(node.target,ast.Tuple) and
                   isinstance(node.iter,ast.List) and any(isinstance(value,ast.Tuple) and
                   isinstance(value.elts[0],ast.Name) and value.elts[0].id=='builtins' for value in node.iter.elts))
        actual_files=[value.elts[0].id+'.'+value.elts[1].value for value in files.iter.elts]
        processes=next(node for node in loops if isinstance(node.iter,ast.Tuple) and
                       any(isinstance(value,ast.Constant) and value.value=='Popen' for value in node.iter.elts))
        actual_processes=['subprocess.'+value.value for value in processes.iter.elts]
        self.assertEqual(receipt['guarded_file_apis'],actual_files)
        self.assertEqual(receipt['guarded_process_apis'],actual_processes)
        self.assertIn('not an OS sandbox',receipt['guard_scope'])
        self.assertEqual(receipt['interpreter']['version'][:2],list(sys.version_info[:2]))
        self.assertTrue(receipt['audit']['verified'])
        self.assertTrue(receipt['negative_control']['rejected'])

    def test_offline_guards_canonicalize_aliases_and_restore_on_import_failure(self):
        import builtins
        import importlib.util
        import os

        import evidence_offline_audit as driver
        target = self.root / 'private.txt'
        target.write_text('original checkout')
        alias = self.root.parent / (self.root.name + '-alias')
        alias.symlink_to(self.root, target_is_directory=True)
        self.addCleanup(alias.unlink)
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            originals = [driver.canonical('.')]
        finally:
            os.chdir(previous)
        self.assertEqual(originals, [str(self.root.resolve())])
        self.assertEqual(driver.canonical(alias), originals[0])
        native = builtins.open
        with tempfile.TemporaryDirectory() as directory:
            module = Path(directory) / 'guard_import.py'
            module.write_text('from builtins import open as read\nread(' + repr(str(alias / 'private.txt')) + ')\n')
            with self.assertRaises(PermissionError, msg='Canonical offline guard allowed an original root alias'), driver.guarded_apis(originals):
                spec=importlib.util.spec_from_file_location('guard_import', module)
                spec.loader.exec_module(importlib.util.module_from_spec(spec))
        self.assertIs(builtins.open, native, 'Import failure did not restore offline guards')
        with driver.guarded_apis(originals):
            for path in (target, alias/'private.txt'):
                with self.assertRaises(PermissionError, msg='Canonical offline guard allowed an original root alias'):
                    path.read_text()

    def test_offline_driver_installs_guards_before_import_for_relative_roots(self):
        import evidence_offline_audit as driver
        target=self.root/'private.txt';target.write_text('original checkout')
        bundle=self.root/'synthetic guard fixture';bundle.mkdir()
        (bundle/'inputs').mkdir();(bundle/'inputs/member').write_text('control')
        (bundle/'report.json').write_text('{}')
        (bundle/'inventory.json').write_text('{}')
        tooling=bundle/'verifier_tooling';tooling.mkdir()
        (tooling/'evidence_bundle.py').write_text(
            'from builtins import open as captured_open\n'
            +'captured_open('+repr(str(target))+').read()\n'
            +'calls=0\ndef verify_bundle(folder):\n global calls\n calls+=1\n'
            +' if calls>1:raise ValueError("inventory corruption")\n'
            +' return dict(verified=True,eligible_current=True)\n')
        output=self.root/'should-not-exist.json'
        completed=subprocess.run([sys.executable,'-B',driver.__file__,str(bundle),str(output),'.'],
                                 cwd=self.root,capture_output=True,text=True,timeout=15)
        self.assertNotEqual(completed.returncode,0,'Verifier import escaped the selected offline guards')
        self.assertIn('Original checkout I/O is forbidden',completed.stderr)
        self.assertFalse(output.exists())

    def test_offline_disposition_defers_to_auditor_with_optional_expectation(self):
        from evidence_offline_audit import validate_disposition
        for unmet in ({'benchmarks':'failed'}, {'hash-experiment':'failed','benchmarks':'blocked'}):
            value=dict(integrity_verified=True,eligible_current=False,unmet_required=unmet)
            try:
                validate_disposition(value, True)
                validate_disposition(value, True, unmet)
            except ValueError:self.fail('Shared audit driver rejected an auditor-approved disposition')
            with self.assertRaisesRegex(ValueError, '--expect-unmet'):
                validate_disposition(value, True, {'unrelated':'failed'})
        with self.assertRaisesRegex(ValueError, 'disposition'):
            validate_disposition(dict(integrity_verified=True,eligible_current=True), True)

    def test_interrupted_committed_extraction_cannot_be_a_sample(self):
        import evidence_historical as historical
        from evidence_carrier import describe_carrier
        from evidence_tool_bench import measure
        package, _ = self.carrier_fixture()
        carrier = describe_carrier(self.root,'carrier')
        native = historical.audit_package
        def interrupted(*args):
            result = native(*args)
            result['publication']['interruptions'].append(dict(signal=2))
            return result
        with patch.object(historical,'audit_package',interrupted):
            with self.assertRaisesRegex(ValueError,'Interrupted extraction measurement',
                                        msg='Interrupted committed extraction became a timing sample') as failed:
                measure(self.root,'unused','historical-extraction',self.root,carrier)
        self.assertTrue(failed.exception.historical_publication['publication']['committed'])
        self.assertTrue(failed.exception.historical_publication['publication']['interruptions'])

    def test_extraction_hash_work_and_live_metric_contract_are_observed(self):
        from evidence_benchmarks import verify_tooling_metrics
        from evidence_carrier import describe_carrier
        from evidence_tool_bench import measure
        _, metadata = self.carrier_fixture()
        carrier=describe_carrier(self.root,'carrier')
        row=measure(self.root,'unused','historical-extraction',self.root,carrier)
        self.assertEqual(row['archive_hash_passes'],3,'Extraction omitted a verified archive hash pass')
        self.assertEqual(row['archive_hash_read_bytes'],metadata['archive']['bytes']*3,'archive_hash_bytes did not match streamed work')
        row.update(harness='tooling',workload='historical-extraction')
        verify_tooling_metrics(row)
        for name, value in (('archive_hash_passes',2),('archive_hash_read_bytes',1),('publication_interruptions',1),('timing_scope','extraction-only')):
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'verified-operation'):
                verify_tooling_metrics(dict(row,**{name:value}))

    def test_live_tooling_requires_an_actual_metric_version(self):
        from evidence_benchmarks import verify_tooling_metrics
        for version in (None,True,1,1.0,2.0,3):
            row=dict(harness='tooling',workload='campaign-layout')
            if version is not None:row['metric_version']=version
            with self.subTest(version=version),self.assertRaisesRegex(ValueError,'tooling metric version',
                msg='Unversioned or mistyped live tooling bypassed its contract'):
                verify_tooling_metrics(row)
        verify_tooling_metrics(dict(harness='tooling',files=128),historical=True)

    def test_archive_instrumentation_delegates_native_hashing_and_optional_buffers(self):
        import evidence_carrier
        import evidence_historical
        from evidence_tool_bench import archive_hash_work
        from verified_sources import file_hash
        package, _ = self.carrier_fixture()
        carrier = evidence_carrier.describe_carrier(self.root, 'carrier')
        archive = self.root/carrier['archive']['path']
        other = self.root/'other';other.write_bytes(b'nonarchive')
        native_open = Path.open
        calls = []
        def native(path, buffer=None):
            calls.append((path,buffer))
            return file_hash(path,buffer)
        buffer = bytearray(13)
        with patch.object(evidence_carrier, 'sha256', native):
            with archive_hash_work(self.root, carrier) as observed:
                self.assertEqual(evidence_carrier.sha256(archive, buffer), file_hash(archive))
                self.assertEqual(evidence_carrier.sha256(other, buffer), file_hash(other))
            self.assertEqual(calls, [(archive,buffer),(other,buffer)], 'Archive instrumentation bypassed native hashing')
            self.assertEqual(observed['archive_hash_read_bytes'], archive.stat().st_size, 'Native hash read bytes were not observed')
            self.assertEqual(observed['archive_hash_passes'], 1)
            self.assertIs(evidence_carrier.sha256, native)
        self.assertIs(Path.open, native_open)
        error = OSError('native hash failure')
        with patch.object(evidence_historical, 'sha256', side_effect=error):
            with self.assertRaises(OSError) as caught:
                with archive_hash_work(self.root,carrier):evidence_historical.sha256(archive)
            self.assertIs(caught.exception,error)
        self.assertIs(Path.open,native_open)

    def test_carrier_descriptor_is_validated_before_any_lookup(self):
        from evidence_tool_bench import archive_hash_work
        for carrier in (None, {}, dict(path='carrier', archive=dict(path='../outside',sha256='0'*64), transport=dict(path='carrier/transport.json',sha256='0'*64))):
            with self.subTest(carrier=carrier), patch.object(Path,'stat') as stat:
                try:
                    with archive_hash_work(self.root,carrier):pass
                except ValueError:pass
                else:self.fail('Carrier lookup preceded descriptor validation')
                self.assertFalse(stat.called,'Unvalidated carrier performed metadata lookup')

    def test_contract_three_is_explicitly_historical(self):
        from evidence_benchmarks import verify_tooling_metrics
        row = dict(harness='tooling',workload='historical-extraction',metric_version=2,extraction_metric_version=3,
                   files=1,expanded_files=1,expanded_bytes=1,historical_extractions=1,owned_processes=0,
                   check_output_calls=0,git_commands=0,process_inspection_commands=0,
                   retained_traced_blocks=0,retained_traced_bytes=0,peak_traced_bytes=0,
                   archive_bytes=10,archive_hash_passes=3,archive_hash_bytes=30,publication_interruptions=0,timing_scope='verified-operation')
        verify_tooling_metrics(row,historical=True)
        with self.assertRaisesRegex(ValueError,'Extraction work'):verify_tooling_metrics(row)

    def test_declared_extraction_carrier_rejects_mutations_and_wrong_returned_identity(self):
        import evidence_historical as historical
        from evidence_carrier import (
            check_carrier,
            describe_carrier,
            selected_carrier_inputs,
            verify_carrier_rows,
        )
        from evidence_tool_bench import measure
        package, _ = self.carrier_fixture()
        carrier = describe_carrier(self.root, 'carrier')
        original = (package / 'transport.json').read_bytes()
        (package / 'transport.json').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'identity changed: transport'):
            check_carrier(self.root, carrier)
        (package / 'transport.json').write_bytes(original)
        native = historical.audit_package
        def wrong(*args):
            value = native(*args); value['archive_sha256'] = '0'*64; return value
        with patch.object(historical, 'audit_package', wrong):
            with self.assertRaisesRegex(ValueError, 'different shared carrier', msg='Extraction accepted a different returned carrier'):
                measure(self.root, 'unused', 'historical-extraction', self.root, carrier)
        self.assertEqual(selected_carrier_inputs(dict(cases=dict(tooling=['historical-extraction']),carrier=carrier)),
                         [carrier['archive']['path'], carrier['transport']['path']], 'Selected extraction omitted carrier declarations')
        self.assertEqual(selected_carrier_inputs(dict(cases=dict(tooling=[]))), [])
        with self.assertRaisesRegex(ValueError, 'shared carrier descriptor'):
            selected_carrier_inputs(dict(cases=dict(tooling=['historical-extraction'])))
        row=dict(harness='tooling', workload='historical-extraction', extraction_metric_version=2,
                 carrier_path='carrier',carrier_archive_sha256='0'*64,carrier_transport_sha256=carrier['transport']['sha256'])
        with self.assertRaisesRegex(ValueError, 'differs from its declared shared carrier', msg='Extraction sample ignored declared carrier identity'):
            verify_carrier_rows([row], carrier)

    def test_carrier_is_checked_again_after_extraction(self):
        import evidence_historical as historical
        from evidence_carrier import describe_carrier
        from evidence_tool_bench import measure
        package, _ = self.carrier_fixture()
        carrier = describe_carrier(self.root, 'carrier')
        native = historical.audit_package
        def mutate(*args):
            value=native(*args); (package / 'transport.json').write_text('changed during audit'); return value
        with patch.object(historical, 'audit_package', mutate):
            with self.assertRaisesRegex(ValueError, 'identity changed: transport', msg='Extraction omitted its final carrier check'):
                measure(self.root, 'unused', 'historical-extraction', self.root, carrier)

    def test_historical_count_metadata_is_validated_before_staging(self):
        import copy

        from evidence_historical import audit_package
        package, original = self.carrier_fixture()
        for section, field in (('archive', 'bytes'), ('expanded', 'files'), ('expanded', 'bytes')):
            for value in (None, True, False, '1', 1.0, 0, -1):
                with self.subTest(section=section, field=field, value=value):
                    metadata = copy.deepcopy(original)
                    if value is None: metadata[section].pop(field)
                    else: metadata[section][field] = value
                    evidence.atomic_json(package / 'transport.json', metadata)
                    with self.assertRaisesRegex(ValueError, section + r'\.' + field, msg='Expected positive-integer carrier field validation: ' + section + '.' + field):
                        audit_package(package, self.root / 'invalid')
                    self.assertFalse((self.root / 'invalid').exists())
                    self.assertFalse(list(self.root.glob('.historical-*')))

    def test_historical_publication_race_preserves_existing_destination(self):
        import evidence_historical as historical
        package, _ = self.carrier_fixture()
        destination = self.root / 'expanded'
        native = historical.rename_exclusive
        def race(stage, target):
            target.mkdir(); (target / 'winner').write_text('untouched')
            return native(stage, target)
        with patch.object(historical, 'rename_exclusive', race):
            with self.assertRaises(OSError):
                historical.audit_package(package, destination)
        self.assertEqual((destination / 'winner').read_text(), 'untouched')
        self.assertEqual([p.name for p in destination.iterdir()], ['winner'])
        self.assertFalse(list(self.root.glob('.historical-*')))

    def test_historical_v2_experiment_remains_auditable_but_not_acceptable(self):
        from evidence_bundle import verify_bundle
        target = self.historical_measurement_bundle(version=2, advisory=128)
        with self.assertRaisesRegex(ValueError, '256 advisory'):
            verify_bundle(target)
        value = verify_bundle(target, historical=True)
        self.assertTrue(value['verified']); self.assertFalse(value['eligible_current'])

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
        self.assertEqual(config["builds"]["make"], str((self.root / "build/fresh/make-preparation").resolve()))
        declared = json.loads(template.read_text()); declared['builds']['make'] = 'custom make'
        evidence.atomic_json(template, declared)
        self.assertEqual((config["sets"], config["measured"], config["python39"]), (3, 4, campaign.discover_python39(preferred="unavailable-python39")))
        other = campaign.prepare(self.root, Path("build/other"), "HEAD", sys.executable, sys.executable, sys.executable,
                                 template_path=template, clang_tidy=sys.executable, cppcheck=sys.executable)
        other_config = json.loads((other.parent / "config.json").read_text())
        self.assertEqual(other_config["python39"], sys.executable)
        self.assertEqual(other_config["builds"]["make"], str((self.root / "build/other/custom make").resolve()))
        self.assertIn('custom make', next(c for c in json.loads(other.read_text())["checks"] if c["id"] == 'make-configure')['argv'][4])


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
        from evidence_analysis import cppcheck_contexts_match, finding_key
        old=dict(id='warning',msg='same',locations=[dict(file='src/a.cpp',line=10,column=1,info='same')]*2)
        new=dict(id='warning',msg='same',locations=[dict(file='src/a.cpp',line=30,column=1,info='same')]*2)
        a,b=finding_key('cppcheck',old,self.old),finding_key('cppcheck',new,self.new)
        entry=dict(location_contexts=[dict(old=list(x),new=list(y),context='first context') for x,y in [(a[2][i],b[2][i]) for i in range(len(a[2]))]])
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

    def test_incomplete_or_unknown_isolation_version_cannot_report_success(self):
        from gtest_isolation import verify_payload
        for version,complete,message in ((3,True,'Unsupported'),(2,False,'Incomplete')):
            value=self.payload();value.update(payload_version=version,complete=complete)
            with self.assertRaisesRegex(ValueError,message):verify_payload(value)

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

    def test_failed_and_skipped_cases_preserve_reports_with_inconsistent_zero_exit(self):
        import gtest_isolation as isolated
        for exit_code in (0,1):
            with self.subTest(exit_code=exit_code), tempfile.TemporaryDirectory() as folder:
                binary=Path(folder)/'binary';binary.write_bytes(b'identity');checkpoints=[]
                def perform(argv, exit_code=exit_code, **kwargs):
                    xml=Path(argv[-1].split('xml:',1)[1])
                    if '--gtest_list_tests' in argv:
                        xml.write_text('<testsuites tests="2"><testsuite name="Suite"><testcase name="A"/><testcase name="B"/></testsuite></testsuites>')
                        return subprocess.CompletedProcess(argv,0,'','')
                    if '--gtest_filter=Suite.A' in argv:
                        xml.write_text('<testsuites tests="1" failures="1"><testsuite name="Suite"><testcase classname="Suite" name="A" status="run"><failure message="intended failure"/><skipped/></testcase></testsuite></testsuites>')
                        return subprocess.CompletedProcess(argv,exit_code,'failure details','')
                    xml.write_text('<testsuites tests="1"><testsuite name="Suite"><testcase classname="Suite" name="B" status="run"/></testsuite></testsuites>')
                    return subprocess.CompletedProcess(argv,0,'','')
                with patch.object(isolated,'supervised_run',side_effect=perform):
                    try:
                        value=isolated.run(binary,lambda current, checkpoints=checkpoints:checkpoints.append(len(current['results'])))
                    except ValueError as error:
                        self.fail('Protocol error lost its failed report: '+str(error))
                self.assertEqual(checkpoints,[1,2])
                self.assertEqual(value['failures'],1,'Protocol error lost its failed report')
                self.assertEqual(value['results'][0]['exit'],exit_code)
                self.assertIn('failure',value['results'][0]['xml'])
                self.assertEqual(value['skipped'],[] if exit_code==0 else ['Suite.A'])
                self.assertEqual(isolated.verify_payload(value)['failures'],1)

    def test_standalone_report_roots_allow_repeat_runs_without_replacement(self):
        import os

        import gtest_isolation as isolated
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);binary=root/'binary';binary.write_bytes(b'identity')
            with patch.dict(os.environ, {k:v for k,v in os.environ.items() if k != 'CODET_EVIDENCE_CONTEXT'}, clear=True), patch.object(isolated,'run',side_effect=lambda *args:self.payload()):
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
        verify_tooling_metrics(dict(harness='tooling',git_commands=5), historical=True) # Historical, not relabeled.

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
                (folder / name).write_text(json.dumps(dict(root=str(self.root))) if name=='manifest.json' else 'original ' + folder.name + ' ' + name)
        native=Path.read_text;reads=[]
        def counted(path,*args,**kwargs):
            if path.name=='manifest.json':reads.append(path)
            return native(path,*args,**kwargs)
        with patch.object(Path,'read_text',counted):paths = history_files(folders)
        self.assertEqual(reads,[folder/'manifest.json' for folder in folders])
        self.assertEqual(len(paths), 8)
        self.assertEqual({p.parent for p in paths if p.name == 'run.json'}, {f / 'capture' for f in folders})
        self.assertTrue(all(p.read_text().startswith('original ') for p in paths if p.name!='manifest.json'))
        (folders[1] / 'capture/run.json').unlink()
        with self.assertRaisesRegex(ValueError, 'Prior attempt metadata'):
            history_files(folders)

    def test_native_control_rejects_skipped_integration_and_gtest_targets(self):
        import evidence_native_controls as controls
        result=dict(exit=0,log='skipped')
        def integration(*args):
            (self.root/'integration-result.json').write_text(json.dumps(dict(tests=1,errors=[],failures=[],skipped=['requires Darwin'])))
            return result
        with patch.object(controls,'_run',side_effect=integration):
            with self.assertRaisesRegex(ValueError,'did not execute cleanly.*requires Darwin'):
                controls._integration(self.root,self.root/'binary',self.root)
        def native(*args):
            (self.root/'result.xml').write_text('<testsuites><testsuite><testcase classname="Portable" name="Image" status="run"><skipped/></testcase></testsuite></testsuites>')
            return result
        with patch.object(controls,'_run',side_effect=native):
            with self.assertRaisesRegex(ValueError,'did not execute exactly'):
                controls._test(self.root/'binary','Portable.Image',self.root)

    def test_history_preflight_does_not_leak_io_errors(self):
        from evidence_campaign import history_files
        try:
            history_files([self.root/'missing-attempt'])
        except ValueError as error:
            self.assertIn('Prior attempt metadata',str(error))
        except Exception as error:
            self.fail('Metadata validation leaked an I/O exception: '+type(error).__name__)
        else:
            self.fail('Missing history metadata was accepted')

    def test_prior_metadata_errors_are_identifying_value_errors(self):
        from evidence_campaign import history_files
        folder=self.root/'malformed-attempt';(folder/'capture').mkdir(parents=True)
        for name in ('config.json','capture-driver.log','capture/run.json'):
            (folder/name).write_text('historical metadata')
        manifest=folder/'manifest.json'
        with self.assertRaisesRegex(ValueError,'Prior attempt metadata.*manifest.json'):
            history_files([folder])
        for data in ('{','{}','[]','{"root": null}','{"root": ""}'):
            manifest.write_text(data)
            with self.subTest(data=data),self.assertRaisesRegex(ValueError,'Prior attempt metadata.*manifest.json') as caught:
                history_files([folder])
            self.assertIsNotNone(caught.exception.__cause__)
        manifest.write_text(json.dumps(dict(root=str(self.root))))
        capture=folder/'capture/broken.json';capture.write_text('{')
        with self.assertRaisesRegex(ValueError,'Prior attempt metadata.*broken.json'):
            history_files([folder])

    def test_prior_attempt_declared_artifacts_are_preserved_and_digest_checked(self):
        from evidence_campaign import history_files
        folder=self.root/'attempt';(folder/'capture').mkdir(parents=True)
        for name in ('config.json','capture-driver.log','capture/run.json'):
            (folder/name).write_text('original historical metadata')
        (folder/'manifest.json').write_text(json.dumps(dict(root=str(self.root))))
        artifact=folder/'analysis.json';artifact.write_text('original observed findings')
        (folder/'capture/analysis.json').write_text(json.dumps(dict(artifacts=[dict(path=str(artifact),sha256=evidence.digest(artifact))])))
        self.assertIn(artifact,history_files([folder]))
        artifact.write_text('changed')
        with self.assertRaisesRegex(ValueError,'Prior attempt artifact changed'):
            history_files([folder])

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


class ConfigurationMeasurement(unittest.TestCase):
    def test_configuration_metric_requires_three_real_commands_and_checks(self):
        from evidence_benchmarks import verify_tooling_metrics
        value=dict(harness='tooling',workload='configuration-selection',metric_version=2,
                   files=0,owned_processes=0,check_output_calls=3,git_commands=0,
                   process_inspection_commands=0,retained_traced_blocks=0,
                   retained_traced_bytes=0,peak_traced_bytes=0,
                   configuration_metric_version=1,cmake_commands=3,configuration_checks=3)
        verify_tooling_metrics(value)
        for field,wrong in (('cmake_commands',2),('files',2),('configuration_checks',2),('configuration_metric_version',99)):
            with self.subTest(field=field):
                with self.assertRaisesRegex(ValueError,'Configuration selection'):
                    verify_tooling_metrics(dict(value,**{field:wrong}))

    def test_real_configuration_fixture_runs_when_tools_are_available(self):
        import shutil

        from evidence_benchmarks import verify_tooling_metrics
        from evidence_tool_bench import measure
        if not shutil.which('cmake') or not shutil.which('ninja'):
            self.skipTest('Real configuration fixture requires CMake and Ninja')
        value=measure(Path(__file__).resolve().parents[1],'unused','configuration-selection')
        value.update(harness='tooling',workload='configuration-selection')
        verify_tooling_metrics(value)
        self.assertEqual((value['cmake_commands'],value['configuration_checks'],value['files']),(3,3,0))

    def test_missing_tools_skip_only_the_fixture_not_required_measurement(self):
        from evidence_tool_bench import measure
        with patch('evidence_tool_bench.shutil.which',return_value=None):
            fixture=ConfigurationMeasurement('test_real_configuration_fixture_runs_when_tools_are_available')
            result=unittest.TestResult();fixture.run(result)
            self.assertEqual(len(result.skipped),1,'Missing tools did not skip the optional fixture')
            self.assertFalse(result.errors or result.failures)
            with self.assertRaisesRegex(ValueError,'requires recorded CMake/Ninja'):
                measure(Path(__file__).resolve().parents[1],'unused','configuration-selection')

class PreparationMeasurement(unittest.TestCase):
    def test_verified_adapter_uses_the_production_lambda_and_rejects_ambiguous_excerpts(self):
        from evidence_harness import cli_preparation_adapter
        source = Path(__file__).resolve().parents[1] / 'src/modern/cli.cpp'
        excerpt, adapter = cli_preparation_adapter(source)
        self.assertIn(excerpt, source.read_text())
        self.assertIn(excerpt, adapter)
        self.assertIn('prepare_input_line', excerpt)
        import hashlib

        from evidence_harness import verify_cli_adapter
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'adapter.hpp'
            path.write_text(adapter)
            record = dict(source=str(source),adapter=str(path),source_sha256=evidence.digest(source),
                          adapter_sha256=evidence.digest(path),excerpt_sha256=hashlib.sha256(excerpt.encode()).hexdigest())
            verify_cli_adapter(record)
            path.write_text(adapter + '// altered')
            with self.assertRaisesRegex(ValueError, 'differs from verified production'):
                verify_cli_adapter(record)
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'cli.cpp'
            path.write_text(source.read_text() + '\n  const auto prepare_line = duplicate;')
            with self.assertRaisesRegex(ValueError, 'excerpt is not unique'):
                cli_preparation_adapter(path)

    def test_preparation_counters_require_executed_positive_controls(self):
        from evidence_benchmarks import verify_preparation_metrics
        row = dict(harness='preparation',workload='allocation-positive',preparation_metric_version=1,
                   iterations=128,allocations=128,allocated_bytes=8388608,payload_sized_allocations=128,
                   rejections=0,prepared_submissions=0,distinct_command_wrappers=0,effective_content_bytes=0)
        verify_preparation_metrics(row, historical=True)
        for key in ('iterations','allocations','allocated_bytes','payload_sized_allocations'):
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'preparation|Preparation'):
                verify_preparation_metrics(dict(row,**{key:0}), historical=True)
        for workload,changes in [('saturated-count',dict(rejections=127)),
                                 ('cli-preparation',dict(prepared_submissions=128,distinct_command_wrappers=0,effective_content_bytes=1664)),
                                 ('normalization',dict(effective_content_bytes=1))]:
            with self.subTest(workload=workload),self.assertRaisesRegex(ValueError,'preparation|Preparation|Normalization'):
                verify_preparation_metrics(dict(row,workload=workload,**changes), historical=True)


class PreparationRecovery(unittest.TestCase):
    @staticmethod
    def row(**changes):
        value = dict(harness='preparation', workload='saturated-count', variant='changed',
                     preparation_metric_version=2, advisory_budget_checks=128, iterations=128, allocations=256, allocated_bytes=18000,
                     payload_sized_allocations=0, rejections=128, prepared_submissions=0,
                     distinct_command_wrappers=0, effective_content_bytes=0)
        value.update(changes)
        return value

    def test_new_counter_contract_rejects_payload_allocations_and_duplicate_wrappers(self):
        from evidence_benchmarks import verify_preparation_metrics
        verify_preparation_metrics(self.row())
        for workload in ('saturated-count', 'saturated-effective', 'saturated-retained'):
            with self.subTest(workload=workload), self.assertRaisesRegex(ValueError, 'payload-sized'):
                verify_preparation_metrics(self.row(workload=workload, payload_sized_allocations=256))
        verify_preparation_metrics(self.row(variant='baseline', payload_sized_allocations=256))
        with self.assertRaisesRegex(ValueError, 'duplicated command wrappers'):
            verify_preparation_metrics(self.row(workload='cli-preparation', prepared_submissions=128,
                distinct_command_wrappers=256, effective_content_bytes=1664))
        verify_preparation_metrics(self.row(workload='cli-preparation', prepared_submissions=128,
            distinct_command_wrappers=128, effective_content_bytes=1664))

    def test_workload_validation_has_an_identifying_error(self):
        from evidence_benchmarks import verify_preparation_metrics
        for workload in (None, [], {}, 1, '', 'saturated-unknown'):
            with self.subTest(workload=workload), self.assertRaisesRegex(ValueError, 'preparation workload'):
                verify_preparation_metrics(self.row(workload=workload))
        row = self.row(); del row['workload']
        with self.assertRaisesRegex(ValueError, 'preparation workload'):
            verify_preparation_metrics(row)
        for variant in (None, '', 'other'):
            with self.subTest(variant=variant), self.assertRaisesRegex(ValueError, 'preparation variant'):
                verify_preparation_metrics(self.row(variant=variant))

    def test_historical_counters_remain_observations(self):
        from evidence_benchmarks import verify_preparation_metrics
        verify_preparation_metrics(self.row(preparation_metric_version=1, payload_sized_allocations=256), historical=True)

    def test_v2_baseline_and_historical_advisory_controls_reject_arbitrary_counts(self):
        from evidence_benchmarks import verify_preparation_metrics
        for historical in (False, True):
            for role in ('baseline', 'changed'):
                if not historical and role == 'changed':
                    continue
                for count in (0, 127, 129, 255, 257, 999):
                    with self.subTest(historical=historical, role=role, count=count), self.assertRaisesRegex(ValueError, 'advisory-check positive control'):
                        verify_preparation_metrics(self.row(variant=role, workload='preflight-checks', advisory_budget_checks=count,
                            prepared_submissions=128, effective_content_bytes=640), historical=historical)
                for count in (128, 256):
                    verify_preparation_metrics(self.row(variant=role, workload='preflight-checks', advisory_budget_checks=count,
                        prepared_submissions=128, effective_content_bytes=640), historical=historical)

    def test_positive_control_wrapper_requires_an_explicit_valid_role(self):
        from evidence_campaign import positive_controls
        row = self.row(workload='preflight-checks', advisory_budget_checks=256, prepared_submissions=128, effective_content_bytes=640)
        positive_controls(row, 'preparation', 'preflight-checks', 'changed')
        with self.assertRaises(TypeError):
            positive_controls(row, 'preparation', 'preflight-checks')
        for role in (None, '', 'other', 'baseline'):
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, 'preparation variant'):
                positive_controls(row, 'preparation', 'preflight-checks', role)

    def test_adapter_driver_rewrites_only_after_production_excerpt_changes(self):
        from preparation_bench_driver import generate
        source = Path(__file__).resolve().parents[1] / 'src/modern/cli.cpp'
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'generated/cli_prepare_adapter.hpp'
            generate(source, output)
            stamp = output.stat().st_mtime_ns
            generate(source, output)
            self.assertEqual(output.stat().st_mtime_ns, stamp)
            self.assertIn('prepare_input_line', output.read_text())
            changed = Path(directory) / 'changed.cpp'
            original = source.read_text()
            self.assertEqual(original.count('interaction.get(), cancel, [&](unsigned stage)'), 1)
            changed.write_text(original.replace('interaction.get(), cancel, [&](unsigned stage)',
                'interaction.get(), cancel, [&](unsigned stage /* valid excerpt change */)', 1))
            from preparation_adapter import cli_preparation_adapter
            expected = cli_preparation_adapter(changed)[1]
            self.assertNotEqual(expected, output.read_text())
            try:
                generate(changed, output)
            except ValueError as error:
                self.fail('Valid excerpt change did not rewrite the adapter: ' + str(error))
            self.assertEqual(output.read_text(), expected, 'Valid excerpt change did not rewrite the adapter')
            ambiguous = Path(directory) / 'cli.cpp'
            ambiguous.write_text(source.read_text() + '\n  const auto prepare_line = duplicate;')
            previous = output.read_bytes()
            with self.assertRaisesRegex(ValueError, 'not unique'):
                generate(ambiguous, output)
            self.assertEqual(output.read_bytes(), previous)

    def test_live_metric_contract_cannot_be_downgraded_or_change_role(self):
        from evidence_benchmarks import verify_preparation_metrics
        for version in (1, True, 1.0, 2.0, None, 3):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, 'preparation measurement counters'):
                verify_preparation_metrics(self.row(preparation_metric_version=version, payload_sized_allocations=256))
        for role in ('', 'other', None):
            with self.subTest(role=role), self.assertRaisesRegex(ValueError, 'preparation variant'):
                verify_preparation_metrics(self.row(), variant=role)
        for count in (128, 0, 255, 257):
            with self.subTest(count=count), self.assertRaisesRegex(ValueError, '256 advisory'):
                verify_preparation_metrics(self.row(workload='preflight-checks', advisory_budget_checks=count,
                    prepared_submissions=128, effective_content_bytes=640))
        verify_preparation_metrics(self.row(workload='preflight-checks', advisory_budget_checks=256,
            prepared_submissions=128, effective_content_bytes=640))
        # Rejected experiments remain auditable only through explicit historical interpretation.
        verify_preparation_metrics(self.row(workload='preflight-checks', advisory_budget_checks=128,
            prepared_submissions=128, effective_content_bytes=640), historical=True)

    def test_adapter_interruption_preserves_previous_complete_header(self):
        from contextlib import contextmanager

        import preparation_adapter
        from preparation_adapter import generate
        source = Path(__file__).resolve().parents[1] / 'src/modern/cli.cpp'
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'adapter.hpp'
            output.write_text('previous complete header')
            factory = preparation_adapter.os.fdopen
            @contextmanager
            def interrupted(*args, **options):
                with factory(*args, **options) as stream:
                    class Partial:
                        name = stream.name
                        def write(self, content):
                            stream.write(content[:23])
                            raise OSError('injected interrupted adapter write')
                    yield Partial()
            with patch('preparation_adapter.os.fdopen', interrupted):
                with self.assertRaisesRegex(OSError, 'interrupted adapter write'):
                    generate(source, output)
            self.assertEqual(output.read_text(), 'previous complete header', 'Interrupted write truncated the adapter')
            self.assertEqual([p.name for p in Path(directory).iterdir()], ['adapter.hpp'])
            with patch('preparation_adapter.os.replace', side_effect=OSError('replace failure')):
                with self.assertRaisesRegex(OSError, 'replace failure'):
                    generate(source, output)
            self.assertEqual(output.read_text(), 'previous complete header')
            generate(source, output)
            self.assertIn('prepare_input_line', output.read_text())

    def test_adapter_permissions_and_snapshot_extraction_are_preserved(self):
        import os
        import stat

        import preparation_adapter
        source = Path(__file__).resolve().parents[1] / 'src/modern/cli.cpp'
        for mask in (0o022, 0o077):
            with self.subTest(mask=mask), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / 'adapter.hpp'
                previous = os.umask(mask)
                try:
                    with patch.object(preparation_adapter, '_extract', wraps=preparation_adapter._extract) as extract:
                        record = preparation_adapter.generate(source, output)
                    self.assertEqual(extract.call_count, 1, 'Historical adapter parsed the same source twice')
                finally:
                    os.umask(previous)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o666 & ~mask, 'New adapter ignored the process umask')
                self.assertEqual(record['source_sha256'], evidence.digest(source))
                self.assertEqual(record['adapter_sha256'], evidence.digest(output))
                output.write_text('old complete header'); output.chmod(0o640)
                preparation_adapter.generate(source, output)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o640, 'Replacement adapter changed existing permissions')
                stamp = output.stat().st_mtime_ns
                preparation_adapter.generate(source, output)
                self.assertEqual(output.stat().st_mtime_ns, stamp)
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o640)

    def test_make_replay_directory_is_configurable_and_legacy_default_is_recorded(self):
        from evidence_campaign import replay_builds
        base = dict(debug='debug', asan='asan', tsan='tsan', cross='cross')
        self.assertEqual(replay_builds(base)['make'], 'make-preparation')
        self.assertNotIn('make', base)
        self.assertEqual(replay_builds(dict(base, make='space dir/custom make'))['make'], 'space dir/custom make')
        for invalid in (dict(base, make=''), dict(base, other='extra'), None, dict(base, debug=1)):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ValueError, 'replay build declaration'):
                replay_builds(invalid)

    def test_historical_sample_measurement_uses_the_historical_contract(self):
        from evidence_benchmarks import verify_tooling_metrics
        from evidence_tool_bench import measure
        value = measure(Path(__file__).resolve().parents[1], 'unused', 'historical-samples')
        value.update(harness='tooling', workload='historical-samples')
        verify_tooling_metrics(value)
        self.assertEqual((value['historical_sample_audits'], value['historical_rows']), (32, 768))
        for field in ('historical_sample_audits', 'historical_rows'):
            with self.assertRaisesRegex(ValueError, 'Historical sample audit'):
                verify_tooling_metrics(dict(value, **{field: 0}))

    def test_explicit_historical_adapter_cli_remains_usable(self):
        root = Path(__file__).resolve().parents[1]
        from preparation_adapter import cli_preparation_adapter
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'CLI adapter.hpp'
            result = subprocess.run([sys.executable, str(root / 'scripts/preparation_bench_driver.py'), 'adapter',
                '--source', str(root / 'src/modern/cli.cpp'), '--output', str(output)], text=True, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output.read_text(), cli_preparation_adapter(root / 'src/modern/cli.cpp')[1])

    def test_adapter_imports_no_evidence_stack_and_normal_build_has_no_generator(self):
        root = Path(__file__).resolve().parents[1]
        script = "import sys;sys.path.insert(0," + repr(str(root / 'scripts')) + ");import preparation_bench_driver;print(','.join(sorted(n for n in sys.modules if n.startswith('evidence_'))))"
        self.assertEqual(subprocess.check_output([sys.executable, '-c', script], text=True, timeout=5).strip(), '')
        cmake = (root / 'CMakeLists.txt').read_text()
        self.assertIn('add_executable(codet_preparation_bench tests/preparation_bench.cpp)', cmake)
        self.assertNotIn('set(preparation_adapter', cmake)
        self.assertNotIn('adapter --source', cmake)

    def test_python_control_patterns_match_their_current_production_locations(self):
        import ast
        root = Path(__file__).resolve().parents[1]
        tree = ast.parse((root / 'scripts/evidence_negative_controls.py').read_text())
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'run_controls')
        cases = next(ast.literal_eval(n.value) for n in function.body if isinstance(n, ast.Assign)
                     and isinstance(n.targets[0], ast.Name) and n.targets[0].id == 'cases')
        for name, module, before, _, _ in cases:
            path = root / ('cmake/dependency-root.cmake' if module == 'dependency-root' else ('tests/' if module == 'test_validation_evidence' else 'scripts/') + module + '.py')
            with self.subTest(control=name):
                self.assertEqual(path.read_text().count(before), 1, 'Python mutation no longer matches its production site: ' + name)

    def test_native_control_patterns_match_their_current_production_locations(self):
        import ast
        root = Path(__file__).resolve().parents[1]
        tree = ast.parse((root / 'scripts/evidence_native_controls.py').read_text())
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'run_controls')
        registries = {}
        for node in function.body:
            if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) and node.targets[0].id in ('cases', 'review_controls'):
                registries[node.targets[0].id] = ast.literal_eval(node.value)
        self.assertEqual(set(registries), {'cases', 'review_controls'})
        for name, _, source, before, _, _ in registries['cases']:
            with self.subTest(control=name):
                self.assertEqual((root / source).read_text().count(before), 1, 'Native mutation no longer matches its production site: ' + name)
        for name, _, before, _, _, *override in registries['review_controls']:
            source = override[0] if override else 'src/modern/cli.cpp'
            with self.subTest(control=name):
                self.assertEqual((root / source).read_text().count(before), 1, 'Integration mutation no longer matches its production site: ' + name)


    def test_adapter_byte_identity_repairs_all_line_endings(self):
        import preparation_adapter
        source = Path(__file__).resolve().parents[1] / 'src/modern/cli.cpp'
        expected = preparation_adapter.cli_preparation_adapter(source)[1].encode('utf-8')
        with tempfile.TemporaryDirectory() as d:
            output = Path(d) / 'adapter.hpp'
            for ending in (b'\n', b'\r\n', b'\r'):
                with self.subTest(ending=ending):
                    output.write_bytes(expected.replace(b'\n', ending))
                    output.chmod(0o640)
                    try:
                        value = preparation_adapter.generate(source, output)
                    except ValueError as error:
                        self.fail('Byte-different adapter was not repaired: ' + str(error))
                    self.assertEqual(output.read_bytes(), expected, 'Byte-different adapter was left in place')
                    self.assertEqual(value['adapter_sha256'], evidence.digest(output))
                    self.assertEqual(output.stat().st_mode & 0o777, 0o640)
                    stamp = output.stat().st_mtime_ns
                    preparation_adapter.generate(source, output)
                    self.assertEqual(output.stat().st_mtime_ns, stamp)
            def corrupt(path, content):
                path.write_text('different complete bytes')
            with patch.object(preparation_adapter, '_write_adapter', corrupt):
                output.write_text('old')
                with self.assertRaisesRegex(ValueError, 'changed during generation'):
                    preparation_adapter.generate(source, output)

    def test_historical_sample_work_counters_observe_completed_calls(self):
        from evidence_benchmarks import verify_samples, verify_tooling_metrics
        from evidence_tool_bench import measure
        with patch('evidence_benchmarks.verify_samples', wraps=verify_samples) as observed:
            value = measure(Path(__file__).resolve().parents[1], 'unused', 'historical-samples')
        self.assertEqual(value['historical_sample_audits'], 32, 'Completed audit work did not match the required workload')
        self.assertEqual(value['historical_sample_audits'], observed.call_count, 'Audit counter was not observed')
        self.assertEqual(value['historical_rows'], sum(len(call.args[0]) for call in observed.call_args_list), 'Row counter was not observed')
        value.update(harness='tooling', workload='historical-samples')
        verify_tooling_metrics(value)
        with patch('evidence_benchmarks.verify_samples', side_effect=RuntimeError('failed audit')) as failed:
            with self.assertRaisesRegex(RuntimeError, 'failed audit'):
                measure(Path(__file__).resolve().parents[1], 'unused', 'historical-samples')
        self.assertEqual(failed.call_count, 1)

    def test_managed_build_layout_rejects_collisions_before_side_effects(self):
        import evidence_campaign as campaign
        base = dict(debug='debug', asan='asan', tsan='tsan', cross='cross')
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d) / 'new campaign'
            for specification in ('debug', './debug', 'debug/nested', '.', '../escape', '/absolute',
                                  'home', 'home/nested', 'baseline-release', 'baseline-release/nested',
                                  'manifest.json', 'harness.json', 'control-example/nested'):
                with self.subTest(specification=specification), self.assertRaisesRegex(ValueError, 'build path'):
                    campaign.resolve_build_layout(folder, dict(base, make=specification))
                self.assertFalse(folder.exists(), 'Layout rejection created campaign files')
            good = campaign.resolve_build_layout(folder, dict(base, make='custom space/make'))
            self.assertEqual(good['make'], str((folder / 'custom space/make').resolve()))
            folder.mkdir(); outside = Path(d) / 'outside'; outside.mkdir()
            (folder / 'alias').symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'escapes campaign', msg='Expected explicit campaign containment rejection'):
                campaign.resolve_build_layout(folder, dict(base, make='alias/make'))
            (folder / 'same').symlink_to(folder / 'debug', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'collision'):
                campaign.resolve_build_layout(folder, dict(base, make='same'))

    def test_portable_layout_aliases_are_rejected_without_side_effects(self):
        import evidence_campaign as campaign
        base = dict(debug='debug', asan='asan', tsan='tsan', cross='cross')
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d) / 'new campaign'
            for name in ('Debug', 'DEBUG/child', 'Home', 'CAPTURE', 'BASELINE-RELEASE',
                         'Manifest.JSON', 'HARNESS.JSON', 'CONTROL-example', 'Native-Control-x', 'SEMGREP-CONTROL-x'):
                with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'build path', msg='Portable path alias was admitted'):
                    campaign.resolve_build_layout(folder, dict(base, make=name))
                self.assertFalse(folder.exists(), 'Portable layout rejection created campaign files')
            for first, second in (('café', 'cafe\u0301'), ('Straße', 'STRASSE'),
                                  ('custom/café', 'CUSTOM/cafe\u0301/nested')):
                with self.subTest(first=first, second=second), self.assertRaisesRegex(ValueError, 'collision', msg='Portable path alias was admitted'):
                    campaign.resolve_build_layout(folder, dict(base, debug=first, make=second))
            good = campaign.resolve_build_layout(folder, dict(base, debug='custom café/debug', make='other space/make'))
            self.assertEqual(good['make'], str((folder / 'other space/make').resolve()))
            # Submitted aliases are forbidden even if an existing symlink resolves elsewhere.
            folder.mkdir(); (folder / 'Home').symlink_to(folder / 'unused', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'collision', msg='Portable path alias was admitted'):
                campaign.resolve_build_layout(folder, base | dict(make='Home/build'))
            (folder / 'redirect').symlink_to(folder / 'DEBUG', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'collision', msg='Portable path alias was admitted'):
                campaign.resolve_build_layout(folder, base | dict(make='redirect/child'))

    def test_layout_measurement_records_completed_resolutions(self):
        import evidence_campaign as campaign
        from evidence_benchmarks import verify_tooling_metrics
        from evidence_tool_bench import measure
        with patch.object(campaign, 'resolve_build_layout', wraps=campaign.resolve_build_layout) as observed:
            value = measure(Path(__file__).resolve().parents[1], 'unused', 'campaign-layout')
        self.assertEqual(value['files'], 0, 'Layout validation claimed file work')
        self.assertEqual(value['layout_checks'], observed.call_count)
        self.assertEqual(value['build_locations'], 5 * observed.call_count, 'Build-location counter omitted completed work')
        value.update(harness='tooling', workload='campaign-layout')
        verify_tooling_metrics(value)
        with self.assertRaisesRegex(ValueError, 'Layout work counters'):
            verify_tooling_metrics(dict(value, layout_checks=0))

    def test_live_tooling_contracts_reject_old_versions_and_mislabelled_files(self):
        from evidence_benchmarks import verify_tooling_metrics
        from evidence_tool_bench import measure
        value=measure(Path(__file__).resolve().parents[1], 'unused', 'campaign-layout')
        value.update(harness='tooling',workload='campaign-layout')
        for version in (None,True,1,1.0,2.0,3):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError,'Layout work counters'):
                verify_tooling_metrics(dict(value,layout_metric_version=version))
        with self.assertRaisesRegex(ValueError,'Layout work counters',msg='Layout operation count was accepted as files'):
            verify_tooling_metrics(dict(value,files=128))
        historical=dict(value,layout_metric_version=1,files=128)
        verify_tooling_metrics(historical,historical=True)
        with self.assertRaisesRegex(ValueError,'Layout work counters'):
            verify_tooling_metrics(historical)
        extraction=dict(value, workload='historical-extraction',extraction_metric_version=4,
                        timing_scope='verified-operation', publication_interruptions=0, archive_bytes=10, archive_hash_passes=3, archive_hash_read_bytes=30,
                        historical_extractions=1,expanded_files=477,expanded_bytes=1024,files=477)
        verify_tooling_metrics(extraction)
        for version in (None,True,1,1.0,2,2.0,3,3.0,4.0,5):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError,'Extraction work counters'):
                verify_tooling_metrics(dict(extraction,extraction_metric_version=version))
        with self.assertRaisesRegex(ValueError,'Extraction work counters',msg='Extraction operation count was accepted as files'):
            verify_tooling_metrics(dict(extraction,files=1))
        verify_tooling_metrics(dict(extraction,extraction_metric_version=1,files=1),historical=True)

    def test_measurement_envelope_binds_declared_carrier_inputs(self):
        import os

        import evidence_campaign as campaign
        from evidence_carrier import selected_carrier_inputs
        from evidence_historical import PACKAGE
        for extraction in (False, True):
            with self.subTest(extraction=extraction), tempfile.TemporaryDirectory() as d:
                root = Path(d).resolve(); folder = root / 'campaign'; folder.mkdir()
                carrier = dict(path=PACKAGE, archive=dict(path=PACKAGE + '/audit-bundle.tar.gz', sha256='a'*64),
                               transport=dict(path=PACKAGE + '/transport.json', sha256='b'*64))
                settings = dict(root=str(root), folder=str(folder), carrier=carrier,
                                cases=dict(tooling=['historical-extraction'] if extraction else ['campaign-layout']))
                names = ['campaign/config.json', 'campaign/harness.json', 'campaign/baseline-release/CMakeCache.txt',
                         'campaign/changed-release/CMakeCache.txt'] + selected_carrier_inputs(settings)
                for name in names:
                    path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text('fixture input')
                config = folder / 'config.json'; config.write_text(json.dumps(settings))
                spec = dict(path='campaign/measure.json', kind='measure', inputs=names)
                context = evidence._context(root, dict(id='1'*32, campaign_id='2'*32), 'benchmarks', {}, {})
                context['declared_artifacts'] = [spec]
                with patch.dict(os.environ, CODET_EVIDENCE_CONTEXT=json.dumps(context)), patch.object(campaign, 'measure', return_value=dict(fixture=True)):
                    try:campaign.produce(config, 'measure')
                    except ValueError as error:self.fail('Measurement producer omitted declared carrier inputs: ' + str(error))
                value = evidence.validate_artifact(root / spec['path'], spec, context)
                self.assertEqual(set(value['input_hashes']), set(names), 'Measurement carrier input coverage changed')

    def test_control_marker_rejects_unrelated_regex_assertions(self):
        from evidence_negative_controls import require_assertion_marker
        marker = 'Expected explicit campaign containment rejection'
        with self.assertRaisesRegex(ValueError, 'missed its intended assertion'):
            require_assertion_marker('layout-containment', dict(failure_details=['AssertionError: other regex does not match']), marker)
        require_assertion_marker('layout-containment', dict(failure_details=['AssertionError: ' + marker]), marker)

    def test_invalid_layout_precedes_tool_probes_and_campaign_creation(self):
        import evidence_campaign as campaign
        root = Path(__file__).resolve().parents[1]
        template = json.loads((root / 'checks/validation-replay.json').read_text())
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'template.json'
            template['builds']['make'] = 'Debug'
            path.write_text(json.dumps(template))
            folder = Path(d) / 'new'
            with patch.object(campaign, 'command', side_effect=AssertionError('unexpected tool probe')):
                with self.assertRaisesRegex(ValueError, 'build path collision'):
                    campaign.prepare(root, folder, template_path=path)
            self.assertFalse(folder.exists())

if __name__ == "__main__":
    unittest.main()
