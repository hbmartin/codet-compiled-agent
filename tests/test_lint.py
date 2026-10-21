"""Check lint evidence, failure propagation and exact tool identity."""
import json
import os
import shutil
import stat
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
        with patch.object(lint, "_run", side_effect=run), self.assertRaises(KeyboardInterrupt):
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
        with patch.object(lint, "_run", side_effect=[False, True, True, True]) as run:
            self.assertFalse(lint.run_groups(lint.GROUPS))
        self.assertEqual(run.call_count, 4)

    def test_unknown_tool_does_not_stop_later_tools_or_groups(self):
        def commands(group):
            name = "unknown-tool" if group == "format" else "ruff"
            return [(name, "--version", None, [[]])]
        with patch.object(lint, "commands", side_effect=commands), \
                patch.object(lint.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "ruff 0.17.1", "")):
            self.assertFalse(lint.run_groups(lint.GROUPS))
        self.assertIn("Unknown lint tool", self.report("format")["results"][0]["failure"])
        self.assertTrue(all(self.report(group)["passed"] for group in lint.GROUPS[1:]))

    def test_groups_invalidate_once_and_publish_private_reports(self):
        requested = [("ruff", "--version", "0.17.1", [[]])]
        with patch.object(lint, "commands", return_value=requested), \
                patch.object(lint.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "ruff 0.17.1", "")), \
                patch.object(lint, "write_report", wraps=lint.write_report) as write:
            self.assertTrue(lint.run_groups(lint.GROUPS))
        for group in lint.GROUPS:
            calls = [call for call in write.call_args_list if call.args[0] == group]
            self.assertEqual(len(calls), 2)
            self.assertFalse(calls[0].args[1]["passed"])
            self.assertTrue(calls[1].args[1]["passed"])
            self.assertEqual(stat.S_IMODE((self.root / "build/lint" / (group + ".json")).stat().st_mode), 0o600)

    def test_failed_atomic_publish_retains_initial_failure_and_cleans_temporary(self):
        lint.invalidate(("python",))
        with patch.object(lint.os, "replace", side_effect=OSError("publish failed")), self.assertRaises(OSError):
            lint.write_report("python", dict(passed=True))
        self.assertFalse(self.report("python")["passed"])
        self.assertEqual([p.name for p in (self.root / "build/lint").iterdir()], ["python.json"])


class Provisioning(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        source = Path(__file__).resolve().parents[1]
        for name in ("Makefile", "toolchain.mk", ".go-version"):
            shutil.copy(source / name, self.root / name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.log = self.root / "commands.log"
        self.python = self.bin / "python"
        self.python.write_text(f"#!{sys.executable}\n" + r'''
import os, pathlib, shutil, subprocess, sys
args = sys.argv[1:]
if args[0] == '-c':
    version = tuple(map(int, os.environ['FAKE_PYTHON_VERSION'].split('.')))
    raise SystemExit(subprocess.run([sys.executable, '-c', 'import sys;sys.version_info=' + repr(version) + ';' + args[1]]).returncode)
if args[:2] == ['-m', 'venv']:
    folder = pathlib.Path(args[2]) / 'bin'
    folder.mkdir(parents=True)
    shutil.copy(__file__, folder / 'python')
with open(os.environ['COMMAND_LOG'], 'a') as log:
    log.write('python ' + ' '.join(args) + '\n')
''')
        self.python.chmod(0o700)
        go = self.bin / "go"
        go.write_text(f"#!{sys.executable}\n" + r'''
import os, sys
with open(os.environ['COMMAND_LOG'], 'a') as log:
    log.write(os.environ.get('GOTOOLCHAIN', '') + ' go ' + ' '.join(sys.argv[1:]) + '\n')
if sys.argv[1] == 'version': print(os.environ['FAKE_GO_BANNER'])
''')
        go.chmod(0o700)
        shellcheck = self.bin / "shellcheck"
        shellcheck.write_text("#!/bin/sh\nexit 0\n")
        shellcheck.chmod(0o700)
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        FAKE_PYTHON_VERSION="3.12.0", FAKE_GO_BANNER="go version go1.27.2 linux/amd64",
                        COMMAND_LOG=str(self.log), GOTOOLCHAIN="local")

    def run_target(self, target):
        return subprocess.run(["make", target, "LINT_PYTHON=" + str(self.python),
                               "SEMGREP_PYTHON=" + str(self.python)], cwd=self.root,
                              env=self.env, text=True, capture_output=True, timeout=10)

    def test_exact_go_version_is_required_before_environment_or_installation(self):
        for banner in ("go version go1.20.9 linux/amd64", "go version go1.27.3 linux/amd64", "malformed"):
            with self.subTest(banner=banner):
                self.env["FAKE_GO_BANNER"] = banner
                result = self.run_target("fetch-lint-tools")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Go 1.27.2 is required", result.stderr)
                self.assertNotIn("install", self.log.read_text())
                self.assertFalse((self.root / "build/lint-venv").exists())

    def test_matching_go_overrides_inherited_local_and_installs_pinned_tools(self):
        result = self.run_target("fetch-lint-tools")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.log.read_text()
        self.assertIn("go1.27.2 go install github.com/rhysd/actionlint/cmd/actionlint@v1.7.12", commands)
        self.assertIn("go1.27.2 go install mvdan.cc/sh/v3/cmd/shfmt@v3.14.1", commands)
        self.assertEqual((self.root / "build/lint/go-version.txt").read_text(), self.env["FAKE_GO_BANNER"] + "\n")

    def test_other_python_minors_fail_before_provisioning(self):
        for version in ("3.9.0", "3.13.0", "3.14.0"):
            for target in ("fetch-lint-tools", "fetch-semgrep"):
                with self.subTest(version=version, target=target):
                    self.env["FAKE_PYTHON_VERSION"] = version
                    result = self.run_target(target)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Provisioning requires Python 3.12", result.stderr)
                    self.assertFalse((self.root / "build").exists())

    def test_python_312_patch_versions_can_provision(self):
        self.env["FAKE_PYTHON_VERSION"] = "3.12.99"
        self.assertEqual(self.run_target("fetch-lint-tools").returncode, 0)
        self.assertEqual(self.run_target("fetch-semgrep").returncode, 0)


if __name__ == "__main__":
    unittest.main()
