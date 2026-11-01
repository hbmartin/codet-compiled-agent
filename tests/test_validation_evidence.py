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

    def rejected(self):
        with self.assertRaises((ValueError, KeyError, FileNotFoundError)):
            self.publish()
        self.assertEqual(self.output.read_text(), "previous artifact")

    def test_success_captures_actual_command_log_and_sources(self):
        result = self.capture()["test"]
        self.assertEqual(result["exit"], 0)
        self.assertEqual(result["argv"], self.checks[0]["argv"])
        self.assertEqual(result["cwd"], str(self.root.resolve()))
        value = self.publish(dict(production_hashes=evidence.production_hashes(self.root)))
        self.assertTrue(value["measurements_apply_to_current_source"])
        self.assertIn("passed", value["validation"][0]["log_tail"])

    def test_missing_result_rejected(self):
        self.capture()
        (self.folder / "test.json").unlink()
        self.rejected()

    def test_nonzero_cannot_be_overridden_by_success_log(self):
        self.checks[0]["argv"] = [sys.executable, "-c", "print('All tests passed');raise SystemExit(7)"]
        result = self.capture()["test"]
        self.assertEqual(result["exit"], 7)
        self.rejected()

    def test_launch_failure(self):
        self.checks[0]["argv"] = [str(self.root / "missing-command")]
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "launch-error")
        self.assertIsNone(result["exit"])
        self.rejected()

    def test_timeout(self):
        self.checks[0].update(argv=[sys.executable, "-c", "import time;time.sleep(60)"], timeout_seconds=0.05)
        result = self.capture()["test"]
        self.assertEqual(result["outcome"], "timeout")
        self.assertNotEqual(result["exit"], 0)
        self.rejected()

    def test_failed_build_never_runs_existing_test_binary(self):
        self.checks = [dict(id="build", argv=[sys.executable, "-c", "raise SystemExit(3)"]),
                       dict(id="test", requires=["build"], argv=[sys.executable, "-c", "open('launched','w').close()"])]
        results = self.capture()
        self.assertEqual(results["test"]["outcome"], "blocked")
        self.assertFalse((self.root / "launched").exists())
        self.rejected()

    def test_stale_sources_and_changed_log_rejected(self):
        for mutation in ("source", "log"):
            with self.subTest(mutation=mutation):
                self.capture()
                if mutation == "source":
                    (self.root / "src/core.cpp").write_text("changed")
                else:
                    (self.folder / "test.log").write_text("replaced")
                self.rejected()

    def test_source_mutation_during_capture_rejected(self):
        self.checks[0]["argv"] = [sys.executable, "-c", "open('src/core.cpp','w').write('new')"]
        self.capture()
        self.rejected()

    def test_changed_manifest_and_run_rejected(self):
        self.capture()
        record = json.loads((self.folder / "test.json").read_text())
        record["run_id"] = "previous capture"
        evidence.atomic_json(self.folder / "test.json", record)
        self.rejected()
        self.capture()
        self.checks[0]["argv"].append("another argument")
        self.setup_manifest()
        self.rejected()

    def test_changed_benchmark_sources_are_explicit(self):
        benchmark = dict(production_hashes=evidence.production_hashes(self.root), ref="recorded")
        (self.root / "src/core.cpp").write_text("changed after benchmark")
        self.capture()
        value = self.publish(benchmark)
        self.assertFalse(value["measurements_apply_to_current_source"])
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


if __name__ == "__main__":
    unittest.main()
