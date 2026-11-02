"""Resolve portable replay settings and produce verifiable follow-up evidence."""

import argparse
import csv
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

from evidence_analysis import compare_analysis, verify_analysis
from evidence_benchmarks import (
    executable_provenance,
    verify_git_source,
    verify_git_validation,
    verify_samples,
)
from modern_sources import application_sources
from validation_evidence import atomic_json, digest, require_quiet_processes, write_artifact

CASES = {
    "line": ["allocation-positive", "geometry-positive", "busy", "native-busy", "editing", "editing-basic",
             "native-editing", "deletion", "unchanged", "deferred-tail", "native-boundary", "native-flush-boundary", "native-unchanged"],
    "output": ["write-positive", "stat-positive", "flags-positive", "bounds-positive", "diagnostics-amplified",
               "diagnostics-standalone-amplified", "diagnostics-pipe-amplified"],
    "export": ["small", "large"],
}
HARNESS_FILES = ["tests/line_input_bench.cpp", "tests/output_bench.cpp", "tests/export_bench.cpp",
                 "tests/descriptor_bench_metrics.hpp", "tests/native_stat_interposer.hpp", "tests/native_symbol.hpp", "tests/temp.hpp"]
TOOLS = ("clang-tidy", "cppcheck", "clang-analyzer")


def command(argv, *, cwd=None, timeout=30):
    return subprocess.check_output([str(s) for s in argv], cwd=cwd, text=True, timeout=timeout).strip()


def prepare(root, folder, baseline=None, check_python=None, python39=None, analysis_python=None,
            ci_run=None, clang_tidy=None, cppcheck=None, template_path=None):
    from evidence_inputs import executable
    root = root.resolve()
    folder = (root / folder).resolve()
    if (folder / "config.json").exists():
        raise ValueError("Campaign configuration already exists; choose a fresh folder")
    template_path = template_path or Path(__file__).resolve().parents[1] / "checks/validation-replay.json"
    template = json.loads(template_path.read_text())
    tools = {}
    def tool(name, candidate, version="--version", optional=False):
        path = shutil.which(str(candidate))
        if path is None:
            if optional:
                return str(candidate)
            raise ValueError("Required tool unavailable: " + name + ": " + str(candidate))
        identity = executable([path], {"PATH": os.defpath})
        banner = command([identity["path"], version], timeout=5) if version else "No supported version probe; executable digest recorded"
        tools[name] = dict(identity, version=banner)
        return identity["path"]
    python = tool("check-python", check_python or sys.executable)
    analysis_python = tool("analysis-python", analysis_python or python)
    python39 = tool("python39", python39 or template["python39"], optional=True)
    native = {n: tool(n, n, version=None if n == "ar" and sys.platform == "darwin" else "--version")
              for n in ("git", "cmake", "ctest", "ninja", "make", "tar", "ar", "cc", "c++")}
    analyzer_env = dict(CLANG_TIDY=tool("clang-tidy", clang_tidy or shutil.which("clang-tidy-18") or shutil.which("clang-tidy") or "clang-tidy-18"),
                        CPPCHECK=tool("cppcheck", cppcheck or root / "build/analysis-tools/cppcheck/bin/cppcheck"))
    baseline = command([native["git"], "-C", root, "rev-parse", (baseline or template["baseline"]) + "^{commit}"])
    folder.mkdir(parents=True, exist_ok=True)
    builds = {name: str(folder / relative) for name, relative in template["builds"].items()}
    environment = dict(PATH=os.pathsep.join(dict.fromkeys([str(Path(t["path"]).parent) for t in tools.values()] + os.defpath.split(os.pathsep))),
                       LANG=os.environ.get("LANG", "en_US.UTF-8" if sys.platform == "darwin" else "C.UTF-8"))
    config_path = folder / "config.json"
    config = dict(root=str(root), folder=str(folder), baseline=baseline, tools=tools, environment=environment,
                  builds=builds, validated_ref=command([native["git"], "-C", root, "rev-parse", "HEAD"]),
                  ci_run=ci_run, analyzer_env=analyzer_env, check_python=python, python39=python39,
                  analysis_python=analysis_python, cases=CASES, sets=template["benchmark"]["sets"],
                  measured=template["benchmark"]["measured_per_set"])
    if sys.platform == "darwin":
        config["sdk"] = dict(path=command(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"], timeout=5),
                             version=command(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-version"], timeout=5))
    atomic_json(config_path, config)
    rel = lambda p: str(Path(p).relative_to(root))
    checks = []
    def add(name, argv, requires=(), policy="required", timeout=600, artifacts=(), **extra):
        checks.append(dict(id=name, argv=[str(a) for a in argv], requires=list(requires), policy=policy,
                           timeout_seconds=timeout, artifacts=list(artifacts),
                           directory_inputs=[".deps/" + n for n in ("googletest", "json", "tomlplusplus", "json-schema-validator")], **extra))
    def producer(name, action, requires=(), inputs=(), timeout=600, **extra):
        names = [rel(config_path)] + list(inputs)
        add(name, [python, "scripts/evidence_campaign.py", "produce", config_path, action], requires,
            timeout=timeout, artifacts=[dict(path=rel(folder / (action + ".json")), kind=action, inputs=names)], **extra)
    def configure(source, build, kind):
        argv = [native["cmake"], "-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + kind,
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCODET_DEPENDENCY_ROOT=" + str(root / ".deps"),
                "-DCMAKE_C_COMPILER=" + native["cc"], "-DCMAKE_CXX_COMPILER=" + native["c++"],
                "-DPython3_EXECUTABLE=" + python]
        if sys.platform == "darwin":
            argv += ["-DCMAKE_OSX_SYSROOT=" + config["sdk"]["path"], "-DCMAKE_OSX_ARCHITECTURES=" + platform.machine(),
                     "-DCMAKE_CXX_FLAGS=-Wno-error=implicit-int-float-conversion -Wno-error=unused-result"]
        return argv
    def configured_outputs(build):
        return [dict(path=rel(Path(build) / n)) for n in ("CMakeCache.txt", "compile_commands.json", "build.ninja", "generated/prompts.hpp", "generated/core_contract.hpp")]
    focused = "--gtest_filter=NativeRecordLifecycle.*:DiagnosticFixture.*:DiagnosticFallbackMetadata.*:DiagnosticReservations.*:LinePrompt.*:DiagnosticWriter.*:TerminalResources.*"
    for mode in ("debug", "asan", "tsan"):
        build = Path(builds[mode])
        args = configure(root, build, "Debug")
        if mode == "asan":
            args += ["-DCODET_SANITIZERS=ON"]
        if mode == "tsan":
            args += ["-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer -Wno-error=implicit-int-float-conversion -Wno-error=unused-result",
                     "-DCMAKE_C_FLAGS=-fsanitize=thread", "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"]
        add(mode + "-configure", args, outputs=configured_outputs(build))
        add(mode + "-build", [native["cmake"], "--build", build, "-j4"], [mode + "-configure"],
            outputs=[dict(path=rel(build / n), archive=False) for n in ("codet", "codet_tests", "codet_publication_fault_tests")])
        if mode != "tsan":
            add(mode + "-ctest", [native["ctest"], "--test-dir", build, "--output-on-failure", "-j2"], [mode + "-build"], timeout=900)
        else:
            add("tsan-20", [build / "codet_tests", focused, "--gtest_repeat=20"], [mode + "-build"])
    debug = Path(builds["debug"])
    add("focused-20", [debug / "codet_tests", focused, "--gtest_repeat=20"], ["debug-build"])
    add("python39", [python39, "-m", "unittest", "discover", "-s", "tests", "-p", "test_validation_evidence.py", "-v"], python_version=[3, 9])
    add("python39-integration", [python39, "tests/hermetic.py", python39, "tests/cli_integration.py", debug / "codet"], ["debug-build"], python_version=[3, 9])
    add("lint", [native["make"], "check-lint", "CHECK_PYTHON=" + python])
    add("semgrep", [native["make"], "check-semgrep", "CHECK_PYTHON=" + python, "SEMGREP=" + str(root / "build/semgrep-venv/bin/pysemgrep")])
    add("runner-tests", [native["make"], "test-lint-runners", "test-modern-analysis-runner", "CHECK_PYTHON=" + python, "ANALYSIS_PYTHON=" + analysis_python])
    cross = Path(builds["cross"])
    producer("cross-placeholder", "cross-placeholder", outputs=[dict(path=rel(folder / "host-prefix/lib/libMacportsLegacySupport.a")), dict(path=rel(folder / "host-placeholder.c"))])
    add("cross-configure", configure(root, cross, "Debug") + ["-DCMAKE_SYSTEM_NAME=" + platform.system(), "-D_prefix=" + str(folder / "host-prefix")],
        ["cross-placeholder"], outputs=configured_outputs(cross))
    add("cross-build", [native["cmake"], "--build", cross, "--target", "codet_tests", "-j4"], ["cross-configure"],
        outputs=[dict(path=rel(cross / "codet_tests"), archive=False)])
    add("cross-tests", [cross / "codet_tests", "--gtest_filter=LinePrompt.*:InteractionWarnings.*:CursesInput.*"], ["cross-build"])
    if (folder / "preliminary-capture").exists():
        history_inputs = [rel(p) for p in sorted((folder / "preliminary-capture").rglob("*")) if p.is_file()]
        producer("historical-attempt", "history", inputs=history_inputs)
    producer("baseline-source", "archive")
    for variant in ("baseline", "changed"):
        source = folder / "baseline-src" if variant == "baseline" else root
        build = folder / (variant + "-release")
        specs = [dict(path=rel(source), git_ref=baseline, git=native["git"])] if variant == "baseline" else [dict(path=".")]
        add(variant + "-configure", configure(source, build, "Release"), ["baseline-source"] if variant == "baseline" else [],
            outputs=configured_outputs(build), source_inputs=specs)
        add(variant + "-build", [native["cmake"], "--build", build, "--target", "codet_line_input_bench", "codet_output_bench", "codet_export_bench", "-j4"],
            [variant + "-configure"], source_inputs=specs,
            outputs=[dict(path=rel(build / n), archive=False) for n in ("codet_line_input_bench", "codet_output_bench", "codet_export_bench")])
        probe = rel(build / "analysis/selftest/selftest.json")
        add(variant + "-selftest", [analysis_python, source / "scripts/analyze-modern.py", "--build-dir", build, "--selftest", "--jobs", "4"],
            [variant + "-configure"], env=analyzer_env, source_inputs=specs, outputs=[dict(path=probe)])
        add(variant + "-analysis", [python, "scripts/evidence_campaign.py", "produce", config_path, "analysis-" + variant],
            [variant + "-selftest"], policy="observation", timeout=1800, env=analyzer_env, source_inputs=specs,
            artifacts=[dict(path=rel(folder / ("analysis-" + variant + ".json")), kind="analysis-" + variant, inputs=[rel(config_path), probe])])
    producer("static-comparison", "analysis", ["baseline-analysis", "changed-analysis"], [rel(folder / ("analysis-" + v + ".json")) for v in ("baseline", "changed")])
    producer("python-negative-controls", "negative-controls", ["runner-tests"])
    producer("native-negative-controls", "native-negative-controls", ["debug-build"], [rel(debug / "compile_commands.json"), rel(debug / "build.ninja")], timeout=2100)
    producer("benchmarks", "measure", ["baseline-build", "changed-build", "static-comparison", "python-negative-controls", "native-negative-controls"],
             [rel(folder / (v + "-release/CMakeCache.txt")) for v in ("baseline", "changed")], timeout=1800)
    metadata_inputs = [rel(Path(b) / "CMakeCache.txt") for b in builds.values()] + [rel(folder / (v + "-release/CMakeCache.txt")) for v in ("baseline", "changed")]
    producer("build-metadata", "build-info", ["debug-build", "asan-build", "tsan-build", "cross-build", "baseline-build", "changed-build"], metadata_inputs)
    if ci_run:
        producer("ubuntu", "ci", timeout=1800)
    atomic_json(folder / "manifest.json", dict(root=str(root), baseline=baseline, environment=environment, tools=tools, checks=checks))
    return folder / "manifest.json"


def positive_controls(row, harness, case):
    if harness == "line":
        if case == "allocation-positive" and (row["allocations"], row["allocated_bytes"]) != (1000, 32000):
            raise ValueError("Allocation positive control failed")
        if case == "geometry-positive" and row["geometry_queries"] != 1000:
            raise ValueError("Geometry positive control failed")
        if case in ("unchanged", "native-unchanged") and any(row[k] for k in ("bytes", "output_operations", "geometry_queries", "allocations")):
            raise ValueError("Unchanged redraw performed work")
    if harness == "output" and case.endswith("-positive"):
        key = {"write-positive": "writes", "stat-positive": "fstat", "flags-positive": "fcntl", "bounds-positive": "fpathconf"}[case]
        if row[key] != 128:
            raise ValueError("Syscall positive control failed")


def measure(config):
    root, folder = Path(config["root"]).resolve(), Path(config["folder"]).resolve()
    require_quiet_processes()
    baseline = verify_git_source(root, folder / "baseline-src", config["baseline"])
    harnesses = {v: {n: digest((folder / "baseline-src" if v == "baseline" else root) / n) for n in HARNESS_FILES}
                 for v in ("baseline", "changed")}
    if harnesses["baseline"] != harnesses["changed"]:
        raise ValueError("Benchmark harness sources differ")
    binaries = {v: {h: executable_provenance(folder / (v + "-release") / executable)
                   for h, executable in dict(line="codet_line_input_bench", output="codet_output_bench", export="codet_export_bench").items()}
                for v in ("baseline", "changed")}
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
                        argv = [binaries[variant][harness]["path"], case, str(result)] + (["file"] if harness == "export" else [])
                        run = subprocess.run(argv, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, timeout=180)
                        if run.returncode:
                            raise ValueError("Benchmark failed: " + repr((argv, run.returncode, run.stderr)))
                        row = json.loads(result.read_text())
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
                harness_hashes=harnesses, executables=binaries, configurations=configurations, host=dict(platform=platform.platform(), machine=platform.machine()))


def produce(config_path, action, *, output=None):
    config_path = config_path.resolve()
    if "CODET_EVIDENCE_CONTEXT" not in os.environ and output is None:
        raise ValueError("Standalone production requires explicit --output")
    config = json.loads(config_path.read_text())
    root, folder = Path(config["root"]).resolve(), Path(config["folder"]).resolve()
    inputs = [str(config_path.relative_to(root))]
    def save(kind, payload):
        if "CODET_EVIDENCE_CONTEXT" in os.environ:
            if output is not None:
                raise ValueError("Captured producer output is selected by its manifest")
            write_artifact(folder / (kind + ".json"), kind, payload, inputs)
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
        paths = sorted(p for p in (folder / "preliminary-capture").rglob("*") if p.is_file())
        inputs.extend(str(p.relative_to(root)) for p in paths)
        payload = dict(label="Interrupted preliminary capture; historical only, never a required-check substitute",
                       files=[dict(path=str(p.relative_to(root)), sha256=digest(p), content=json.loads(p.read_text()) if p.suffix == ".json" else p.read_text(errors="replace")) for p in paths])
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
        inputs.append(str(probe.relative_to(root)))
        if json.loads(probe.read_text()) != {t: True for t in TOOLS}:
            raise ValueError("Analyzer self-check did not pass")
        for tool in TOOLS:
            directory = build / "analysis" / tool
            if directory.exists():
                shutil.rmtree(directory)
        run = subprocess.run([config["analysis_python"], str(source / "scripts/analyze-modern.py"),
                              "--build-dir", str(build), "--tool", "all", "--jobs", "4"], timeout=1750)
        if variant == "baseline":
            verify_git_source(root, source, config["baseline"])
            verify_git_validation(root, source, config["baseline"])
        expected = [str(p.relative_to(source)) for p in application_sources(source)]
        manifests = {t: json.loads((build / "analysis" / t / "manifest.json").read_text()) for t in TOOLS}
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
        inputs.extend(str(p.relative_to(root)) for p in paths)
        values = [json.loads(p.read_text())["payload"] for p in paths]
        payload = []
        for tool in TOOLS:
            value = compare_analysis(values[0]["manifests"][tool], values[1]["manifests"][tool], folder / "baseline-src", root, expected)
            if not value["equal"]:
                raise ValueError("Analyzer findings differ: " + repr(value))
            payload.append(value)
    elif action == "measure":
        inputs.extend(str((folder / (v + "-release/CMakeCache.txt")).relative_to(root)) for v in ("baseline", "changed"))
        payload = measure(config)
    elif action == "negative-controls":
        from evidence_negative_controls import run_controls
        payload = run_controls(root, folder)
    elif action == "native-negative-controls":
        from evidence_native_controls import run_controls
        build = Path(config["builds"]["debug"]).resolve()
        inputs.extend(str((build / p).relative_to(root)) for p in ("compile_commands.json", "build.ninja"))
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
            inputs.append(str(cache.relative_to(root)))
            compiler = next(line.split("=", 1)[1] for line in cache.read_text().splitlines() if line.startswith("CMAKE_CXX_COMPILER:"))
            payload.append(dict(name=name, cache=cache.read_text(), cache_sha256=digest(cache),
                                compiler_path=compiler, compiler_sha256=digest(Path(compiler)), compiler=command([compiler, "--version"])))
    elif action == "ci":
        deadline = time.monotonic() + 1700
        while True:
            payload = json.loads(command(["gh", "run", "view", str(config["ci_run"]), "--json", "status,conclusion,headSha,jobs,url"]))
            verify_git_source(root, root, payload["headSha"])
            verify_git_validation(root, root, payload["headSha"])
            if payload["headSha"] != config["validated_ref"]:
                raise ValueError("CI head does not match validated source commit")
            if payload["status"] == "completed":
                if payload["conclusion"] != "success" or any(j["conclusion"] != "success" for j in payload["jobs"]):
                    raise ValueError("Required CI did not succeed: " + repr(payload))
                break
            if time.monotonic() >= deadline:
                raise TimeoutError("CI did not complete")
            time.sleep(10)
    else:
        raise ValueError("Unknown producer: " + action)
    save(action, payload)


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=sorted(set().union(*(r.keys() for r in rows))), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    replay = sub.add_parser("prepare")
    replay.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    replay.add_argument("--folder", type=Path, default=Path("build/validation-provenance-followup"))
    replay.add_argument("--baseline")
    replay.add_argument("--check-python", default=sys.executable)
    replay.add_argument("--python39")
    replay.add_argument("--template", type=Path)
    replay.add_argument("--analysis-python", default=sys.executable)
    replay.add_argument("--ci-run")
    replay.add_argument("--clang-tidy")
    replay.add_argument("--cppcheck")
    producer = sub.add_parser("produce")
    producer.add_argument("config", type=Path)
    producer.add_argument("kind", choices=("archive", "analysis", "analysis-baseline", "analysis-changed", "measure", "negative-controls", "native-negative-controls", "cross-placeholder", "build-info", "ci", "history"))
    producer.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.action == "prepare":
        print(prepare(args.root, args.folder, args.baseline, args.check_python, args.python39, args.analysis_python, args.ci_run, args.clang_tidy, args.cppcheck, args.template))
    else:
        return produce(args.config, args.kind, output=args.output)


if __name__ == "__main__":
    raise SystemExit(main())
