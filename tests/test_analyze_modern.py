"""Exercise coverage and enforcement, without depending on installed analyzers."""
import importlib.util
import json
import os
import signal
import subprocess
import threading
import time
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
        analysis.CANCEL.clear()
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
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
        def complete(*args):
            (self.folder / "checkers.txt").write_text("No critical errors encountered.\nYes  CheckBufferOverrun::arrayIndex\nYes  CheckUninitVar::check\n")
            (self.folder / "cache/result.a1").write_text("<analyzerinfo><hash>1</hash></analyzerinfo>")
            return dict(exit_code=0, failure=None, stdout=stdout, stderr=xml)
        with patch.object(analysis, "capture", side_effect=complete):
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

    def checked_cppcheck(self, xml, evidence=True):
        def complete(*args):
            if evidence:
                (self.folder / "checkers.txt").write_text("No critical errors encountered.\nYes  CheckBufferOverrun::arrayIndex\nYes  CheckUninitVar::check\n")
                (self.folder / "cache/result.a1").write_text("<analyzerinfo><hash>1</hash></analyzerinfo>")
            return dict(exit_code=0, failure=None, stdout="Checking " + self.entries[0]["file"] + " ...", stderr=xml)
        with patch.object(analysis, "capture", side_effect=complete):
            return analysis.cppcheck(self.entries[:1], "cppcheck", self.folder, self.database, 5, self.root, 1)

    def test_progress_and_empty_cache_never_prove_completion(self):
        run = self.checked_cppcheck('<results><errors/></results>', False)
        self.assertEqual(run['status'], 'failed')
        self.assertEqual(run['completed_sources'], [])

    def test_all_fatal_ids_fail_even_when_inside_dependencies(self):
        for id in analysis.FATAL_IDS | {'internalFutureError'}:
            with self.subTest(id=id):
                xml = f'<results><errors><error id="{id}" severity="information"><location file=".deps/vendor.h"/></error></errors></results>'
                self.assertEqual(self.checked_cppcheck(xml)['status'], 'failed')

    def test_suppressed_fatal_still_fails_from_safety_evidence(self):
        xml = '<results><errors><error id="checkersReport" severity="information" msg="Active checkers: There was critical errors"/></errors></results>'
        result = self.checked_cppcheck(xml)
        self.assertEqual(result['failure'], 'Parser or analysis failure')
        self.assertTrue(result['completion_proof']['critical_error'])

    def test_dependency_findings_retained_without_gating_but_call_locations_gate(self):
        xml = '<results><errors><error id="arrayIndexOutOfBounds" severity="error" file0="src/modern/main.cpp"><location file=".deps/vendor.h"/></error></errors></results>'
        run = self.checked_cppcheck(xml)
        self.assertEqual(run['status'], 'passed')
        self.assertEqual(len(run['all_diagnostics']), 1)
        xml = xml.replace('</error>', '<location file="src/modern/main.cpp"/></error>')
        self.assertEqual(self.checked_cppcheck(xml)['status'], 'findings')

    def test_first_party_resolves_alias_roots(self):
        alias = self.root / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        self.assertTrue(analysis.first_party(self.sources[0], alias))
        self.assertFalse(analysis.first_party(self.root / '.deps/vendor.h', alias))

    def test_clang_dependency_findings_and_first_party_call_paths(self):
        files = [str(self.root / '.deps/vendor.h'), str(self.sources[0])]
        diagnostic = {'check_name': 'core.NullDereference', 'location': {'file': 0}}
        run = self.analyzer_run({'files': files, 'diagnostics': [diagnostic]})
        self.assertEqual(run['status'], 'passed')
        self.assertEqual(len(run['all_diagnostics']), 1)
        diagnostic['path'] = [{'location': {'file': 1}}]
        self.assertEqual(self.analyzer_run({'files': files, 'diagnostics': [diagnostic]})['status'], 'findings')

    def test_all_include_spellings_are_replaced_and_audited(self):
        vendor = self.root / 'vendor with spaces'
        header = vendor / 'toml++/impl/preprocessor.hpp'
        header.parent.mkdir(parents=True)
        header.write_text('#define FLAGS(op) operator TOML_CONCAT(op, =)\n')
        alias = self.root / 'vendor-alias'
        alias.symlink_to(vendor, target_is_directory=True)
        forms = [['-I'+str(vendor)], ['-I', str(vendor)], ['-isystem'+str(vendor)],
                 ['-isystem', str(vendor)], ['-I', 'vendor with spaces'], ['-I', str(alias)]]
        for i, form in enumerate(forms):
            with self.subTest(form=form):
                entry = dict(self.entries[0], arguments=['clang'] + form + ['-c', str(self.sources[0])])
                result = dict(command=['clang'], exit_code=0, failure=None, stdout='identical', stderr='')
                destination = self.folder / str(i)
                with patch.object(analysis, 'capture', return_value=result) as called:
                    changed = analysis.cppcheck_compatibility([entry], destination, 1)
                self.assertEqual(called.call_count, 2)
                self.assertNotEqual(changed[0]['arguments'], entry['arguments'])
                audit = json.loads((destination/'compatibility.json').read_text())
                self.assertEqual(audit['headers'][0]['replacements'], 1)
                self.assertEqual(len(audit['headers'][0]['comparisons']), 1)

    def test_required_compatibility_audit_cannot_be_empty(self):
        with self.assertRaisesRegex(ValueError, 'audit is empty'):
            analysis.cppcheck_compatibility(self.entries, self.folder, 1, require_adapter=True)

    def test_explicit_missing_clang_tidy_never_falls_back(self):
        with patch.dict(os.environ, {'CLANG_TIDY': 'requested-missing-tool'}):
            with patch.object(analysis.shutil, 'which', return_value=None) as which:
                self.assertEqual(analysis.select_binary('clang-tidy'), 'requested-missing-tool')
                which.assert_called_once_with('requested-missing-tool')

    def test_timeout_race_preserves_output_and_reaps(self):
        kill = analysis.os.killpg
        def disappeared(pid, sig):
            kill(pid, sig)
            raise ProcessLookupError()
        with patch.object(analysis.os, 'killpg', side_effect=disappeared):
            result = analysis.capture([sys.executable, '-c', 'import time; print("retained",flush=True); time.sleep(10)'], self.root, .3)
        self.assertEqual(result['failure'], 'timeout')
        self.assertIn('retained', result['stdout'])
        self.assertIsNotNone(result['exit_code'])

    def test_interrupt_event_cleans_up_running_analyzer(self):
        timer = threading.Timer(.3, analysis.CANCEL.set)
        timer.start()
        try:
            start = time.monotonic()
            result = analysis.capture([sys.executable, '-c', 'import time; print("started",flush=True); time.sleep(10)'], self.root, 10)
            self.assertEqual(result['failure'], 'interrupted')
            self.assertLess(time.monotonic()-start, 2.6)
            self.assertIsNotNone(result['exit_code'])
        finally:
            timer.join()
            analysis.CANCEL.clear()

    def test_actual_sigint_cancels_workers_and_queued_work(self):
        driver = self.root / 'interrupt.py'
        marker = self.root / 'started'
        driver.write_text("import importlib.util,sys,signal\n"
            + f"spec=importlib.util.spec_from_file_location('runner',{str(Path(analysis.__file__))!r})\n"
            + "runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)\n"
            + "def run(i):\n"
            + f" return runner.capture([sys.executable,'-c',\"from pathlib import Path; import time; Path({str(marker)!r}).touch(); time.sleep(30)\"],{str(self.root)!r},30)\n"
            + "try: runner.parallel_map(run,range(8),1)\n"
            + "except KeyboardInterrupt: sys.exit(130)\n")
        child = subprocess.Popen([sys.executable, str(driver)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic()+5
            while not marker.exists() and time.monotonic()<deadline:
                time.sleep(.01)
            self.assertTrue(marker.exists())
            started = time.monotonic()
            child.send_signal(signal.SIGINT)
            out, err = child.communicate(timeout=3)
            self.assertEqual(child.returncode, 130, err)
            self.assertLess(time.monotonic()-started, 2.6)
        finally:
            if child.poll() is None:
                child.kill(); child.communicate(timeout=3)

    def test_escaped_descendant_pipes_cannot_block_cleanup_forever(self):
        code = 'import subprocess,sys,time; subprocess.Popen([sys.executable,"-c","import os,time; os.setsid(); time.sleep(2.5)"]); print("partial",flush=True); time.sleep(10)'
        start = time.monotonic()
        result = analysis.capture([sys.executable, '-c', code], self.root, .2)
        self.assertEqual(result['failure'], 'timeout')
        self.assertIn('partial', result['stdout'])
        self.assertLess(time.monotonic()-start, 2.6)
        self.assertIsNotNone(result['exit_code'])


if __name__ == "__main__":
    unittest.main()
