"""Exercise local check failures without installing or invoking Semgrep."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("semgrep_report", ROOT / "scripts/semgrep-report.py")
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)


class SemgrepReports(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="codet-semgrep-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "scripts").mkdir()
        shutil.copy2(ROOT / "Makefile", self.root)
        shutil.copy2(ROOT / "scripts/semgrep-report.py", self.root / "scripts")
        (self.root / "toolchain.mk").write_text("", encoding="utf-8")
        self.tool = self.root / "fake-semgrep"
        self.tool.write_text(
            f"#!{sys.executable}\nimport os,sys\n"
            "if '--version' in sys.argv:\n print('1.177.0');sys.exit(0)\n"
            "print(os.environ['CODET_FAKE_REPORT'])\nsys.exit(int(os.environ['CODET_FAKE_EXIT']))\n",
            encoding="utf-8")
        self.tool.chmod(0o700)

    def run_check(self, target, payload, status):
        env = dict(os.environ, CODET_FAKE_REPORT=payload, CODET_FAKE_EXIT=str(status))
        return subprocess.run(
            ["make", target, f"SEMGREP={self.tool}", f"SEMGREP_PYTHON={sys.executable}"],
            cwd=self.root, env=env, text=True, capture_output=True, timeout=10)

    def test_scan_and_fixture_failures_keep_json_and_original_failure(self):
        for target, filename in [("check-semgrep", "scan.json"), ("test-semgrep", "tests.json")]:
            with self.subTest(target=target):
                data = {"errors": [{"message": "fixture failure detail"}]}
                result = self.run_check(target, json.dumps(data), 7)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("fixture failure detail", result.stderr)
                self.assertIn("Error 7", result.stderr)
                self.assertEqual(json.loads((self.root / "build/semgrep" / filename).read_text()), data)

    def test_invalid_report_still_fails_and_reports_parse_error(self):
        result = self.run_check("check-semgrep", "{invalid", 9)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Report is missing or invalid", result.stderr)
        self.assertIn("Error 9", result.stderr)

    def test_success_keeps_report_without_failure_message_or_installing(self):
        result = self.run_check("check-semgrep", '{"results":[]}', 0)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("failed", result.stderr)
        self.assertFalse((self.root / "build/semgrep-venv").exists())

    def test_environment_records_the_running_interpreter_and_installed_version(self):
        output = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "environment"]), \
                patch.object(report.importlib.metadata, "version", return_value="1.177.0"), \
                contextlib.redirect_stdout(output):
            report.main()
        data = json.loads(output.getvalue())
        self.assertEqual(data["semgrep"], "1.177.0")
        self.assertEqual(data["executable"], sys.executable)
        self.assertTrue(data["python"])
        self.assertTrue(data["platform"])

    def test_missing_report_is_diagnostic(self):
        errors = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "failure", str(self.root / "missing")]), \
                contextlib.redirect_stderr(errors):
            report.main()
        self.assertIn("Report is missing or invalid", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
