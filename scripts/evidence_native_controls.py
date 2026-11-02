"""Native assertion controls, kept distinct from Python evidence controls."""

import json
import shlex
import subprocess
import xml.etree.ElementTree as ET

from evidence_benchmarks import replace_once


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
    commands = subprocess.check_output([ninja, "-C", str(build), "-t", "commands", "codet_tests"], text=True, timeout=30)
    link = shlex.split(commands.splitlines()[-1])
    if "&&" in link:
        link = link[link.index("&&") + 1:]
        if "&&" in link:
            link = link[:link.index("&&")]
    cases = [
        ("deferred", "tests/diagnostic_lifecycle_test.cpp", "include/codet/detail/line_prompt.hpp",
         "if (output_.presentation_deferred())\n          return;\n        dirty_ = visible_ = false;",
         "if (false)\n          return;\n        dirty_ = visible_ = false;",
         "NativeRecordLifecycle.DeferredEraseRetainsWorkUntilDiagnosticCompletion"),
        ("separator", "src/modern/platform.cpp", "src/modern/platform.cpp",
         "record.request_separator(io_->line_output->before_output(native, inspection.identity));",
         "(void)io_->line_output->before_output(native, inspection.identity);",
         "NativeRecordLifecycle.InjectedStdoutSharesSeparatorWithNativeStderr"),
        ("printer", "tests/diagnostic_lifecycle_test.cpp", "tests/diagnostic_lifecycle_test.cpp",
         "(void)::dup2(out, STDOUT_FILENO);\n", "(void)out;\n",
         "DiagnosticFixture.FailureDetailsReachParentLogAndDoNotContaminatePty"),
    ]
    results = []
    for name, entry_name, source_name, before, after, target in cases:
        directory = folder / ("native-control-" + name)
        directory.mkdir(exist_ok=True)
        baseline = _test(build / "codet_tests", target, directory)
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
        if name != "deferred":
            argv[argv.index(str(root / entry_name))] = str(source)
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
        if mutant["exit"] != 1 or not mutant["assertions"]:
            raise ValueError("Native mutant did not fail its intended assertion: " + target)
        results.append(dict(name=name, target=target, baseline=baseline, compile=compiled, link=linked, mutant=mutant))
    return results
