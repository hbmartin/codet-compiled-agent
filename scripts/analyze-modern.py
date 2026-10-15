#!/usr/bin/env python3
"""Strict host analysis of the modern application using CMake's compilation database."""

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shlex
import shutil
import signal
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ("clang-tidy", "cppcheck", "clang-analyzer")


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def capture(command, cwd, timeout):
    """Bound the entire process group, including compiler children."""
    started = time.monotonic()
    result = dict(command=command, directory=str(cwd), exit_code=None,
                  stdout="", stderr="", failure=None)
    try:
        child = subprocess.Popen(command, cwd=cwd, text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, start_new_session=True)
        try:
            result["stdout"], result["stderr"] = child.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            result["stdout"], result["stderr"] = child.communicate()
            result["failure"] = "timeout"
        result["exit_code"] = child.returncode
    except OSError as exc:
        result["failure"] = str(exc)
    result["seconds"] = round(time.monotonic() - started, 3)
    return result


def select_entries(database, root=ROOT):
    expected = {p for p in (root / "src/modern").rglob("*") if p.suffix in (".c", ".cc", ".cpp", ".cxx")}
    expected.add(root / "src/text_compat.c")
    expected = {p.resolve() for p in expected}
    data = json.loads(database.read_text())
    if not isinstance(data, list):
        raise ValueError("Compilation database must be an array")
    selected = {}
    for entry in data:
        directory = (database.parent / entry["directory"]).resolve()
        source = (directory / entry["file"]).resolve()
        if source not in expected:
            continue
        command = entry.get("arguments") or shlex.split(entry["command"])
        normalized = dict(directory=str(directory), file=str(source), arguments=command)
        if source in selected and selected[source] != normalized:
            raise ValueError(f"Conflicting compilation commands for {source}")
        selected[source] = normalized
    missing = expected - selected.keys()
    if not selected or missing:
        raise ValueError("Missing compilation commands: " + ", ".join(map(str, sorted(missing))))
    for source in selected:
        if not source.is_file():
            raise ValueError(f"Missing source: {source}")
    return [selected[source] for source in sorted(selected)]


def compiler_arguments(entry):
    """Keep semantic flags; remove compilation and dependency output flags."""
    command = entry["arguments"]
    result = []
    i = 0
    paired = ("-o", "-MF", "-MT", "-MQ", "-MJ", "--serialize-diagnostics")
    while i < len(command):
        arg = command[i]
        if arg in paired:
            i += 2
            continue
        if arg in ("-c", "-MD", "-MMD", "-MP", "-MG"):
            i += 1
            continue
        if any(arg.startswith(flag) and arg != flag for flag in paired[1:5]):
            i += 1
            continue
        result.append(arg)
        i += 1
    return result


def first_party(filename, root):
    if not filename:
        return True
    path = Path(filename)
    if not path.is_absolute():
        path = root / path
    return any(path.resolve().is_relative_to(root / folder) for folder in ("src", "include"))


def report_result(run, findings, failure=None):
    run["findings"] = findings
    run["failure"] = run["failure"] or failure
    run["status"] = "failed" if run["failure"] else "findings" if findings else "passed"
    return run


def cppcheck_entries(entries):
    # Cppcheck 2.13's argument-array importer drops embedded quotes in -D values.
    # Its -isystem directories are also omitted from normal include lookup. Use
    # the equivalent command form and promote these paths for Cppcheck only.
    # This serialized command is imported by Cppcheck, never executed by a shell.
    imported = []
    for entry in entries:
        arguments = ["-I" if arg == "-isystem" else "-I" + arg[len("-isystem"):]
                     if arg.startswith("-isystem") else arg for arg in entry["arguments"]]
        imported.append(dict(directory=entry["directory"], file=entry["file"],
                             command=subprocess.list2cmdline(arguments)))
    return imported


def cppcheck_compatibility(entries, folder, timeout):
    """Work around SimpleCpp's nested token-paste bug without changing C++ tokens."""
    include_roots = set()
    for entry in entries:
        arguments = entry["arguments"]
        for i, arg in enumerate(arguments[:-1]):
            if arg in ("-I", "-isystem"):
                include_roots.add((Path(entry["directory"]) / arguments[i + 1]).resolve())
            elif arg.startswith("-I"):
                include_roots.add((Path(entry["directory"]) / arg[2:]).resolve())
    rewritten = entries
    audits = []
    for original in sorted(include_roots):
        relative = Path("toml++/impl/preprocessor.hpp")
        header = original / relative
        if not header.exists():
            continue
        content = header.read_text()
        before = "operator TOML_CONCAT(op, =)"
        if content.count(before) != 1:
            raise ValueError("Unrecognized toml++ token-paste macro; compatibility review required")
        overlay = folder / "headers" / str(len(audits))
        shutil.copytree(original, overlay, dirs_exist_ok=True)
        adapted = content.replace(before, "operator op##=")
        (overlay / relative).write_text(adapted)
        changed_entries = []
        comparisons = []
        for entry in rewritten:
            if str(original) not in entry["arguments"]:
                changed_entries.append(entry)
                continue
            arguments = [str(overlay) if a == str(original) else a for a in entry["arguments"]]
            arguments.append(f"-fmacro-prefix-map={overlay}={original}")
            changed = dict(entry, arguments=arguments)
            # -P removes line markers; prefix mapping preserves __FILE__ values.
            # Exact equality includes string literals and whitespace, with no
            # normalization that could hide a token or behavioral difference.
            runs = [capture(compiler_arguments(e) + ["-E", "-P"], e["directory"], timeout)
                    for e in (entry, changed)]
            same = all(r["exit_code"] == 0 and not r["failure"] for r in runs) and runs[0]["stdout"] == runs[1]["stdout"]
            comparisons.append(dict(source=entry["file"], equivalent=same,
                commands=[r["command"] for r in runs],
                exit_codes=[r["exit_code"] for r in runs], failures=[r["failure"] for r in runs],
                stderr=[r["stderr"] for r in runs],
                sha256=[hashlib.sha256(r["stdout"].encode()).hexdigest() for r in runs]))
            if not same:
                write_json(folder / "compatibility.json", dict(comparisons=comparisons, passed=False))
                raise ValueError("toml++ compatibility preprocessing differs or failed for " + entry["file"])
            changed_entries.append(changed)
        rewritten = changed_entries
        audits.append(dict(header=str(header), original_sha256=hashlib.sha256(content.encode()).hexdigest(),
                           adapted_sha256=hashlib.sha256(adapted.encode()).hexdigest(), comparisons=comparisons))
    write_json(folder / "compatibility.json", dict(passed=True, headers=audits))
    return rewritten


def clang_one(tool, entry, binary, folder, database, timeout, root):
    source = Path(entry["file"])
    name = "__".join(source.relative_to(root).parts)
    report = folder / (name + (".plist" if tool == "clang-analyzer" else ".yaml"))
    report.unlink(missing_ok=True)
    if tool == "clang-analyzer":
        command = compiler_arguments(entry) + ["--analyze", "-Xanalyzer",
                    "-analyzer-output=plist", "-o", str(report)]
    else:
        command = [binary, "-p", str(database.parent), "--config-file=" + str(ROOT / ".clang-tidy"),
                   "--warnings-as-errors=*", "--use-color=false", "--export-fixes=" + str(report),
                   "--header-filter=^" + re.escape(str(root)) + "/(src|include)/", str(source)]
    run = capture(command, entry["directory"], timeout)
    run["source"] = str(source)
    (folder / (name + ".log")).write_text(run["stdout"] + run["stderr"])
    try:
        if tool == "clang-analyzer":
            data = plistlib.loads(report.read_bytes())
            if not isinstance(data.get("diagnostics"), list):
                raise ValueError("Missing diagnostics array")
            findings = data["diagnostics"]
        else:
            # Clang-Tidy does not create an export file when there are no diagnostics.
            import yaml
            data = yaml.safe_load(report.read_text()) if report.exists() else {"Diagnostics": []}
            if not isinstance(data, dict) or not isinstance(data.get("Diagnostics"), list):
                raise ValueError("Missing Diagnostics array")
            findings = data["Diagnostics"]
        failure = None
        if run["exit_code"] not in ((0, 1) if tool == "clang-tidy" and findings else (0,)):
            failure = "Tool exited unsuccessfully"
        if any(d.get("DiagnosticName") == "clang-diagnostic-error" for d in findings):
            failure = "Compilation failed during analysis"
        return report_result(run, findings, failure)
    except Exception as exc:
        return report_result(run, [], "Invalid or missing report: " + str(exc))


def cppcheck(entries, binary, folder, database, timeout, root, jobs):
    command = [binary, "--project=" + str(database), "--std=c11", "--std=c++17",
               "--enable=warning,performance,portability", "--check-level=normal",
               "--inline-suppr", "--error-exitcode=1", "--xml", "--xml-version=2",
               "--cppcheck-build-dir=" + str(folder / "cache"), "-j", str(jobs)]
    # Always validate a fresh scan, including after configuration/header changes.
    shutil.rmtree(folder / "cache", ignore_errors=True)
    (folder / "cache").mkdir()
    run = capture(command, root, timeout)
    (folder / "stdout.log").write_text(run["stdout"])
    (folder / "report.xml").write_text(run["stderr"])
    try:
        data = ET.fromstring(run["stderr"])
        errors = data.find("errors")
        if data.tag != "results" or errors is None:
            raise ValueError("Missing results/errors")
        diagnostics = [dict(e.attrib, locations=[dict(loc.attrib) for loc in e.findall("location")])
                       for e in errors]
        findings = [d for d in diagnostics if not d["locations"] or
                    any(first_party(loc.get("file"), root) for loc in d["locations"])]
        completed = [e["file"] for e in entries if any(
            line.startswith("Checking " + e["file"]) for line in run["stdout"].splitlines())]
        run["completed_sources"] = completed
        run["all_diagnostics"] = diagnostics
        failure = None
        if len(completed) != len(entries):
            failure = "Incomplete source coverage"
        if run["exit_code"] not in (0, 1) or (run["exit_code"] == 1 and not diagnostics):
            failure = "Tool exited unsuccessfully"
        if any(d.get("id") in ("syntaxError", "internalError", "cppcheckError", "unknownMacro",
                                "preprocessorErrorDirective") for d in diagnostics):
            failure = "Parser or analysis failure"
        return report_result(run, findings, failure)
    except (ET.ParseError, ValueError, KeyError, TypeError) as exc:
        return report_result(run, [], "Invalid XML report: " + str(exc))


def run_tool(tool, entries, folder, jobs, timeout, root=ROOT):
    folder.mkdir(parents=True, exist_ok=True)
    database = folder / "compile_commands.json"
    write_json(database, entries)
    if tool == "clang-analyzer":
        binaries = sorted({entry["arguments"][0] for entry in entries})
    else:
        candidates = ([os.environ.get("CLANG_TIDY", "clang-tidy-18"), "clang-tidy"]
                      if tool == "clang-tidy" else [os.environ.get("CPPCHECK", "cppcheck")])
        binaries = [next((shutil.which(b) for b in candidates if shutil.which(b)), candidates[0])]
    versions = [capture([binary, "--version"], root, min(timeout, 10)) for binary in binaries]
    failed = any(v["failure"] or v["exit_code"] != 0 for v in versions)
    if tool == "clang-analyzer" and any("clang" not in v["stdout"].lower() for v in versions):
        failed = True
    if failed:
        results = [report_result(dict(failure="Missing/incompatible tool; configure with Clang",
                                     exit_code=None), [])]
    elif tool == "cppcheck":
        write_json(database, cppcheck_entries(cppcheck_compatibility(entries, folder, timeout)))
        results = [cppcheck(entries, binaries[0], folder, database, timeout, root, jobs)]
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            results = list(pool.map(lambda entry: clang_one(tool, entry, binaries[0], folder,
                                                           database, timeout, root), entries))
    completed = ([source for run in results for source in run.get("completed_sources", [])]
                 if tool == "cppcheck" else [r["source"] for r in results if not r["failure"]])
    summary = dict(tool=tool, versions=versions, expected_sources=[e["file"] for e in entries],
                   completed_sources=completed, results=results,
                   passed=bool(results) and all(r["status"] == "passed" for r in results),
                   findings=sum(len(r["findings"]) for r in results))
    write_json(folder / "manifest.json", summary)
    return summary


def run_all(entries, folder, selected, jobs, timeout, root=ROOT):
    sources = [dict(source=e["file"], sha256=hashlib.sha256(Path(e["file"]).read_bytes()).hexdigest())
               for e in entries]
    summaries = []
    for tool in selected:
        try:
            summary = run_tool(tool, entries, folder / tool, jobs, timeout, root)
        except Exception as exc:
            # Preserve results from the other tools, including unexpected tool/report failures.
            summary = dict(tool=tool, passed=False, findings=0, failure=str(exc))
            write_json(folder / tool / "manifest.json", summary)
        summaries.append(summary)
        print(f"{tool}: {'PASS' if summary['passed'] else 'FAIL'}; {summary['findings']} findings", flush=True)
    changed = [s["source"] for s in sources if hashlib.sha256(Path(s["source"]).read_bytes()).hexdigest() != s["sha256"]]
    manifest = dict(sources=sources, tools=summaries, changed_sources=changed,
                    passed=bool(summaries) and not changed and all(s["passed"] for s in summaries))
    write_json(folder / "manifest.json", manifest)
    return manifest


def selftest(entries, folder, jobs, timeout):
    """Analyze deliberately defective fixtures; never link or execute them."""
    folder.mkdir(parents=True, exist_ok=True)
    compilers = {Path(e["file"]).suffix: e["arguments"][0] for e in entries}
    with tempfile.TemporaryDirectory(prefix="fixtures-", dir=folder) as temp:
        root = Path(temp)
        source_dir = root / "src/modern"
        source_dir.mkdir(parents=True)
        include_dir = root / "third party"
        include_dir.mkdir()
        (include_dir / "fixture_version.h").write_text("#define FIXTURE_VERSION PROBE_VERSION\n")
        toml_header = include_dir / "toml++/impl/preprocessor.hpp"
        toml_header.parent.mkdir(parents=True)
        toml_header.write_text("#define TOML_CONCAT_1(x, y) x##y\n"
            "#define TOML_CONCAT(x, y) TOML_CONCAT_1(x, y)\n"
            "struct Flags { unsigned bits; };\n"
            "#define MAKE_FLAGS(op) inline Flags& operator TOML_CONCAT(op, =)(Flags& a, Flags b) { a.bits op##= b.bits; return a; }\n"
            "MAKE_FLAGS(&)\n")
        c = source_dir / "bounds.c"
        c.write_text("#include <fixture_version.h>\nconst char *version = FIXTURE_VERSION;\n"
                     "int bounds(void) { int a[1] = {0}; return a[2]; }\n")
        cpp = source_dir / "raii.cpp"
        cpp.write_text("#include <mutex>\n#include <toml++/impl/preprocessor.hpp>\nvoid observe();\n"
                       "void raii(std::mutex &m) { std::lock_guard<std::mutex>{m}; observe(); }\n")
        fixtures = [dict(file=str(p), directory=str(root), arguments=[compilers[p.suffix],
                    '-DPROBE_VERSION="1.2.3"', "-isystem", str(include_dir),
                    "-std=" + std, "-c", str(p), "-o", str(root / "unused.o")])
                    for p, std in ((c, "c11"), (cpp, "c++17"))]
        result = run_all(fixtures, folder, TOOLS, jobs, timeout, root)
        expected = {"clang-tidy": "bugprone-unused-raii", "clang-analyzer": "core.UndefinedBinaryOperatorResult",
                    "cppcheck": "arrayIndexOutOfBounds"}
        checks = {}
        for summary in result["tools"]:
            runs = summary.get("results", [])
            diagnostics = [d for run in runs for d in run["findings"]]
            # Clang may classify the bounds fixture as an undefined return rather than a binary operand.
            matched = any(expected[summary["tool"]] in json.dumps(d) or
                          (summary["tool"] == "clang-analyzer" and "core.uninitialized.UndefReturn" in json.dumps(d))
                          for d in diagnostics)
            checks[summary["tool"]] = bool(runs) and matched and not any(r["failure"] for r in runs)
        write_json(folder / "selftest.json", checks)
        print("Analyzer self-checks: " + str(checks), flush=True)
        return all(checks.values())


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--tool", choices=("all", *TOOLS), default="all")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/modern-checks")
    parser.add_argument("--jobs", type=positive, default=4)
    parser.add_argument("--timeout", type=positive, default=300)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    folder = build / "analysis"
    folder.mkdir(parents=True, exist_ok=True)
    try:
        entries = select_entries(build / "compile_commands.json")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        write_json(folder / "manifest.json", dict(passed=False, failure=str(exc)))
        parser.exit(1, f"Analysis setup failed: {exc}\nConfigure the analysis build with Clang first.\n")
    if args.selftest:
        return int(not selftest(entries, folder / "selftest", args.jobs, args.timeout))
    selected = TOOLS if args.tool == "all" else (args.tool,)
    return int(not run_all(entries, folder, selected, args.jobs, args.timeout)["passed"])


if __name__ == "__main__":
    raise SystemExit(main())
