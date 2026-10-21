"""Exercise report formatting and environment provenance without invoking Semgrep."""
import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
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

    def test_environment_records_relative_interpreter_and_installed_version(self):
        output = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "environment"]), \
                patch.object(sys, "executable", str(ROOT / "build/semgrep-venv/bin/python")), \
                patch.object(report.importlib.metadata, "version", return_value="1.177.0"), \
                contextlib.redirect_stdout(output):
            report.main()
        data = json.loads(output.getvalue())
        self.assertEqual(data["semgrep"], "1.177.0")
        self.assertEqual(data["executable"], "build/semgrep-venv/bin/python")
        self.assertTrue(data["python"])
        self.assertTrue(data["platform"])

    def test_external_interpreter_does_not_publish_its_parent_directories(self):
        with patch.object(sys, "executable", str(self.root / "private/bin/python3")):
            self.assertEqual(report.interpreter_path(), "<external>/python3")

    def test_interpreter_symlink_keeps_its_venv_path(self):
        venv = self.root / "build/semgrep-venv/bin/python"
        venv.parent.mkdir(parents=True)
        venv.symlink_to(sys.executable)
        with patch.object(report, "__file__", str(self.root / "scripts/semgrep-report.py")), \
                patch.object(sys, "executable", str(venv)):
            self.assertEqual(report.interpreter_path(), "build/semgrep-venv/bin/python")

    def test_missing_report_is_diagnostic(self):
        errors = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "failure", str(self.root / "missing")]), \
                contextlib.redirect_stderr(errors):
            report.main()
        self.assertIn("Report is missing or invalid", errors.getvalue())

    def test_failure_pretty_prints_json_without_changing_report(self):
        path = self.root / "report.json"
        content = '{"errors":[{"message":"failure detail"}]}'
        path.write_text(content)
        errors = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "failure", str(path)]), \
                contextlib.redirect_stderr(errors):
            report.main()
        self.assertIn(json.dumps(json.loads(content), indent=2), errors.getvalue())
        self.assertEqual(path.read_text(), content)

    def test_invalid_report_is_diagnostic_and_remains_available(self):
        path = self.root / "report.json"
        path.write_text("{invalid")
        errors = io.StringIO()
        with patch.object(sys, "argv", ["semgrep-report.py", "failure", str(path)]), \
                contextlib.redirect_stderr(errors):
            report.main()
        self.assertIn("Report is missing or invalid", errors.getvalue())
        self.assertEqual(path.read_text(), "{invalid")

    def run_make(self, payload, status):
        for name in ("Makefile", "toolchain.mk"):
            shutil.copy(ROOT / name, self.root / name)
        scripts = self.root / "scripts"
        scripts.mkdir(exist_ok=True)
        shutil.copy(ROOT / "scripts/semgrep-report.py", scripts)
        tool = self.root / "semgrep"
        tool.write_text(f"#!{sys.executable}\n" + r'''
import os, sys
if '--version' in sys.argv:
    print('1.177.0')
else:
    print(os.environ['FAKE_REPORT'], end='')
    print('scan stderr detail', file=sys.stderr)
    raise SystemExit(int(os.environ['FAKE_STATUS']))
''')
        tool.chmod(0o700)
        return subprocess.run(["make", "check-semgrep", "SEMGREP=" + str(tool),
                               "SEMGREP_PYTHON=" + sys.executable], cwd=self.root,
                              env=dict(os.environ, FAKE_REPORT=payload, FAKE_STATUS=str(status)),
                              text=True, capture_output=True, timeout=10)

    def test_make_preserves_scan_evidence_and_original_failure_status(self):
        for payload, status in (('{"errors":[{"message":"scan failure detail"}]}', 7),
                                ("{invalid", 9)):
            with self.subTest(status=status):
                run = self.run_make(payload, status)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn("Error " + str(status), run.stderr)
                self.assertIn("scan stderr detail", run.stderr)
                self.assertIn("Semgrep check failed", run.stderr)
                self.assertIn("scan failure detail" if status == 7 else "Report is missing or invalid",
                              run.stderr)
                self.assertEqual((self.root / "build/semgrep/scan.json").read_text(), payload)
                self.assertEqual((self.root / "build/semgrep/scan.stderr.log").read_text(),
                                 "scan stderr detail\n")

    def test_make_success_retains_report_and_removes_obsolete_evidence(self):
        folder = self.root / "build/semgrep"
        folder.mkdir(parents=True)
        for name in ("tests.json", "test.stderr.log", "test.manifest.json", "scan.manifest.json"):
            (folder / name).write_text("obsolete")
        run = self.run_make('{"results":[]}', 0)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertNotIn("Semgrep check failed", run.stderr)
        self.assertEqual(sorted(p.name for p in folder.iterdir()), ["scan.json", "scan.stderr.log"])
        self.assertFalse((self.root / "build/semgrep-venv").exists())


if __name__ == "__main__":
    unittest.main()
