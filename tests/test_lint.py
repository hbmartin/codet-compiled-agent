"""Check lint failure propagation and tool identity without installing tools."""

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
    def test_missing_tool_does_not_hide_later_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def invoke(command, **kwargs):
                if command[0] == "missing":
                    raise FileNotFoundError("missing")
                return subprocess.CompletedProcess(command, 0, "tool 1.0\n", "")
            tools = [("missing", "--version", "1.0", [["check"]]),
                     ("working", "--version", "1.0", [["check"]])]
            with patch.object(lint, "ROOT", root), patch.object(lint, "commands", return_value=tools), \
                 patch.object(lint, "binary", side_effect=lambda name: name), \
                 patch.object(lint.subprocess, "run", side_effect=invoke):
                self.assertFalse(lint.run("python"))
            report = json.loads((root / "build/lint/python.json").read_text())
            self.assertFalse(report["passed"])
            self.assertTrue(report["results"][1]["passed"])

    def test_wrong_tool_version_or_nonzero_diagnostic_fails(self):
        for version, status in (("wrong version", 0), ("tool 1.0", 1)):
            with self.subTest(version=version, status=status), tempfile.TemporaryDirectory() as directory:
                replies = [subprocess.CompletedProcess([], 0, version, ""),
                           subprocess.CompletedProcess([], status, "diagnostic", "")]
                with patch.object(lint, "ROOT", Path(directory)), \
                     patch.object(lint, "commands", return_value=[("tool", "--version", "1.0", [["check"]])]), \
                     patch.object(lint.subprocess, "run", side_effect=replies):
                    self.assertFalse(lint.run("python"))


if __name__ == "__main__":
    unittest.main()
