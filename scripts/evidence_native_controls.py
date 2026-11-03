"""Native assertion controls, kept distinct from Python evidence controls."""

import json
import shlex
import subprocess
import xml.etree.ElementTree as ET

from evidence_benchmarks import replace_once
from validation_evidence import digest


def _run(argv, cwd, timeout):
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, timeout=timeout)
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
    cases = [
        ("deferred", "tests/diagnostic_lifecycle_test.cpp", "include/codet/detail/line_prompt.hpp",
         "if (submitted() == DiagnosticResult::deferred)\n      return {};",
         "if (false)\n      return {};",
         "NativeRecordLifecycle.DeferredSubmissionRetainsRawBytesAndOriginalRequest"),
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
         "std::move(tail), std::move(view), false, redraw",
         "std::move(tail), view, false, redraw",
         "LineSubmissionFault.AllocationFailureCannotConsumeAnUnownedLine"),
    ]
    results = []
    for name, entry_name, source_name, before, after, target in cases:
        directory = folder / ("native-control-" + name)
        directory.mkdir()
        binary_target = "codet_publication_fault_tests" if target.startswith("LineSubmissionFault.") else "codet_tests"
        link = link_command(binary_target)
        baseline = _test(build / binary_target, target, directory)
        if baseline["exit"] or baseline["assertions"]:
            raise ValueError("Native control baseline failed: " + target)
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
        binary = directory / "control-tests"
        mutated_link = [p for p in link if p != original_object]
        mutated_link.insert(1, str(obj))
        mutated_link[mutated_link.index("-o") + 1] = str(binary)
        linked = _run(mutated_link, build, 600)
        if linked["exit"]:
            raise ValueError("Native mutant linking failed: " + repr(linked))
        mutant = _test(binary, target, directory)
        markers = {"deferred": "deferred", "separator": 'read()', "printer": "Expected equality of these values", "pid-guard": "unexpected wait",
                   "refresh": "Refresh after failed drawing", "inspection": "handle->read",
                   "submission-allocation": "Allocation consumed a line without pending ownership"}
        if (mutant["exit"] != 1 or not mutant["assertions"] or
                any("C++ exception" in message or "unknown file" in message for message in mutant["assertions"]) or
                not any(markers[name] in message for message in mutant["assertions"])):
            raise ValueError("Native mutant did not fail its intended assertion: " + target + repr(mutant))
        results.append(dict(name=name, target=target, mutation=dict(before=before, after=after, original_sha256=digest(root / source_name), mutant_sha256=digest(source)), binary_sha256=digest(binary), baseline=baseline, compile=compiled, link=linked, mutant=mutant))
    return results
