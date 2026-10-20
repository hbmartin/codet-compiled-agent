#!/usr/bin/env python3
"""Strict host analysis of the modern application using CMake's compilation database."""

import argparse
import concurrent.futures
import hashlib
import json
import os
import plistlib
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from pathlib import Path

from modern_sources import application_sources

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ("clang-tidy", "cppcheck", "clang-analyzer")
CPPCHECK_LIBRARIES = ("posix", "bsd", "libcurl", "openssl", "sqlite3", "zlib")


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


CANCEL = threading.Event()


def capture(command, cwd, timeout):
    """Each worker owns its child through bounded signal, drain and reap cleanup."""
    started = time.monotonic()
    result = dict(command=command, directory=str(cwd), exit_code=None,
                  stdout="", stderr="", failure=None)
    child = None
    def partial(exc):
        for field, value in (("stdout", exc.output), ("stderr", exc.stderr)):
            if value is not None:
                result[field] = value.decode("utf-8", "replace") if isinstance(value, bytes) else value
    def signal_group(sig):
        if child.returncode is None:
            try:
                os.killpg(child.pid, sig)
            except ProcessLookupError:
                pass
            except PermissionError:
                # Darwin returns EPERM for an owned group containing only its zombie leader.
                if sys.platform != "darwin":
                    raise
    def cleanup():
        deadline = time.monotonic() + 2
        signal_group(signal.SIGTERM)
        try:
            result["stdout"], result["stderr"] = child.communicate(timeout=.25)
            return
        except subprocess.TimeoutExpired as exc:
            partial(exc)
        signal_group(signal.SIGKILL)
        try:
            result["stdout"], result["stderr"] = child.communicate(timeout=max(0, deadline-time.monotonic()))
        except subprocess.TimeoutExpired as exc:
            partial(exc)
            for pipe in (child.stdout, child.stderr):
                if pipe is not None:
                    pipe.close()
            try:
                child.wait(timeout=max(0, deadline-time.monotonic()))
            except subprocess.TimeoutExpired:
                result["cleanup_failure"] = "Owned analyzer did not reap within cleanup budget"
    try:
        if CANCEL.is_set():
            result["failure"] = "interrupted"
            return result
        child = subprocess.Popen(command, cwd=cwd, text=True, encoding="utf-8", errors="replace",
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        deadline = started + timeout
        while True:
            if CANCEL.is_set() or time.monotonic() >= deadline:
                result["failure"] = "interrupted" if CANCEL.is_set() else "timeout"
                cleanup()
                break
            try:
                result["stdout"], result["stderr"] = child.communicate(timeout=min(.1, max(0, deadline-time.monotonic())))
                break
            except subprocess.TimeoutExpired as exc:
                partial(exc)
        result["exit_code"] = child.returncode
    except KeyboardInterrupt:
        CANCEL.set()
        if child is not None:
            cleanup()
        raise
    except OSError as exc:
        result["failure"] = str(exc)
        if child is not None:
            cleanup()
    finally:
        result["seconds"] = round(time.monotonic() - started, 3)
    return result


def parallel_map(function, entries, jobs):
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=jobs)
    try:
        return list(pool.map(function, entries))
    except KeyboardInterrupt:
        CANCEL.set()
        raise
    finally:
        pool.shutdown(wait=True, cancel_futures=True)


def select_entries(database, root=ROOT):
    expected = set(application_sources(root))
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
    return any(path.resolve().is_relative_to(root.resolve() / folder) for folder in ("src", "include"))


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


def include_arguments(entry):
    """Return argv spans and canonical paths without losing include search order."""
    args = entry["arguments"]
    result = []
    i = 1
    while i < len(args):
        arg = args[i]
        flag = next((f for f in ("-isystem", "-iquote", "-idirafter", "-I") if arg.startswith(f)), None)
        if flag:
            if arg == flag:
                if i + 1 >= len(args):
                    raise ValueError("Missing include path")
                value, width = args[i + 1], 2
            else:
                value, width = arg[len(flag):], 1
            result.append((i, width, flag, (Path(entry["directory"]) / value).resolve()))
            i += width
        else:
            i += 1
    return result


def cppcheck_compatibility(entries, folder, timeout, require_adapter=False):
    """A generated, token-equivalent overlay; originals remain untouched."""
    roots = sorted({item[3] for e in entries for item in include_arguments(e)})
    rewritten = [dict(e, arguments=list(e["arguments"])) for e in entries]
    audits = []
    for original in roots:
        relative = Path("toml++/impl/preprocessor.hpp")
        header = original / relative
        if not header.exists():
            continue
        content = header.read_text()
        before = "operator TOML_CONCAT(op, =)"
        if content.count(before) != 1:
            raise ValueError("Unrecognized toml++ token-paste macro; compatibility review required")
        overlay = (folder / "headers" / str(len(audits))).resolve()
        shutil.copytree(original, overlay, dirs_exist_ok=True)
        adapted = content.replace(before, "operator op##=")
        (overlay / relative).write_text(adapted)
        changed_entries, comparisons = [], []
        replacements = 0
        for entry in rewritten:
            spans = [x for x in include_arguments(entry) if x[3] == original]
            if not spans:
                changed_entries.append(entry)
                continue
            args = list(entry["arguments"])
            for start, width, flag, _ in reversed(spans):
                args[start:start+width] = [flag, str(overlay)]
                replacements += 1
            args.append(f"-fmacro-prefix-map={overlay}={original}")
            changed = dict(entry, arguments=args)
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
        if not replacements or not comparisons:
            raise ValueError("Compatibility overlay was not applied or verified")
        rewritten = changed_entries
        audits.append(dict(header=str(header), original_sha256=hashlib.sha256(content.encode()).hexdigest(),
                           adapted_sha256=hashlib.sha256(adapted.encode()).hexdigest(),
                           replacements=replacements, comparisons=comparisons))
    if require_adapter and not audits:
        raise ValueError("Required compatibility audit is empty")
    write_json(folder / "compatibility.json", dict(passed=True, applied=bool(audits), headers=audits))
    return rewritten


def clang_one(tool, entry, binary, folder, database, timeout, root):
    source = Path(entry["file"]).resolve()
    root = root.resolve()
    name = "__".join(source.relative_to(root).parts)
    report = folder / (name + (".plist" if tool == "clang-analyzer" else ".yaml"))
    report.unlink(missing_ok=True)
    if tool == "clang-analyzer":
        command = compiler_arguments(entry) + ["--analyze", "-Xanalyzer",
                    "-analyzer-output=plist", "-o", str(report)]
    else:
        command = [binary, "-p", str(database.parent), "--config-file=" + str(ROOT / ".clang-tidy"),
                   "--warnings-as-errors=*", "--use-color=false", "--export-fixes=" + str(report),
                   "--header-filter=^" + re.escape(str(root)) + "/(src|include|\\.deps)/",
                   "--system-headers", str(source)]
    run = capture(command, entry["directory"], timeout)
    run["source"] = str(source)
    if CANCEL.is_set():
        return report_result(run, [], "interrupted")
    (folder / (name + ".log")).write_text(run["stdout"] + run["stderr"])
    try:
        if tool == "clang-analyzer":
            data = plistlib.loads(report.read_bytes())
            if not isinstance(data.get("diagnostics"), list):
                raise ValueError("Missing diagnostics array")
            diagnostics = data["diagnostics"]
            files = data.get("files", [])
            def locations(value):
                if isinstance(value, dict):
                    if "file" in value and isinstance(value["file"], int):
                        index = value["file"]
                        if index < 0 or index >= len(files):
                            raise ValueError("Invalid diagnostic file index")
                        yield files[index]
                    for item in value.values():
                        yield from locations(item)
                elif isinstance(value, list):
                    for item in value:
                        yield from locations(item)
            findings = [d for d in diagnostics if not files or not list(locations(d)) or
                        any(first_party(f, root) for f in locations(d))]
        else:
            # Clang-Tidy does not create an export file when there are no diagnostics.
            import yaml
            data = yaml.load(report.read_text(), Loader=getattr(yaml, "CSafeLoader", yaml.SafeLoader)) if report.exists() else {"Diagnostics": []}
            if not isinstance(data, dict) or not isinstance(data.get("Diagnostics"), list):
                raise ValueError("Missing Diagnostics array")
            diagnostics = data["Diagnostics"]
            findings = [d for d in diagnostics if first_party(d.get("DiagnosticMessage", {}).get("FilePath"), root)
                        or any(first_party(n.get("FilePath"), root) for n in d.get("Notes", []))]
        run["all_diagnostics"] = diagnostics
        failure = None
        if run["exit_code"] not in ((0, 1) if tool == "clang-tidy" and diagnostics else (0,)):
            failure = "Tool exited unsuccessfully"
        if any(d.get("DiagnosticName") == "clang-diagnostic-error" for d in diagnostics):
            failure = "Compilation failed during analysis"
        return report_result(run, findings, failure)
    except Exception as exc:
        return report_result(run, [], "Invalid or missing report: " + str(exc))


FATAL_IDS = {"syntaxError", "internalError", "internalAstError", "instantiationError",
             "cppcheckError", "unknownMacro", "preprocessorErrorDirective", "cppcheckLimit",
             "missingInclude", "checkLevelNormal", "normalCheckLevelMaxBranches", "toomanyconfigs"}


def cppcheck(entries, binary, folder, database, timeout, root, jobs):
    checkers = folder / "checkers.txt"
    checkers.unlink(missing_ok=True)
    command = [binary, "--project=" + str(database), "--std=c11", "--std=c++17",
               "--library=" + ",".join(CPPCHECK_LIBRARIES),
               "--enable=warning,performance,portability,information,missingInclude",
               "--check-level=exhaustive", "--safety", "--inline-suppr", "--error-exitcode=1",
               "--xml", "--xml-version=2", "--checkers-report=" + str(checkers),
               "--cppcheck-build-dir=" + str(folder / "cache"), "-j", "1"]
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
        findings = [d for d in diagnostics if d.get("severity", "warning") != "information" and
                    d.get("id") != "checkersReport" and (not d["locations"] or
                    any(first_party(loc.get("file"), root) for loc in d["locations"]))]
        run["all_diagnostics"] = diagnostics
        failure = None
        if run["exit_code"] not in (0, 1) or (run["exit_code"] == 1 and not diagnostics):
            failure = "Tool exited unsuccessfully"
        critical = any(d.get("id") in FATAL_IDS or d.get("id", "").startswith("internal") or
                       re.search(r"bailing out|analysis (?:is )?incomplete|limiting (?:valueflow )?analysis",
                                 d.get("msg", ""), re.I) or
                       (d.get("id") == "checkersReport" and
                        re.search(r"there (?:was|were) critical errors", d.get("msg", ""), re.I))
                       for d in diagnostics)
        if critical:
            failure = "Parser or analysis failure"
        proof = checkers.read_text() if checkers.exists() else ""
        active = re.findall(r"^Yes\s+(\S+)", proof, re.M)
        if ("No critical errors encountered." not in proof or
            not any(x.startswith("CheckBufferOverrun::") for x in active) or
            not any(x.startswith("CheckUninitVar::") for x in active)):
            failure = failure or "Missing semantic analysis completion evidence"
        cache_files = list((folder / "cache").glob("*.a1"))
        if not cache_files or not any(ET.fromstring(f.read_bytes()).find("hash") is not None or
                                     ET.fromstring(f.read_bytes()).find("FileInfo") is not None for f in cache_files):
            failure = failure or "Missing per-translation-unit analysis artifact"
        run["completion_proof"] = dict(active_checkers=active, critical_error=bool(critical),
             report_sha256=hashlib.sha256(proof.encode()).hexdigest(), artifacts=[str(p) for p in cache_files])
        run["completed_sources"] = [e["file"] for e in entries] if not (run["failure"] or failure) else []
        return report_result(run, findings, failure)
    except (ET.ParseError, ValueError, KeyError, TypeError) as exc:
        return report_result(run, [], "Invalid XML report: " + str(exc))


def select_binary(tool):
    variable = "CLANG_TIDY" if tool == "clang-tidy" else "CPPCHECK"
    if variable in os.environ:
        return shutil.which(os.environ[variable]) or os.environ[variable]
    if tool == "cppcheck":
        pinned = ROOT / "build/analysis-tools/cppcheck/bin/cppcheck"
        return str(pinned) if pinned.is_file() else shutil.which("cppcheck") or "cppcheck"
    return shutil.which("clang-tidy-18") or shutil.which("clang-tidy") or "clang-tidy-18"


def actual_header_canaries(entries, folder, root):
    if not (root / "include/codet/core.hpp").is_file():
        return []
    result = []
    for suffix, header in ((".c", '"text_compat.h"'), (".cpp", '"codet/core.hpp"\n#include <toml++/toml.hpp>')):
        source_entry = next(e for e in entries if Path(e["file"]).suffix == suffix)
        source = (folder / ("actual-headers" + suffix)).resolve()
        source.write_text("#include " + header + "\nint codet_analysis_canary(void) { int a[1] = {0}; return a[2]; }\n")
        arguments = []
        for arg in source_entry["arguments"]:
            candidate = Path(source_entry["directory"]) / arg
            arguments.append(str(source) if not arg.startswith("-") and
                             candidate.resolve() == Path(source_entry["file"]).resolve() else arg)
        result.append(dict(source_entry, file=str(source), arguments=arguments,
                           expectation="defective", expected_diagnostic="arrayIndexOutOfBounds"))
    # Exercise each loaded model with both defective and safe calls. These are
    # parsed through actual application/dependency headers, never executed.
    contracts = {
        "posix": ('<fcntl.h>\n#include <unistd.h>',
                  'open("/unexecuted", O_RDONLY);',
                  'int fd = open("/unexecuted", O_RDONLY); if (fd >= 0) close(fd);'),
        "bsd": ('<string.h>', 'char out[4] = {}; strlcat(out, nullptr, sizeof(out));',
                'char out[4] = {}; strlcat(out, "x", sizeof(out));'),
        "libcurl": ('<curl/curl.h>', 'curl_easy_init();',
                    'CURL *h = curl_easy_init(); if (h) curl_easy_cleanup(h);'),
        "openssl": ('<openssl/evp.h>', 'EVP_CIPHER_CTX_new();',
                    'EVP_CIPHER_CTX *h = EVP_CIPHER_CTX_new(); if (h) EVP_CIPHER_CTX_free(h);'),
        "sqlite3": ('<sqlite3.h>', 'sqlite3_malloc(4);',
                    'void *p = sqlite3_malloc(4); sqlite3_free(p);'),
        "zlib": ('<zlib.h>', 'gzopen("/unexecuted", "rb");',
                 'gzFile h = gzopen("/unexecuted", "rb"); if (h) gzclose(h);'),
    }
    template = result[-1]
    # Keep SDK/library search paths, but do not preprocess application-only
    # headers or the toml++ compatibility overlay for library model contracts.
    omit = set()
    for index, width, _, path in include_arguments(template):
        if (path.is_relative_to(root / "include") or path.is_relative_to(root / ".deps") or
                path.name == "generated"):
            omit.update(range(index, index + width))
    library_arguments = [arg for index, arg in enumerate(template["arguments"]) if index not in omit]
    for model in CPPCHECK_LIBRARIES:
        header, defective, safe = contracts[model]
        for kind, body in (("defective", defective), ("safe", safe)):
            source = (folder / ("library-" + model + "-" + kind + ".cpp")).resolve()
            source.write_text('#include ' + header +
                              '\nvoid codet_library_canary() { ' + body + ' }\n')
            result.append(dict(template, file=str(source),
                arguments=[str(source) if a == template["file"] else a for a in library_arguments],
                expected_diagnostic=("nullPointer" if model == "bsd" else "leakReturnValNotUsed")
                    if kind == "defective" else None,
                library_model=model, expectation="clean" if kind == "safe" else "defective"))
    fatal = (folder / "suppressed-fatal.cpp").resolve()
    fatal.write_text("#include \"codet/core.hpp\"\n// cppcheck-suppress syntaxError\nint invalid( {\n")
    result.append(dict(template, file=str(fatal), arguments=[str(fatal) if a == template["file"] else a
                                                          for a in template["arguments"]],
                       expectation="fatal", expected_diagnostic=None))
    return result


def canary_passed(probe, run):
    kind = probe.get("expectation")
    if kind not in {"defective", "clean", "fatal"} or any(
            key in probe for key in ("expected_clean", "expected_failure")):
        raise ValueError("Invalid or contradictory canary expectation")
    if bool(probe.get("expected_diagnostic")) != (kind == "defective"):
        raise ValueError("Invalid canary diagnostic expectation")
    if kind == "fatal":
        return bool(run["failure"]) and run["exit_code"] not in (None, 0) and bool(
            run.get("completion_proof", {}).get("critical_error"))
    if run["failure"]:
        return False
    source = Path(probe["file"]).resolve()
    directory = Path(probe["directory"])
    attributed = [d for d in run.get("all_diagnostics", []) if any(
        loc.get("file") and (directory / loc["file"]).resolve() == source
        for loc in d.get("locations", []))]
    if kind == "clean":
        return not any(d.get("severity") != "information" for d in attributed)
    return any(d.get("id") == probe["expected_diagnostic"] for d in attributed)


def run_tool(tool, entries, folder, jobs, timeout, root=ROOT):
    root = root.resolve()
    folder = folder.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    database = folder / "compile_commands.json"
    write_json(database, entries)
    binaries = sorted({e["arguments"][0] for e in entries}) if tool == "clang-analyzer" else [select_binary(tool)]
    versions = [capture([binary, "--version"], root, min(timeout, 10)) for binary in binaries]
    failed = any(v["failure"] or v["exit_code"] != 0 for v in versions)
    if tool == "clang-analyzer" and any("clang" not in v["stdout"].lower() for v in versions):
        failed = True
    if tool == "cppcheck" and any(not re.search(r"Cppcheck 2\.22\.0(?:\s|$)", v["stdout"]) for v in versions):
        failed = True
    canaries = []
    if failed:
        results = [report_result(dict(failure="Missing/incompatible requested tool; use Clang and pinned Cppcheck 2.22.0",
                                     exit_code=None), [])]
    elif tool == "cppcheck":
        probes = actual_header_canaries(entries, folder, root)
        normalized = cppcheck_compatibility(entries + probes, folder, timeout,
            require_adapter=(root / ".deps/tomlplusplus/include/toml++/impl/preprocessor.hpp").is_file())
        def one(pair):
            index, entry = pair
            dest = folder / ("unit-" + str(index))
            dest.mkdir(exist_ok=True)
            db = dest / "compile_commands.json"
            write_json(db, cppcheck_entries([entry]))
            return cppcheck([entry], binaries[0], dest, db, timeout, root, 1)
        all_results = parallel_map(one, list(enumerate(normalized)), jobs)
        results = all_results[:len(entries)]
        for probe, run in zip(probes, all_results[len(entries):], strict=True):
            passed = canary_passed(probe, run)
            canaries.append(dict(source=probe["file"], expectation=probe["expectation"],
                                 expected_diagnostic=probe.get("expected_diagnostic"),
                                 library_model=probe.get("library_model"), passed=passed, result=run))
    else:
        results = parallel_map(lambda entry: clang_one(tool, entry, binaries[0], folder,
                                                       database, timeout, root), entries, jobs)
    completed = ([source for run in results for source in run.get("completed_sources", [])]
                 if tool == "cppcheck" else [r["source"] for r in results if not r["failure"]])
    summary = dict(tool=tool, versions=versions, expected_sources=[e["file"] for e in entries],
                   completed_sources=completed, results=results, canaries=canaries,
                   passed=bool(results) and len(completed) == len(entries) and
                          all(r["status"] == "passed" for r in results) and all(c["passed"] for c in canaries),
                   findings=sum(len(r["findings"]) for r in results))
    write_json(folder / "manifest.json", summary)
    return summary


def run_all(entries, folder, selected, jobs, timeout, root=ROOT):
    sources = [dict(source=e["file"], sha256=hashlib.sha256(Path(e["file"]).read_bytes()).hexdigest())
               for e in entries]
    inputs = [dict(source=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest())
              for p in sorted((root / "include").rglob("*")) if p.is_file()]
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
    changed_inputs = [s["source"] for s in inputs if not Path(s["source"]).exists() or
                      hashlib.sha256(Path(s["source"]).read_bytes()).hexdigest() != s["sha256"]]
    manifest = dict(sources=sources, inputs=inputs, tools=summaries, changed_sources=changed,
                    changed_inputs=changed_inputs,
                    passed=bool(summaries) and not changed and not changed_inputs and all(s["passed"] for s in summaries))
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
    CANCEL.clear()
    parser = argparse.ArgumentParser(description=__doc__)
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
    try:
        signal.signal(signal.SIGINT, signal.default_int_handler)
        raise SystemExit(main())
    except KeyboardInterrupt:
        CANCEL.set()
        raise SystemExit(130) from None
