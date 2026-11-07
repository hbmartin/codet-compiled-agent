"""Resolve portable replay settings and produce verifiable follow-up evidence."""

import argparse
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

from clang_plist import resolve_files
from evidence_analysis import compare_analysis, verify_analysis
from evidence_benchmarks import (
    executable_provenance,
    verify_git_source,
    verify_git_validation,
    verify_samples,
)
from modern_sources import application_sources
from process_supervision import run as supervised_run
from validation_evidence import atomic_json, digest, require_quiet_processes, write_artifact

CASES = {
    "line": ["allocation-positive", "geometry-positive", "busy", "native-busy", "editing", "editing-basic",
             "native-editing", "deletion", "unchanged", "deferred-tail", "native-boundary", "native-flush-boundary", "native-unchanged", "pending-submission", "native-pending-submission", "native-pending-fragment", "preparation-queue", "bounded-expansion"],
    "tooling": ["hashing", "git-inspection", "supervision", "build-context", "dependency-resolution", "configuration-selection", "measurement-cleanup"],
    "output": ["write-positive", "stat-positive", "flags-positive", "bounds-positive", "diagnostics-amplified",
               "diagnostics-standalone-amplified", "diagnostics-pipe-amplified"],
    "export": ["small", "large"],
}
HARNESS_FILES = ["tests/line_input_bench.cpp", "tests/output_bench.cpp", "tests/export_bench.cpp",
                 "tests/descriptor_bench_metrics.hpp", "tests/native_stat_interposer.hpp", "tests/native_symbol.hpp", "tests/temp.hpp", "scripts/evidence_tool_bench.py", "scripts/evidence_tiger_context_bench.py"]
TOOLS = ("clang-tidy", "cppcheck", "clang-analyzer")


def command(argv, *, cwd=None, timeout=30, env=None):
    return supervised_run([str(s) for s in argv], cwd=cwd, text=True, stdout=subprocess.PIPE,
                          check=True, timeout=timeout, env=env).stdout.strip()


def input_name(root, path):
    path = Path(path)
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path.absolute())


def history_files(folders):
    files = []
    for prior in folders:
        prior = Path(prior)
        required = [prior / name for name in ("config.json", "manifest.json", "capture-driver.log", "capture/run.json")]
        for path in required:
            if not path.is_file():
                raise ValueError("Prior attempt metadata is missing: " + str(path))
        manifest_path = prior / "manifest.json"
        try:
            manifest = json.loads(manifest_path.read_text())
            if not isinstance(manifest, dict) or not isinstance(manifest.get("root"), str) or not manifest["root"]:
                raise ValueError("root must be a nonempty string")
            original_root = Path(manifest["root"])
        except (OSError, ValueError, TypeError) as error:
            raise ValueError("Prior attempt metadata is invalid: " + str(manifest_path) + ": " + str(error)) from error
        files.extend(required)
        files.extend(sorted((prior / "capture").glob("*")))
        for captured in sorted((prior / "capture").glob("*.json")):
            if captured.name == "run.json":
                continue
            try:
                record = json.loads(captured.read_text())
                if not isinstance(record, dict) or not isinstance(record.get("artifacts", []), list):
                    raise ValueError("artifacts must be a list")
                for artifact in record.get("artifacts", []):
                    path = original_root / artifact["path"]
                    if not path.is_file() or digest(path) != artifact["sha256"]:
                        raise ValueError("Prior attempt artifact changed: " + str(path))
                    files.append(path)
            except (OSError, ValueError, KeyError, TypeError) as error:
                raise ValueError("Prior attempt metadata is invalid: " + str(captured) + ": " + str(error)) from error
    if any(not path.is_file() for path in files):
        raise ValueError("Prior attempt metadata is missing")
    return list(dict.fromkeys(files))


def discover_python39(override=None, preferred="python3.9"):
    if override:
        return str(override)
    candidates = [preferred, "python3.9", "/usr/bin/python3", "python3"]
    for candidate in dict.fromkeys(candidates):
        path = shutil.which(str(candidate))
        if not path:
            continue
        try:
            version = command([path, "-c", "import sys;print('%d.%d'%sys.version_info[:2])"], timeout=5,
                              env={"PATH": os.defpath})
        except (OSError, subprocess.SubprocessError):
            continue
        if version == "3.9":
            return path
    return "python3.9-unavailable"


def prepare(root, folder, baseline=None, check_python=None, python39=None, analysis_python=None,
            ci_run=None, clang_tidy=None, cppcheck=None, template_path=None, prior_attempt=None, ca_bundle=None, hash_correctness_ref=None):
    from evidence_inputs import executable
    root = root.resolve()
    if ca_bundle is not None:
        ca_bundle = Path(ca_bundle).resolve()
        if not ca_bundle.is_file():
            raise ValueError("CA bundle does not exist: " + str(ca_bundle))
    folder = (root / folder).resolve()
    if folder.exists():
        raise ValueError("Campaign directory already exists; choose a fresh folder")
    template_path = template_path or Path(__file__).resolve().parents[1] / "checks/validation-replay.json"
    template = json.loads(template_path.read_text())
    if template.get("schema_version") != 5:
        raise ValueError("Unsupported replay template version")
    if set(template) != {"schema_version", "baseline", "builds", "python39", "benchmark"}:
        raise ValueError("Unknown or missing replay template setting")
    if (set(template["builds"]) != {"debug", "asan", "tsan", "cross"} or
            any(not isinstance(v, str) or not v for v in template["builds"].values()) or
            set(template["benchmark"]) != {"sets", "measured_per_set"} or
            any(type(v) is not int or v < 1 for v in template["benchmark"].values())):
        raise ValueError("Invalid replay build or benchmark declaration")
    tools = {}
    def tool(name, candidate, version="--version", optional=False):
        path = shutil.which(str(candidate))
        if path is None:
            if optional:
                return str(candidate)
            raise ValueError("Required tool unavailable: " + name + ": " + str(candidate))
        identity = executable([path], {"PATH": os.defpath})
        probe_env = dict(PATH=str(Path(identity["path"]).parent) + os.pathsep + os.defpath,
                         SEMGREP_ENABLE_VERSION_CHECK="0", SEMGREP_SEND_METRICS="off", SEMGREP_LOG_FILE=str(root / "build/evidence-semgrep-probe.log"))
        banner = command([identity["path"], version], timeout=5, env=probe_env) if version else "No supported version probe; executable digest recorded"
        tools[name] = dict(identity, version=banner)
        return identity["path"]
    python = tool("check-python", check_python or sys.executable)
    analysis_python = tool("analysis-python", analysis_python or python)
    python39 = tool("python39", discover_python39(python39, template["python39"]), optional=True)
    native = {n: tool(n, n, version=None if n == "ar" and sys.platform == "darwin" else "--version")
              for n in ("git", "cmake", "ctest", "ninja", "make", "tar", "ar", "cc", "c++")}
    for name in ("pkg-config", "ranlib", "ld", "strip", "ps"):
        if shutil.which(name):
            tool(name, shutil.which("ps", path=os.defpath) if name == "ps" else name, version=None if sys.platform == "darwin" and name != "pkg-config" else "--version")
    analyzer_env = dict(CLANG_TIDY=tool("clang-tidy", clang_tidy or shutil.which("clang-tidy-18") or shutil.which("clang-tidy") or "clang-tidy-18"),
                        CPPCHECK=tool("cppcheck", cppcheck or root / "build/analysis-tools/cppcheck/bin/cppcheck"))
    # Record transitive tool invocations, not just the top-level Python/Make binary.
    if (root / "build/lint-venv/bin").exists():
        for name, option in (("clang-format", "--version"), ("ruff", "--version"), ("zizmor", "--version")):
            tool(name, root / "build/lint-venv/bin" / name, option)
        for name, option in (("shfmt", "--version"), ("actionlint", "-version")):
            tool(name, root / "build/lint-tools/bin" / name, option)
        tool("shellcheck", "shellcheck")
        tool("semgrep", root / "build/semgrep-venv/bin/pysemgrep")
        tool("shell", "/bin/sh", version=None)
        go = root / "build/go/bin/go"
        if go.exists() or shutil.which("go"):
            tool("go", go if go.exists() else "go", "version")
    baseline = command([native["git"], "-C", root, "rev-parse", (baseline or template["baseline"]) + "^{commit}"])
    folder.mkdir(parents=True, exist_ok=True)
    builds = {name: str(folder / relative) for name, relative in template["builds"].items()}
    if any(Path(value).exists() for value in builds.values()):
        raise ValueError("Managed build directory already exists")
    environment = dict(PATH=os.pathsep.join(dict.fromkeys([str(Path(t["path"]).parent) for t in tools.values()] + os.defpath.split(os.pathsep))),
                       LANG="en_US.UTF-8" if sys.platform == "darwin" else "C.UTF-8",
                       HOME=str(folder / "home"), TMPDIR=str(folder / "tmp"))
    environment.update(SEMGREP_ENABLE_VERSION_CHECK="0", SEMGREP_SEND_METRICS="off", SEMGREP_LOG_FILE=str(folder / "semgrep.log"))
    if ca_bundle is not None:
        environment["SSL_CERT_FILE"] = str(ca_bundle)
    for name in ("home", "tmp"):
        (folder / name).mkdir()
    config_path = folder / "config.json"
    config = dict(schema_version=5, root=str(root), folder=str(folder), baseline=baseline, tools=tools, environment=environment,
                  hash_correctness_ref=command([native["git"], "-C", root, "rev-parse", (hash_correctness_ref or baseline) + "^{commit}"]),
                  ca_bundle=dict(path=str(ca_bundle), sha256=digest(ca_bundle)) if ca_bundle else None,
                  dependency_roots=[".deps/" + n for n in ("googletest", "json", "tomlplusplus", "json-schema-validator")],
                  builds=builds, validated_ref=command([native["git"], "-C", root, "rev-parse", "HEAD"]),
                  ci_run=ci_run, analyzer_env=analyzer_env, check_python=python, python39=python39,
                  analysis_python=analysis_python, prior_attempts=[str((root / path).resolve()) for path in
                      ([prior_attempt] if isinstance(prior_attempt, (str, Path)) else prior_attempt or [])],
                  cases=CASES, sets=template["benchmark"]["sets"],
                  measured=template["benchmark"]["measured_per_set"])
    if sys.platform == "darwin":
        config["sdk"] = dict(path=command(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"], timeout=5),
                             version=command(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-version"], timeout=5))
    atomic_json(config_path, config)
    rel = lambda p: input_name(root, p)
    checks = []
    def add(name, argv, requires=(), policy="required", timeout=600, artifacts=(), **extra):
        checks.append(dict(id=name, argv=[str(a) for a in argv], requires=list(requires), policy=policy,
                           timeout_seconds=timeout, artifacts=list(artifacts),
                           directory_inputs=(config["dependency_roots"] if
                               name.endswith(("-configure", "-build", "-selftest", "-analysis")) or
                               name in ("benchmark-harness", "native-negative-controls") else []) + extra.pop("directory_inputs", []),
                           file_inputs=([dict(path=str(ca_bundle))] if ca_bundle else []) + extra.pop("file_inputs", []), **extra))
    def producer(name, action, requires=(), inputs=(), timeout=600, **extra):
        names = [rel(config_path)] + list(inputs)
        add(name, [python, "scripts/evidence_campaign.py", "produce", config_path, action], requires,
            timeout=timeout, artifacts=[dict(path=rel(folder / (action + ".json")), kind=action, inputs=names)], **extra)
    def configure(source, build, kind):
        argv = [native["cmake"], "-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + kind,
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_MAKE_PROGRAM=" + native["ninja"], "-DCODET_DEPENDENCY_ROOT=" + str(root / ".deps"),
                "-DCMAKE_C_COMPILER=" + native["cc"], "-DCMAKE_CXX_COMPILER=" + native["c++"],
                "-DPython3_EXECUTABLE=" + python]
        if sys.platform == "darwin":
            argv += ["-DCMAKE_OSX_SYSROOT=" + config["sdk"]["path"], "-DCMAKE_OSX_ARCHITECTURES=" + platform.machine(),
                     "-DCMAKE_CXX_FLAGS=-Wno-error=implicit-int-float-conversion -Wno-error=unused-result"]
        return argv
    def configured_outputs(build, compiler_metadata=True):
        names = ["CMakeCache.txt", "compile_commands.json", "build.ninja", "generated/prompts.hpp", "generated/core_contract.hpp"]
        if compiler_metadata:
            names.extend(("codet-compiler-identity.json", "CMakeFiles/codet-dependency-root.txt"))
        return [dict(path=rel(Path(build) / n)) for n in names]
    add("provision-offline", [python, "scripts/fetch-deps.py", "--offline"])
    focused = "--gtest_filter=NativeRecordLifecycle.*:DiagnosticFixture.*:DiagnosticFallbackMetadata.*:DiagnosticReservations.*:LinePrompt.*:DiagnosticWriter.*:TerminalResources.*:PendingControl.*:SubmissionQueue.*:TurnInput.*:TlsTrust.*:InteractionWarnings.*:InputPreparation.*:InputAttachments.*:Runtime.InteractiveExpansion*"
    for mode in ("debug", "asan", "tsan"):
        build = Path(builds[mode])
        args = configure(root, build, "Debug")
        isolation = None
        if mode == "asan":
            args += ["-DCODET_SANITIZERS=ON"]
            if sys.platform == "darwin":
                isolation_inputs = ["scripts/gtest_isolation.py", rel(build / "codet_tests"),
                                    rel(build / "CMakeCache.txt"), rel(build / "CTestTestfile.cmake")]
                isolation = dict(path=rel(build / "unit-isolation.json"), kind="gtest-isolation", inputs=isolation_inputs)
                args += ["-DCODET_UNIT_ISOLATION=ON", "-DCODET_UNIT_ISOLATION_INPUTS=" + json.dumps(isolation_inputs)]
        if mode == "tsan":
            warnings = " -Wno-error=implicit-int-float-conversion -Wno-error=unused-result" if sys.platform == "darwin" else ""
            args += ["-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer" + warnings,
                     "-DCMAKE_C_FLAGS=-fsanitize=thread", "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"]
        instructions = [dict(path=rel(build / "CTestTestfile.cmake"))] if isolation else []
        add(mode + "-configure", args, ["provision-offline"], outputs=configured_outputs(build) + instructions)
        add(mode + "-build", [native["cmake"], "--build", build, "-j4"], [mode + "-configure"],
            outputs=configured_outputs(build) + instructions + [dict(path=rel(build / n), archive=False) for n in ("codet", "codet_tests", "codet_publication_fault_tests", "libcodet_core.a", "libcodet_text.a", "deps/schema/libnlohmann_json_schema_validator.a", "lib/libgtest.a", "lib/libgmock.a")])
        if mode != "tsan":
            add(mode + "-ctest", [native["ctest"], "--test-dir", build, "--output-on-failure", "-j2"], [mode + "-build"], timeout=1200,
                artifacts=[isolation] if isolation else [],
                file_inputs=([dict(path=name, archive=not name.endswith("/codet_tests")) for name in isolation_inputs] if isolation else []) +
                    [dict(path=rel(path)) for path in sorted((root / "licenses").glob("*")) if path.is_file()],
                directory_inputs=["licenses"] + config["dependency_roots"])
        else:
            add("tsan-20", [build / "codet_tests", focused, "--gtest_repeat=20"], [mode + "-build"])
    debug = Path(builds["debug"])
    add("focused-20", [debug / "codet_tests", focused, "--gtest_repeat=20"], ["debug-build"])
    add("python39", [python39, "-m", "unittest", "discover", "-s", "tests", "-p", "test_validation_evidence.py", "-v"], python_version=[3, 9])
    add("python39-attempt", [python39, "-m", "unittest", "discover", "-s", "tests", "-p", "test_attempt_evidence.py", "-v"], python_version=[3, 9])
    add("python39-tiger", [python39, "-m", "unittest", "discover", "-s", "tests", "-p", "test_tiger_recovery.py", "-v"], python_version=[3, 9], directory_inputs=config["dependency_roots"])
    add("python39-integration", [python39, "tests/hermetic.py", python39, "tests/cli_integration.py", debug / "codet"], ["debug-build"], python_version=[3, 9])
    add("lint", [native["make"], "check-lint", "CHECK_PYTHON=" + python])
    add("semgrep", [native["make"], "check-semgrep", "CHECK_PYTHON=" + python, "SEMGREP=" + str(root / "build/semgrep-venv/bin/pysemgrep")])
    add("runner-tests", [native["make"], "test-lint-runners", "test-modern-analysis-runner", "CHECK_PYTHON=" + python, "ANALYSIS_PYTHON=" + analysis_python])
    cross = Path(builds["cross"])
    producer("cross-placeholder", "cross-placeholder", outputs=[dict(path=rel(folder / "host-prefix/lib/libMacportsLegacySupport.a")), dict(path=rel(folder / "host-placeholder.c"))])
    add("cross-configure", configure(root, cross, "Debug") + ["-DCMAKE_SYSTEM_NAME=" + platform.system(), "-D_prefix=" + str(folder / "host-prefix")],
        ["cross-placeholder", "provision-offline"], outputs=configured_outputs(cross))
    add("cross-build", [native["cmake"], "--build", cross, "--target", "codet_tests", "-j4"], ["cross-configure"],
        outputs=[dict(path=rel(cross / "codet_tests"), archive=False)])
    add("cross-tests", [cross / "codet_tests", "--gtest_filter=LinePrompt.*:InteractionWarnings.*:CursesInput.*"], ["cross-build"])
    if config["prior_attempts"]:
        producer("historical-attempt", "history", inputs=[rel(p) for p in history_files(config["prior_attempts"])])
    producer("baseline-source", "archive")
    for variant in ("baseline", "changed"):
        source = folder / "baseline-src" if variant == "baseline" else root
        build = folder / (variant + "-release")
        specs = [dict(path=rel(source), git_ref=baseline, git=native["git"])] if variant == "baseline" else [dict(path=".")]
        add(variant + "-configure", configure(source, build, "Release"), ["baseline-source", "provision-offline"] if variant == "baseline" else ["provision-offline"],
            outputs=configured_outputs(build, variant == "changed"), source_inputs=specs)
        add(variant + "-build", [native["cmake"], "--build", build, "--target", "codet_line_input_bench", "codet_output_bench", "codet_export_bench", "-j4"],
            [variant + "-configure"], source_inputs=specs,
            outputs=configured_outputs(build, variant == "changed") + [dict(path=rel(build / n), archive=False) for n in ("codet_line_input_bench", "codet_output_bench", "codet_export_bench", "libcodet_core.a", "deps/schema/libnlohmann_json_schema_validator.a")])
        probe = rel(build / "analysis/selftest/selftest.json")
        add(variant + "-selftest", [analysis_python, source / "scripts/analyze-modern.py", "--build-dir", build, "--selftest", "--jobs", "4"],
            [variant + "-configure"], env=analyzer_env, source_inputs=specs, file_inputs=configured_outputs(build, variant == "changed"), outputs=[dict(path=probe)])
        add(variant + "-analysis", [python, "scripts/evidence_campaign.py", "produce", config_path, "analysis-" + variant],
            [variant + "-selftest"], policy="observation", timeout=1800, env=analyzer_env, source_inputs=specs, file_inputs=configured_outputs(build, variant == "changed"),
            artifacts=[dict(path=rel(folder / ("analysis-" + variant + ".json")), kind="analysis-" + variant, inputs=[rel(config_path), probe])])
    producer("static-comparison", "analysis", ["baseline-analysis", "changed-analysis"], [rel(folder / ("analysis-" + v + ".json")) for v in ("baseline", "changed")] + ["checks/analyzer-relocations.json"])
    producer("semgrep-controls", "semgrep-controls", ["semgrep"], timeout=300)
    producer("python-negative-controls", "negative-controls", ["runner-tests"])
    producer("native-negative-controls", "native-negative-controls", ["debug-build"], [rel(debug / "compile_commands.json"), rel(debug / "build.ninja")], timeout=2100)
    harness_inputs = list(HARNESS_FILES) + [rel(folder / (v + "-release" ) / n) for v in ("baseline", "changed") for n in ("compile_commands.json", "build.ninja", "libcodet_core.a")]
    harness_outputs = [dict(path=rel(folder / (v + "-harness") / n), archive=False) for v in ("baseline", "changed") for n in ("codet_line_input_bench", "codet_output_bench", "codet_export_bench")]
    producer("benchmark-harness", "harness", ["baseline-build", "changed-build"], harness_inputs, timeout=1800, outputs=harness_outputs,
             file_inputs=[dict(path=rel(folder / (v + "-release/libcodet_core.a")), archive=False) for v in ("baseline", "changed")])
    producer("benchmarks", "measure", ["benchmark-harness", "static-comparison", "python-negative-controls", "native-negative-controls", "semgrep-controls"],
             [rel(folder / (v + "-release/CMakeCache.txt")) for v in ("baseline", "changed")] + [rel(folder / "harness.json")], timeout=1800)
    metadata_inputs = [rel(Path(b) / "CMakeCache.txt") for b in builds.values()] + [rel(folder / (v + "-release/CMakeCache.txt")) for v in ("baseline", "changed")]
    producer("platform-availability", "platforms")
    producer("build-metadata", "build-info", ["debug-build", "asan-build", "tsan-build", "cross-build", "baseline-build", "changed-build"], metadata_inputs)
    if ci_run:
        producer("ubuntu", "ci", timeout=1800)
    atomic_json(folder / "manifest.json", dict(schema_version=5, root=str(root), baseline=baseline, single_attempt=True, environment=environment, tools=tools, checks=checks))
    return folder / "manifest.json"


def positive_controls(row, harness, case):
    if harness == "line":
        if case == "allocation-positive" and (row["allocations"], row["allocated_bytes"]) != (1000, 32000):
            raise ValueError("Allocation positive control failed")
        if case == "geometry-positive" and row["geometry_queries"] != 1000:
            raise ValueError("Geometry positive control failed")
        if case in ("unchanged", "native-unchanged") and any(row[k] for k in ("bytes", "output_operations", "geometry_queries", "allocations")):
            raise ValueError("Unchanged redraw performed work")
    if harness == "line" and case in ("preparation-queue", "bounded-expansion"):
        expected = (262144 if case == "bounded-expansion" else 135) * 128
        if row["prepared_submissions"] != 128 or row["effective_content_bytes"] != expected:
            raise ValueError("Prepared submission counter positive control failed")
    if harness == "output" and case.endswith("-positive"):
        key = {"write-positive": "writes", "stat-positive": "fstat", "flags-positive": "fcntl", "bounds-positive": "fpathconf"}[case]
        if row[key] != 128:
            raise ValueError("Syscall positive control failed")


def measure(config):
    root, folder = Path(config["root"]).resolve(), Path(config["folder"]).resolve()
    require_quiet_processes()
    baseline = verify_git_source(root, folder / "baseline-src", config["baseline"])
    harness_build = json.loads((folder / "harness.json").read_text())["payload"]
    harnesses = {v: harness_build["harness_hashes"] for v in ("baseline", "changed")}
    for name, value in harness_build["harness_hashes"].items():
        if digest(folder / "harness-src" / name) != value:
            raise ValueError("Declared benchmark harness changed")
    binaries = {v: {h: executable_provenance(Path(record["executable"]))
                   for h, record in programs.items()} for v, programs in harness_build["variants"].items()}
    configurations = {}
    for variant in ("baseline", "changed"):
        cache = folder / (variant + "-release") / "CMakeCache.txt"
        values = {}
        for line in cache.read_text().splitlines():
            if line.startswith(("CMAKE_C_FLAGS:", "CMAKE_CXX_FLAGS:", "CMAKE_C_FLAGS_RELEASE:", "CMAKE_CXX_FLAGS_RELEASE:", "CMAKE_BUILD_TYPE:", "CMAKE_C_COMPILER:", "CMAKE_CXX_COMPILER:", "CMAKE_OSX_ARCHITECTURES:")):
                key, value = line.split("=", 1)
                values[key.split(":", 1)[0]] = value
        configurations[variant] = dict(values=values, cache_sha256=digest(cache), compiler=command([values["CMAKE_CXX_COMPILER"], "--version"]))
    if configurations["baseline"]["values"] != configurations["changed"]["values"]:
        raise ValueError("Release compiler settings differ")
    rows, invocations = [], []
    for number in range(1, config["sets"] + 1):
        for harness, cases in config["cases"].items():
            for case in cases:
                require_quiet_processes()
                for sample in range(config["measured"] + 1):
                    variants = ("baseline", "changed") if (number + sample) % 2 == 0 else ("changed", "baseline")
                    for variant in variants:
                        result = folder / "measurement.json"
                        result.unlink(missing_ok=True)
                        if harness == "tooling":
                            source = folder / "baseline-src" if variant == "baseline" else root
                            ref = config["baseline"] if variant == "baseline" else config["validated_ref"]
                            argv = [config["check_python"], str(folder / "harness-src/scripts/evidence_tool_bench.py"), str(source), ref, case, str(result), str(root)]
                        else:
                            argv = [binaries[variant][harness]["path"], case, str(result)] + (["file"] if harness == "export" else [])
                        run = supervised_run(argv, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, timeout=180)
                        if run.returncode:
                            raise ValueError("Benchmark failed: " + repr((argv, run.returncode, run.stderr)))
                        row = json.loads(result.read_text())
                        if row.get("case") != case:
                            raise ValueError("Benchmark returned a different workload")
                        positive_controls(row, harness, case)
                        row.update(set=number, sample=sample, variant=variant, harness=harness, workload=case, warmup=sample == 0)
                        rows.append(row)
                        invocations.append(dict(argv=argv, exit=run.returncode, stderr=run.stderr, result_sha256=digest(result)))
                print("Measured", number, harness, case, flush=True)
    summary = []
    for number in range(1, config["sets"] + 1):
        for harness, cases in config["cases"].items():
            for case in cases:
                for variant in ("baseline", "changed"):
                    values = [r for r in rows if r["set"] == number and r["harness"] == harness and r["workload"] == case and r["variant"] == variant and not r["warmup"]]
                    metrics = [k for k, v in values[0].items() if type(v) in (int, float) and k not in ("set", "sample")]
                    summary.append(dict(set=number, harness=harness, workload=case, variant=variant,
                                        **{k: statistics.median(r[k] for r in values) for k in metrics}))
    verified = verify_samples(rows, summary, config["cases"], sets=config["sets"], measured=config["measured"])
    require_quiet_processes()
    for programs in binaries.values():
        for executable in programs.values():
            if executable_provenance(Path(executable["path"])) != executable:
                raise ValueError("Benchmark executable changed during measurement")
    return dict(**verified, rows=rows, summary=summary, invocations=invocations, baseline=baseline,
                harness_hashes=harnesses, executables=binaries, configurations=configurations, harness_build=harness_build, host=dict(platform=platform.platform(), machine=platform.machine()))


def produce(config_path, action, *, output=None):
    config_path = config_path.resolve()
    if "CODET_EVIDENCE_CONTEXT" not in os.environ and output is None:
        raise ValueError("Standalone production requires explicit --output")
    config = json.loads(config_path.read_text())
    root, folder = Path(config["root"]).resolve(), Path(config["folder"]).resolve()
    if "CODET_EVIDENCE_CONTEXT" in os.environ and output is not None:
        declarations = json.loads(os.environ["CODET_EVIDENCE_CONTEXT"]).get("declared_artifacts", [])
        if not any((root / a["path"]).resolve() == output.resolve() and a["kind"] == action for a in declarations):
            raise ValueError("Captured output is not declared by its manifest")
    inputs = [input_name(root, config_path)]
    def save(kind, payload):
        if "CODET_EVIDENCE_CONTEXT" in os.environ:
            write_artifact(output.resolve() if output is not None else folder / (kind + ".json"), kind, payload, inputs)
        else:
            from validation_evidence import (
                SCHEMA_VERSION,
                input_hashes,
                production_hashes,
                validation_hashes,
            )
            atomic_json(output.resolve(), dict(schema_version=SCHEMA_VERSION, binding="standalone-unbound", kind=kind,
                        production_hashes=production_hashes(root), validation_hashes=validation_hashes(root),
                        input_hashes=input_hashes(root, inputs), payload=payload))
    if action == "history":
        paths = history_files(config["prior_attempts"])
        inputs.extend(input_name(root, p) for p in paths)
        payload = dict(label="Prior campaign attempts; original failed/interrupted outcomes, historical only, never a required-check substitute",
                       files=[dict(path=input_name(root, p), sha256=digest(p)) for p in paths])
    elif action == "archive":
        source = folder / "baseline-src"
        if source.exists():
            raise ValueError("Baseline archive destination already exists")
        source.mkdir()
        archive = folder / "baseline.tar"
        command([config["tools"]["git"]["path"], "-C", root, "archive", "--format=tar", "--output=" + str(archive), config["baseline"]])
        command([config["tools"]["tar"]["path"], "-xf", archive, "-C", source])
        (source / ".deps").symlink_to(root / ".deps", target_is_directory=True)
        payload = verify_git_source(root, source, config["baseline"])
    elif action.startswith("analysis-"):
        variant = action.removeprefix("analysis-")
        source = folder / "baseline-src" if variant == "baseline" else root
        build = folder / (variant + "-release")
        probe = build / "analysis/selftest/selftest.json"
        inputs.append(input_name(root, probe))
        if json.loads(probe.read_text()) != {t: True for t in TOOLS}:
            raise ValueError("Analyzer self-check did not pass")
        for tool in TOOLS:
            directory = build / "analysis" / tool
            if directory.exists():
                shutil.rmtree(directory)
        run = supervised_run([config["analysis_python"], str(source / "scripts/analyze-modern.py"),
                              "--build-dir", str(build), "--tool", "all", "--jobs", "4"], timeout=1750, cleanup_grace=4)
        if variant == "baseline":
            verify_git_source(root, source, config["baseline"])
            verify_git_validation(root, source, config["baseline"])
        expected = [str(p.relative_to(source)) for p in application_sources(source)]
        manifests = {t: json.loads((build / "analysis" / t / "manifest.json").read_text()) for t in TOOLS}
        # Older analyzer runners keep plist file indices; resolve using their exact report.
        import plistlib
        for result in manifests["clang-analyzer"]["results"]:
            if not result["findings"]:
                continue
            argv = result["command"]
            report = Path(argv[argv.index("-o") + 1])
            files = plistlib.loads(report.read_bytes()).get("files", [])
            result["findings"] = resolve_files(result["findings"], files)
        for manifest in manifests.values():
            verify_analysis(manifest, source, expected)
        if run.returncode not in (0, 1):
            raise ValueError("Analyzer runner did not complete")
        from validation_evidence import production_hashes
        payload = dict(argv=[config["analysis_python"], str(source / "scripts/analyze-modern.py")],
                       exit=run.returncode, source_hashes=production_hashes(source), manifests=manifests)
        save(action, payload)
        return run.returncode
    elif action == "analysis":
        expected = [str(p.relative_to(root)) for p in application_sources(root)]
        paths = [folder / ("analysis-" + v + ".json") for v in ("baseline", "changed")]
        inputs.extend(input_name(root, p) for p in paths)
        values = [json.loads(p.read_text())["payload"] for p in paths]
        registry_path = root / "checks/analyzer-relocations.json"
        inputs.append(input_name(root, registry_path))
        registry = json.loads(registry_path.read_text())
        from validation_evidence import production_hashes
        if registry.get("entries") and (registry.get("baseline") != production_hashes(folder / "baseline-src") or registry.get("changed") != production_hashes(root)):
            raise ValueError("Analyzer relocation registry source mismatch")
        payload = []
        for tool in TOOLS:
            value = compare_analysis(values[0]["manifests"][tool], values[1]["manifests"][tool], folder / "baseline-src", root,
                                     [str(p.relative_to(folder / "baseline-src")) for p in application_sources(folder / "baseline-src")], expected,
                                     [entry for entry in registry.get("entries", []) if entry["tool"] == tool])
            if not value["equal"]:
                raise ValueError("Analyzer findings differ: " + repr(value))
            payload.append(value)
    elif action == "semgrep-controls":
        from evidence_semgrep_controls import run_controls
        payload = run_controls(root, folder, config["tools"]["semgrep"]["path"])
    elif action == "platforms":
        payload = dict(host=sys.platform, native_tiger=False, ubuntu_runtime="not available locally", tools=[])
        for name in ("docker", "podman", "powerpc-apple-darwin8-g++"):
            path = shutil.which(name)
            result = dict(tool=name, path=path, outcome="unavailable")
            if path:
                run = supervised_run([path, "--version"], capture_output=True, text=True, timeout=5)
                result.update(outcome="available", exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
            payload["tools"].append(result)
    elif action == "harness":
        from evidence_harness import build_harnesses
        inputs.extend(HARNESS_FILES)
        inputs.extend(input_name(root, folder / (v + "-release") / n) for v in ("baseline", "changed") for n in ("compile_commands.json", "build.ninja", "libcodet_core.a"))
        payload = build_harnesses(root, folder, HARNESS_FILES, config["tools"]["ninja"]["path"])
    elif action == "measure":
        inputs.append(input_name(root, folder / "harness.json"))
        inputs.extend(input_name(root, folder / (v + "-release/CMakeCache.txt")) for v in ("baseline", "changed"))
        payload = measure(config)
    elif action == "negative-controls":
        from evidence_negative_controls import run_controls
        payload = run_controls(root, folder)
    elif action == "native-negative-controls":
        from evidence_native_controls import run_controls
        build = Path(config["builds"]["debug"]).resolve()
        inputs.extend(input_name(root, build / p) for p in ("compile_commands.json", "build.ninja"))
        payload = run_controls(root, folder, build, config["tools"]["ninja"]["path"])
    elif action == "cross-placeholder":
        source = folder / "host-placeholder.c"
        source.write_text("/* Host-only unused link-list placeholder; no Tiger implementation. */\nint codet_host_placeholder;\n")
        obj = folder / "host-placeholder.o"
        archive = folder / "host-prefix/lib/libMacportsLegacySupport.a"
        archive.parent.mkdir(parents=True, exist_ok=True)
        command([config["tools"]["cc"]["path"], "-c", source, "-o", obj], timeout=600)
        command([config["tools"]["ar"]["path"], "rcs", archive, obj], timeout=600)
        payload = dict(label="Host cross-selection only; not a PPC/Tiger build", source_sha256=digest(source), archive_sha256=digest(archive))
    elif action == "build-info":
        payload = []
        for name, build in [(n, Path(b).resolve()) for n, b in config["builds"].items()] + [(v, folder / (v + "-release")) for v in ("baseline", "changed")]:
            cache = build / "CMakeCache.txt"
            inputs.append(input_name(root, cache))
            compiler = next(line.split("=", 1)[1] for line in cache.read_text().splitlines() if line.startswith("CMAKE_CXX_COMPILER:"))
            payload.append(dict(name=name, cache=cache.read_text(), cache_sha256=digest(cache),
                                compiler_path=compiler, compiler_sha256=digest(Path(compiler)), compiler=command([compiler, "--version"])))
    elif action == "hash-profile":
        import io
        import tarfile

        from evidence_hash_experiment import measure as hash_measure
        from evidence_inputs import git_hashes
        source = folder / "hash-correctness"
        source.mkdir()
        ref = config["hash_correctness_ref"]
        names = {"scripts/evidence_inputs.py", "scripts/verified_sources.py"}
        data = subprocess.check_output([config["tools"]["git"]["path"], "-C", str(root), "archive", ref, *sorted(names)], timeout=30)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            members = [m for m in archive.getmembers() if m.isfile()]
            if {m.name for m in members} != names or len(members) != len(names):
                raise ValueError("Hash correctness archive coverage mismatch")
            for member in members:
                target = source / member.name
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as stream, target.open("wb") as target_stream:
                    shutil.copyfileobj(stream, target_stream)
        expected = git_hashes(root, ref, "validation", config["tools"]["git"]["path"])
        if any(digest(source / name) != expected[name] for name in names):
            raise ValueError("Hash correctness snapshot does not match its recorded commit")
        payload = hash_measure(root, source, config["check_python"])
        payload["correctness_ref"] = ref
    elif action == "ci":
        from evidence_ci import read_public_ci
        payload = read_public_ci("hbmartin/codet-compiled-agent", config["ci_run"], config["validated_ref"])
        verify_git_source(root, root, payload["head_sha"])
        verify_git_validation(root, root, payload["head_sha"])
    else:
        raise ValueError("Unknown producer: " + action)
    save(action, payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    replay = sub.add_parser("prepare")
    replay.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    replay.add_argument("--folder", type=Path, default=Path("build/pending-input-replay-followup"))
    replay.add_argument("--baseline")
    replay.add_argument("--check-python", default=sys.executable)
    replay.add_argument("--python39")
    replay.add_argument("--template", type=Path)
    replay.add_argument("--analysis-python", default=sys.executable)
    replay.add_argument("--ci-run")
    replay.add_argument("--ca-bundle", type=Path)
    replay.add_argument("--hash-correctness-ref")
    replay.add_argument("--prior-attempt", type=Path, action="append")
    replay.add_argument("--clang-tidy")
    replay.add_argument("--cppcheck")
    producer = sub.add_parser("produce")
    producer.add_argument("config", type=Path)
    producer.add_argument("kind", choices=("archive", "harness", "platforms", "semgrep-controls", "analysis", "analysis-baseline", "analysis-changed", "measure", "negative-controls", "native-negative-controls", "cross-placeholder", "build-info", "ci", "history", "hash-profile"))
    producer.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.action == "prepare":
        print(prepare(args.root, args.folder, args.baseline, args.check_python, args.python39, args.analysis_python, args.ci_run, args.clang_tidy, args.cppcheck, args.template, args.prior_attempt, args.ca_bundle, args.hash_correctness_ref))
    else:
        return produce(args.config, args.kind, output=args.output)


if __name__ == "__main__":
    raise SystemExit(main())
