"""Exercise coverage and enforcement, without depending on installed analyzers."""
import importlib.util
import json
from pathlib import Path
import plistlib
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("analyze_modern", Path(__file__).resolve().parents[1] /
                                            "scripts/analyze-modern.py")
analysis = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analysis)


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "src/modern").mkdir(parents=True)
        self.sources = [self.root / p for p in ("src/modern/main.cpp", "src/text_compat.c")]
        for source in self.sources:
            source.write_text("int value(void) { return 0; }\n")
        self.entries = [dict(directory=str(self.root), file=str(p),
                             arguments=["clang", "-c", str(p), "-o", "unused.o"])
                        for p in self.sources]
        self.database = self.root / "compile_commands.json"
        self.database.write_text(json.dumps(self.entries))
        self.folder = self.root / "reports"
        self.folder.mkdir()

    def test_selects_cpp_and_text_helper_excluding_dependencies_and_tests(self):
        extra = [dict(self.entries[0], file=str(self.root / p))
                 for p in (".deps/vendor.cpp", "tests/unit.cpp", "src/subsystems/tools.c")]
        self.database.write_text(json.dumps(self.entries + extra))
        selected = analysis.select_entries(self.database, self.root)
        self.assertEqual({e["file"] for e in selected}, set(map(str, self.sources)))

    def test_missing_text_helper_and_empty_database_fail(self):
        for entries in (self.entries[:1], []):
            self.database.write_text(json.dumps(entries))
            with self.assertRaisesRegex(ValueError, "Missing compilation commands"):
                analysis.select_entries(self.database, self.root)

    def test_new_application_source_requires_database_entry(self):
        (self.root / "src/modern/new.cc").write_text("int extra;\n")
        with self.assertRaisesRegex(ValueError, "new.cc"):
            analysis.select_entries(self.database, self.root)

    def test_relative_paths_and_command_string(self):
        data = [dict(directory=".", file=str(p.relative_to(self.root)),
                     command=f"clang -c {p} -o unused.o") for p in self.sources]
        self.database.write_text(json.dumps(data))
        self.assertEqual(len(analysis.select_entries(self.database, self.root)), 2)

    def test_conflicting_commands_fail(self):
        self.database.write_text(json.dumps(self.entries + [dict(self.entries[0], arguments=["gcc"])]))
        with self.assertRaisesRegex(ValueError, "Conflicting"):
            analysis.select_entries(self.database, self.root)

    def test_strips_only_object_and_dependency_flags(self):
        entry = dict(arguments=["clang", "-Iinclude", "-DDEBUG", "-std=c++17", "-c", "file.cpp",
                                "-o", "file.o", "-MD", "-MF", "file.d", "-MTfile.o"])
        self.assertEqual(analysis.compiler_arguments(entry),
                         ["clang", "-Iinclude", "-DDEBUG", "-std=c++17", "file.cpp"])

    def test_timeout_kills_command(self):
        run = analysis.capture([sys.executable, "-c", "import time; time.sleep(30)"], self.root, .05)
        self.assertEqual(run["failure"], "timeout")
        self.assertNotEqual(run["exit_code"], 0)

    def test_missing_executable_fails(self):
        run = analysis.capture([str(self.root / "no-such-tool")], self.root, 1)
        self.assertIsNotNone(run["failure"])

    def analyzer_run(self, payload, exit_code=0):
        def capture(command, cwd, timeout):
            report = Path(command[command.index("-o") + 1])
            report.write_bytes(plistlib.dumps(payload) if isinstance(payload, dict) else payload)
            return dict(command=command, exit_code=exit_code, stdout="", stderr="", failure=None)
        with patch.object(analysis, "capture", side_effect=capture):
            return analysis.clang_one("clang-analyzer", self.entries[0], "clang", self.folder,
                                      self.database, 5, self.root)

    def test_analyzer_diagnostics_fail_even_with_zero_exit(self):
        run = self.analyzer_run({"diagnostics": [{"check_name": "core.NullDereference"}]})
        self.assertEqual(run["status"], "findings")

    def test_malformed_report_and_tool_failure_fail(self):
        self.assertEqual(self.analyzer_run(b"invalid")["status"], "failed")
        self.assertEqual(self.analyzer_run({"diagnostics": []}, exit_code=2)["status"], "failed")

    def test_clean_report_passes(self):
        self.assertEqual(self.analyzer_run({"diagnostics": []})["status"], "passed")

    def test_cppcheck_incomplete_coverage_and_parser_errors_fail(self):
        for stdout, xml in (("", '<results><errors/></results>'),
                            ("\n".join("Checking " + e["file"] + " ..." for e in self.entries),
                             '<results><errors><error id="syntaxError"><location file=".deps/header.h"/></error></errors></results>'),
                            ("", '<results>')):
            with patch.object(analysis, "capture", return_value=dict(exit_code=0, failure=None,
                              stdout=stdout, stderr=xml)):
                run = analysis.cppcheck(self.entries, "cppcheck", self.folder, self.database,
                                         5, self.root, 1)
            self.assertEqual(run["status"], "failed")

    def test_tidy_diagnostics_and_malformed_report_fail(self):
        import yaml
        for payload in ({"Diagnostics": [{"DiagnosticName": "bugprone-use-after-move"}]}, [], "[invalid"):
            def capture(command, cwd, timeout):
                output = next(arg.split("=", 1)[1] for arg in command if arg.startswith("--export-fixes="))
                Path(output).write_text(payload if isinstance(payload, str) else yaml.safe_dump(payload))
                return dict(command=command, exit_code=1, failure=None, stdout="", stderr="")
            with patch.object(analysis, "capture", side_effect=capture):
                run = analysis.clang_one("clang-tidy", self.entries[0], "clang-tidy", self.folder,
                                          self.database, 5, self.root)
            self.assertNotEqual(run["status"], "passed")

    def test_cppcheck_diagnostics_fail_even_with_zero_exit(self):
        stdout = "\n".join("Checking " + e["file"] + " ..." for e in self.entries)
        xml = '<results><errors><error id="arrayIndexOutOfBounds"><location file="src/text_compat.c"/></error></errors></results>'
        with patch.object(analysis, "capture", return_value=dict(exit_code=0, failure=None,
                          stdout=stdout, stderr=xml)):
            run = analysis.cppcheck(self.entries, "cppcheck", self.folder, self.database, 5, self.root, 1)
        self.assertEqual(run["status"], "findings")

    def test_compatibility_requires_equal_preprocessing(self):
        vendor = self.root / "vendor"
        header = vendor / "toml++/impl/preprocessor.hpp"
        header.parent.mkdir(parents=True)
        header.write_text("#define FLAGS(op) operator TOML_CONCAT(op, =)\n")
        entries = [dict(self.entries[0], arguments=self.entries[0]["arguments"] + ["-I", str(vendor)])]
        runs = [dict(command=["clang"], exit_code=0, failure=None, stdout=text, stderr="")
                for text in ("original", "different")]
        with patch.object(analysis, "capture", side_effect=runs):
            with self.assertRaisesRegex(ValueError, "preprocessing differs"):
                analysis.cppcheck_compatibility(entries, self.folder, 5)

    def test_compatibility_refuses_unrecognized_header(self):
        vendor = self.root / "vendor"
        header = vendor / "toml++/impl/preprocessor.hpp"
        header.parent.mkdir(parents=True)
        header.write_text("#define DIFFERENT_MACRO 42\n")
        entries = [dict(self.entries[0], arguments=self.entries[0]["arguments"] + ["-I", str(vendor)])]
        with self.assertRaisesRegex(ValueError, "compatibility review required"):
            analysis.cppcheck_compatibility(entries, self.folder, 5)

    def test_missing_tool_does_not_pass(self):
        with patch.object(analysis, "capture", return_value=dict(exit_code=None, failure="missing", stdout="")):
            summary = analysis.run_tool("clang-analyzer", self.entries, self.folder, 1, 5, self.root)
        self.assertFalse(summary["passed"])

    def test_gcc_database_is_rejected_by_clang_analyzer(self):
        with patch.object(analysis, "capture", return_value=dict(exit_code=0, failure=None, stdout="gcc 14")):
            summary = analysis.run_tool("clang-analyzer", self.entries, self.folder, 1, 5, self.root)
        self.assertFalse(summary["passed"])

    def test_source_mutation_during_scan_is_rejected(self):
        def tool(name, *args):
            self.sources[0].write_text("changed during scan")
            return dict(tool=name, passed=True, findings=0)
        with patch.object(analysis, "run_tool", side_effect=tool):
            result = analysis.run_all(self.entries, self.folder, ("clang-tidy",), 1, 5, self.root)
        self.assertFalse(result["passed"])
        self.assertEqual(result["changed_sources"], [str(self.sources[0])])

    def test_aggregation_continues_after_failure_and_exception(self):
        def tool(name, *args):
            if name == "clang-tidy":
                raise RuntimeError("broken report")
            return dict(tool=name, passed=name != "cppcheck", findings=1 if name == "cppcheck" else 0)
        with patch.object(analysis, "run_tool", side_effect=tool) as run:
            result = analysis.run_all(self.entries, self.folder, analysis.TOOLS, 1, 5, self.root)
        self.assertEqual(run.call_count, 3)
        self.assertFalse(result["passed"])
        self.assertEqual(len(result["tools"]), 3)


if __name__ == "__main__":
    unittest.main()
