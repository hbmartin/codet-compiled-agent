"""Check lint evidence, failure propagation and exact tool identity."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import lint


class LintEnforcement(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.override = patch.object(lint, "ROOT", self.root)
        self.override.start()
        self.addCleanup(self.override.stop)

    def report(self, group):
        return json.loads((self.root / "build/lint" / (group + ".json")).read_text())

    def seed(self):
        lint.invalidate(lint.GROUPS)
        for group in lint.GROUPS:
            lint.write_report(group, dict(passed=True))

    def test_exact_versions_and_supported_banners(self):
        banners = {"ruff": "ruff 0.17.1", "clang-format": "clang-format version 18.1.8",
                   "shfmt": "v3.14.1", "actionlint": "v1.7.12\ninstalled by building from source",
                   "zizmor": "zizmor 1.30.1", "shellcheck": "ShellCheck - shell script analysis tool\nversion: 0.11.0"}
        for name, banner in banners.items():
            with self.subTest(name=name):
                self.assertRegex(lint.tool_version(name, banner), r"^\d+\.\d+\.\d+$")
        for banner in ("ruff 0.17.1rc1", "ruff 0.17.1-dev", "ruff 0.17.1\nruff 0.17.2", "", "other ruff 0.17.1"):
            with self.subTest(banner=banner), self.assertRaises(ValueError):
                lint.tool_version("ruff", banner)
        with self.assertRaises(ValueError):
            lint.tool_version("actionlint", "1.7.12\n1.7.13")

    def test_adjacent_versions_and_nonzero_results_fail(self):
        for version, status in (("ruff 0.17.10", 0), ("ruff 0.17.2", 0), ("ruff 0.17.1", 1)):
            with self.subTest(version=version, status=status):
                replies = [subprocess.CompletedProcess([], 0, version, ""),
                           subprocess.CompletedProcess([], status, "diagnostic", "")]
                with patch.object(lint, "commands", return_value=[("ruff", "--version", "0.17.1", [["check"]])]), \
                     patch.object(lint.subprocess, "run", side_effect=replies):
                    self.assertFalse(lint.run("python"))
                self.assertFalse(self.report("python")["passed"])

    def test_missing_tool_and_timeout_do_not_hide_later_checks(self):
        for failure in (FileNotFoundError("missing"), subprocess.TimeoutExpired("ruff", 15)):
            with self.subTest(failure=failure):
                def invoke(command, failure=failure, **kwargs):
                    if command[0] == "missing":
                        raise failure
                    return subprocess.CompletedProcess(command, 0, "ruff 0.17.1\n" if "--version" in command else "", "")
                tools = [("ruff", "--version", "0.17.1", [["check"]])] * 2
                with patch.object(lint, "commands", return_value=tools), \
                     patch.object(lint, "binary", side_effect=["missing", "working"]), \
                     patch.object(lint.subprocess, "run", side_effect=invoke):
                    self.assertFalse(lint.run("python"))
                self.assertTrue(self.report("python")["results"][1]["passed"])

    def test_inventory_failure_replaces_stale_success(self):
        self.seed()
        with patch.object(lint, "commands", side_effect=OSError("inventory unavailable")):
            self.assertFalse(lint.run("format"))
        self.assertIn("inventory unavailable", self.report("format")["failure"])
        self.assertFalse(self.report("format")["passed"])

    def test_interruption_before_inventory_invalidates_all_requested_groups(self):
        self.seed()
        with patch.object(lint, "commands", side_effect=KeyboardInterrupt), self.assertRaises(KeyboardInterrupt):
            lint.run_groups(lint.GROUPS)
        self.assertTrue(all(not self.report(group)["passed"] for group in lint.GROUPS))

    def test_interruption_between_groups_leaves_later_reports_failing(self):
        self.seed()
        def run(group):
            if group == "python":raise KeyboardInterrupt
            lint.write_report(group, dict(passed=True))
            return True
        with patch.object(lint, "run", side_effect=run), self.assertRaises(KeyboardInterrupt):
            lint.run_groups(lint.GROUPS)
        self.assertTrue(self.report("format")["passed"])
        self.assertTrue(all(not self.report(group)["passed"] for group in lint.GROUPS[1:]))

    def test_tool_interruption_keeps_initial_failing_report(self):
        self.seed()
        with patch.object(lint, "commands", return_value=[("ruff", "--version", "0.17.1", [[]])]), \
             patch.object(lint.subprocess, "run", side_effect=KeyboardInterrupt), self.assertRaises(KeyboardInterrupt):
            lint.run("python")
        self.assertFalse(self.report("python")["passed"])

    def test_all_groups_run_after_a_failure(self):
        with patch.object(lint, "run", side_effect=[False, True, True, True]) as run:
            self.assertFalse(lint.run_groups(lint.GROUPS))
        self.assertEqual(run.call_count, 4)

    def test_failed_atomic_publish_retains_initial_failure_and_cleans_temporary(self):
        lint.invalidate(("python",))
        with patch.object(lint.os, "replace", side_effect=OSError("publish failed")), self.assertRaises(OSError):
            lint.write_report("python", dict(passed=True))
        self.assertFalse(self.report("python")["passed"])
        self.assertEqual([p.name for p in (self.root / "build/lint").iterdir()], ["python.json"])


if __name__ == "__main__":
    unittest.main()
