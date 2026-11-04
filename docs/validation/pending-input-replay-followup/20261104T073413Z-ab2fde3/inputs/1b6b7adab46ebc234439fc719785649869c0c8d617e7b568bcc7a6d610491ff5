"""Native assertion controls, kept distinct from Python evidence controls."""

import json
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

from evidence_benchmarks import replace_once
from process_supervision import run as supervised_run
from validation_evidence import digest


def _run(argv, cwd, timeout):
    result = supervised_run(argv, cwd=cwd, capture_output=True, text=True, timeout=timeout, cleanup_grace=4)
    return dict(argv=argv, cwd=str(cwd), exit=result.returncode, log=result.stdout + result.stderr)


def _test(binary, target, directory):
    xml = directory / "result.xml"
    xml.unlink(missing_ok=True)
    result = _run([str(binary), "--gtest_filter=" + target, "--gtest_output=xml:" + str(xml)], directory, 30)
    if not xml.is_file():
        raise ValueError("Native control did not produce a test result: " + target)
    cases = list(ET.parse(xml).getroot().iter("testcase"))
    if len(cases) != 1 or cases[0].get("classname") + "." + cases[0].get("name") != target or cases[0].get("status") != "run":
        raise ValueError("Native control did not execute exactly the intended test")
    result["assertions"] = [f.get("message", "") for f in cases[0].findall("failure")]
    return result


INTEGRATION_RUNNER = r'''
import importlib.util,json,sys,unittest
from pathlib import Path
root,binary,output=sys.argv[1:]
path=Path(root)/'tests/cli_integration.py'
sys.path.insert(0,str(path.parent));sys.argv=[str(path),binary]
spec=importlib.util.spec_from_file_location('pending_native_control',path)
module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
module.CliTests.pending_outcomes=('quit',)
suite=unittest.defaultTestLoader.loadTestsFromName('CliTests.test_frozen_submission_obeys_turn_outcome_and_services_controls',module)
if suite.countTestCases()!=1:raise ValueError('Pending-control target was not discovered exactly once')
result=unittest.TextTestRunner(verbosity=2).run(suite)
Path(output).write_text(json.dumps(dict(tests=result.testsRun,failures=[trace for _,trace in result.failures],errors=[trace for _,trace in result.errors])))
raise SystemExit(int(not result.wasSuccessful()))
'''


def _integration(root, binary, directory):
    result_path = directory / "integration-result.json"
    result_path.unlink(missing_ok=True)
    result = _run([sys.executable, "-c", INTEGRATION_RUNNER, str(root), str(binary), str(result_path)], root, 30)
    if not result_path.is_file():
        raise ValueError("Pending-control integration result missing")
    value = json.loads(result_path.read_text())
    if value.get("tests") != 1 or value.get("errors"):
        raise ValueError("Pending-control target did not execute cleanly: " + repr(value))
    result["assertions"] = value["failures"]
    return result


def run_controls(root, folder, build, ninja):
    database = json.loads((build / "compile_commands.json").read_text())
    def link_command(target):
        commands = subprocess.check_output([ninja, "-C", str(build), "-t", "commands", target], text=True, timeout=30)
        link = shlex.split(commands.splitlines()[-1])
        if "&&" in link:
            link = link[link.index("&&") + 1:]
            if "&&" in link:
                link = link[:link.index("&&")]
        return link
    def compile_mutant(directory, binary_target, binary_name, entry_name, source_name, before, after):
        link = link_command(binary_target)
        source = directory / source_name
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text(replace_once((root / source_name).read_text(), before, after))
        entry = next(e for e in database if e["file"] == str(root / entry_name))
        argv = list(entry.get("arguments") or shlex.split(entry["command"]))
        original_object = argv[argv.index("-o") + 1]
        obj = directory / "mutant.o"
        argv[argv.index("-o") + 1] = str(obj)
        if source_name.endswith(".cpp"):
            argv[argv.index(str(root / entry_name))] = str(source)
        elif source_name.startswith("tests/"):
            copied_entry = directory / entry_name
            copied_entry.parent.mkdir(parents=True, exist_ok=True)
            copied_entry.write_text((root / entry_name).read_text())
            argv[argv.index(str(root / entry_name))] = str(copied_entry)
        argv[1:1] = ["-I" + str(directory / "include"), "-I" + str(root / "tests")]
        compiled = _run(argv, entry["directory"], 600)
        if compiled["exit"]:
            raise ValueError("Native mutant compilation failed: " + repr(compiled))
        binary = directory / binary_name
        mutated_link = [p for p in link if p != original_object]
        mutated_link.insert(1, str(obj))
        mutated_link[mutated_link.index("-o") + 1] = str(binary)
        linked = _run(mutated_link, build, 600)
        if linked["exit"]:
            raise ValueError("Native mutant linking failed: " + repr(linked))
        return source, binary, compiled, linked
    cases = [
        ("deferred", "tests/diagnostic_lifecycle_test.cpp", "include/codet/detail/line_prompt.hpp",
         "if (submission_result == DiagnosticResult::deferred) {",
         "if (((void)submission_result, false)) {",
         "NativeRecordLifecycle.DeferredSubmissionRetainsRawBytesAndOriginalRequest"),
        ("pending-visible", "tests/diagnostic_lifecycle_test.cpp", "include/codet/detail/line_prompt.hpp",
         "frozen_view(submission)", "std::string_view(submission.tail)",
         "NativeRecordLifecycle.KnownWidthPendingLineIsVisible"),
        ("pending-interruption", "tests/diagnostic_lifecycle_test.cpp", "include/codet/detail/line_prompt.hpp",
         "  void observe_boundary() noexcept {", "  void observe_boundary() noexcept { return;",
         "NativeRecordLifecycle.InterruptedSubmissionRepaintsAfterItsSeparatorWasDeferred"),
        ("separator", "src/modern/platform.cpp", "src/modern/platform.cpp",
         "record.request_separator(io_->line_output->before_output(native, inspection.identity));",
         "(void)io_->line_output->before_output(native, inspection.identity);",
         "NativeRecordLifecycle.InjectedStdoutSharesSeparatorWithNativeStderr"),
        ("printer", "tests/diagnostic_lifecycle_test.cpp", "tests/diagnostic_lifecycle_test.cpp",
         "(void)::dup2(out, STDOUT_FILENO);\n", "(void)out;\n",
         "DiagnosticFixture.FailureDetailsReachParentLogAndDoNotContaminatePty"),
        ("pid-guard", "tests/cli_diagnostics_test.cpp", "tests/process_helper.hpp",
         "if (child <= 0)", "if (child == 0)",
         "ProcessHelperSafety.InvalidPidsNeverReachWaitOrSignal"),
        ("refresh", "tests/approval_render_test.cpp", "include/codet/detail/curses_render.hpp",
         "success_ && (ops_.refresh_screen(ops_.context) != ERR)",
         "(ops_.refresh_screen(ops_.context) != ERR) && success_",
         "ApprovalCurses.OrdinaryNativeFailuresKeepStateAndNextFrameRecovers"),
        ("inspection", "src/modern/inspection.cpp", "src/modern/inspection.cpp",
         "count = static_cast<size_t>(std::min<uint64_t>(count, size_ - offset_));",
         "(void)count;", "InspectionRead.OpeningSizeBoundsReadsSeeksAndRejectsTruncation"),
        ("submission-allocation", "tests/publication_fault_test.cpp", "include/codet/detail/line_prompt.hpp",
         "std::move(tail), std::move(view),",
         "std::move(tail), view,",
         "LineSubmissionFault.AllocationFailureCannotConsumeAnUnownedLine"),
    ]
    results = []
    for name, entry_name, source_name, before, after, target in cases:
        directory = folder / ("native-control-" + name)
        directory.mkdir()
        binary_target = "codet_publication_fault_tests" if target.startswith("LineSubmissionFault.") else "codet_tests"
        baseline = _test(build / binary_target, target, directory)
        if baseline["exit"] or baseline["assertions"]:
            raise ValueError("Native control baseline failed: " + target)
        source, binary, compiled, linked = compile_mutant(directory, binary_target, "control-tests", entry_name, source_name, before, after)
        mutant = _test(binary, target, directory)
        markers = {"deferred": "deferred", "pending-visible": "Frozen submitted line was not displayed",
                   "pending-interruption": "read()", "separator": 'read()', "printer": "Expected equality of these values", "pid-guard": "unexpected wait",
                   "refresh": "Refresh after failed drawing", "inspection": "handle->read",
                   "submission-allocation": "Allocation consumed a line without pending ownership"}
        if (mutant["exit"] != 1 or not mutant["assertions"] or
                any("C++ exception" in message or "unknown file" in message for message in mutant["assertions"]) or
                not any(markers[name] in message for message in mutant["assertions"])):
            raise ValueError("Native mutant did not fail its intended assertion: " + target + repr(mutant))
        results.append(dict(name=name, target=target, mutation=dict(before=before, after=after, original_sha256=digest(root / source_name), mutant_sha256=digest(source)), binary_sha256=digest(binary), baseline=baseline, compile=compiled, link=linked, mutant=mutant))
    directory = folder / "native-control-pending-keyboard"
    directory.mkdir()
    baseline = _integration(root, build / "codet", directory)
    if baseline["exit"] or baseline["assertions"]:
        raise ValueError("Pending keyboard control baseline failed")
    before = "  std::optional<detail::InputLine> active_input(bool &closed) {\n"
    after = before + "    if (line_ && line_->submission_pending()) {\n      (void)detail::retry_terminal_input(EAGAIN, read_cancel_ ? &read_cancel_->requested : nullptr);\n      return line_->try_take();\n    }\n"
    source, binary, compiled, linked = compile_mutant(directory, "codet_tests", "codet_tests", "src/modern/cli.cpp", "src/modern/cli.cpp", before, after)
    placeholder = directory / "codet"
    shutil.copy2(build / "codet", placeholder)
    mutant = _integration(root, placeholder, directory)
    marker = "Pending-input keyboard gate was not reached before diagnostic release: b's'"
    if mutant["exit"] != 1 or len(mutant["assertions"]) != 1 or marker not in mutant["assertions"][0]:
        raise ValueError("Pending keyboard mutant did not fail its intended assertion: " + repr(mutant))
    results.append(dict(name="pending-keyboard", target="CliTests.test_frozen_submission_obeys_turn_outcome_and_services_controls", mutation=dict(before=before, after=after, original_sha256=digest(root / "src/modern/cli.cpp"), mutant_sha256=digest(source)), binary_sha256=digest(binary), baseline=baseline, compile=compiled, link=linked, mutant=mutant))
    return results
