"""Resolve portable replay settings and produce verifiable follow-up evidence."""

import argparse
import csv
import json
import platform
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

from evidence_analysis import compare_analysis
from evidence_benchmarks import executable_provenance, verify_git_source, verify_samples
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


def prepare(root, folder, baseline, check_python, python39, analysis_python, ci_run=None):
    root = root.resolve()
    folder = (root / folder).resolve()
    check_python = shutil.which(check_python) or check_python
    folder.mkdir(parents=True, exist_ok=True)
    baseline = command(["git", "-C", root, "rev-parse", baseline + "^{commit}"])
    template = json.loads((Path(__file__).resolve().parents[1] / "checks/validation-replay.json").read_text())
    config_path = folder / "config.json"
    config = dict(root=str(root), folder=str(folder), baseline=baseline,
                  validated_ref=command(["git", "-C", root, "rev-parse", "HEAD"]), ci_run=ci_run,
                  check_python=str(Path(check_python).resolve()), python39=python39, analysis_python=analysis_python,
                  cases=CASES, sets=template["benchmark"]["sets"], measured=template["benchmark"]["measured_per_set"])
    atomic_json(config_path, config)
    python = str(Path(check_python).resolve())
    checks = []
    def add(name, argv, requires=(), policy="required", timeout=600, artifacts=(), **extra):
        checks.append(dict(id=name, argv=[str(a) for a in argv], requires=list(requires), policy=policy,
                           timeout_seconds=timeout, artifacts=list(artifacts), **extra))
    def producer(name, action, requires=(), inputs=(), timeout=600):
        names = [str(config_path.relative_to(root))] + list(inputs)
        add(name, [python, "scripts/evidence_campaign.py", "produce", config_path, action], requires,
            timeout=timeout, artifacts=[dict(path=str((folder / (action + ".json")).relative_to(root)),
                                           kind=action, inputs=names)])
    def configure(source, build, kind):
        argv = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + kind,
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCODET_DEPENDENCY_ROOT=" + str(root / ".deps")]
        if sys.platform == "darwin":
            argv += ["-DCMAKE_C_COMPILER=/usr/bin/cc", "-DCMAKE_CXX_COMPILER=/usr/bin/c++",
                     "-DCMAKE_OSX_ARCHITECTURES=" + platform.machine(),
                     "-DCMAKE_CXX_FLAGS=-Wno-error=implicit-int-float-conversion -Wno-error=unused-result"]
        return argv
    add("debug-configure", configure(root, root / "build/host", "Debug"))
    add("debug-build", ["cmake", "--build", "build/host", "-j4"], ["debug-configure"])
    add("debug-ctest", ["ctest", "--test-dir", "build/host", "--output-on-failure", "-j2"], ["debug-build"], timeout=900)
    add("asan-configure", configure(root, root / "build/sanitize", "Debug") + ["-DCODET_SANITIZERS=ON"])
    add("asan-build", ["cmake", "--build", "build/sanitize", "-j4"], ["asan-configure"])
    add("asan-ctest", ["ctest", "--test-dir", "build/sanitize", "--output-on-failure", "-j2"], ["asan-build"], timeout=900)
    focused = "--gtest_filter=NativeRecordLifecycle.*:DiagnosticFixture.*:DiagnosticFallbackMetadata.*:DiagnosticReservations.*:LinePrompt.*:DiagnosticWriter.*:TerminalResources.*"
    add("focused-20", ["build/host/codet_tests", focused, "--gtest_repeat=20"], ["debug-build"])
    tsan = root / template["builds"]["tsan"]
    add("tsan-configure", configure(root, tsan, "Debug") + ["-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer -Wno-error=implicit-int-float-conversion -Wno-error=unused-result", "-DCMAKE_C_FLAGS=-fsanitize=thread", "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"])
    add("tsan-build", ["cmake", "--build", tsan, "--target", "codet_tests", "-j4"], ["tsan-configure"])
    add("tsan-20", [tsan / "codet_tests", focused, "--gtest_repeat=20"], ["tsan-build"])
    add("python39", [python39, "-m", "unittest", "discover", "-s", "tests", "-p", "test_validation_evidence.py", "-v"], python_version=[3, 9])
    add("python39-integration", [python39, "tests/hermetic.py", python39, "tests/cli_integration.py", "build/host/codet"], ["debug-build"], python_version=[3, 9])
    add("lint", ["make", "check-lint", "CHECK_PYTHON=" + python])
    add("semgrep", ["make", "check-semgrep", "CHECK_PYTHON=" + python])
    add("runner-tests", ["make", "test-lint-runners", "test-modern-analysis-runner", "CHECK_PYTHON=" + python, "ANALYSIS_PYTHON=" + analysis_python])
    cross = root / template["builds"]["cross"]
    # Existing cross-selection configuration is a host portability check, not native Tiger evidence.
    add("cross-build", ["cmake", "--build", cross, "--target", "codet_tests", "-j4"])
    add("cross-tests", [cross / "codet_tests", "--gtest_filter=LinePrompt.*:InteractionWarnings.*:CursesInput.*"], ["cross-build"])
    producer("baseline-source", "archive")
    for variant in ("baseline", "changed"):
        source = folder / "baseline-src" if variant == "baseline" else root
        build = folder / (variant + "-release")
        add(variant + "-configure", configure(source, build, "Release"), ["baseline-source"] if variant == "baseline" else [])
        add(variant + "-build", ["cmake", "--build", build, "--target", "codet_line_input_bench", "codet_output_bench", "codet_export_bench", "-j4"], [variant + "-configure"])
        add(variant + "-selftest", [analysis_python, source / "scripts/analyze-modern.py", "--build-dir", build, "--selftest", "--jobs", "4"], [variant + "-configure"])
        add(variant + "-analysis", [analysis_python, source / "scripts/analyze-modern.py", "--build-dir", build, "--tool", "all", "--jobs", "4"], [variant + "-selftest"], policy="observation", timeout=1800)
    analysis_inputs = [str((folder / (v + "-release") / "analysis" / t / "manifest.json").relative_to(root))
                       for v in ("baseline", "changed") for t in TOOLS]
    analysis_inputs += [str((folder / (v + "-release") / "analysis/selftest/selftest.json").relative_to(root)) for v in ("baseline", "changed")]
    producer("static-comparison", "analysis", ["baseline-analysis", "changed-analysis"], analysis_inputs)
    producer("negative-controls", "negative-controls", ["runner-tests"], timeout=600)
    producer("benchmarks", "measure", ["baseline-build", "changed-build", "static-comparison", "negative-controls"], timeout=1800)
    producer("build-metadata", "build-info", ["debug-build", "asan-build", "cross-build", "baseline-build", "changed-build"])
    if ci_run:
        producer("ubuntu", "ci", timeout=1800)
    atomic_json(folder / "manifest.json", dict(root=str(root), baseline=baseline, checks=checks))
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
    root, folder = Path(config["root"]), Path(config["folder"])
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
                values[key] = value
        configurations[variant] = dict(values=values, cache_sha256=digest(cache), compiler=command([values.get("CMAKE_CXX_COMPILER:FILEPATH", "c++"), "--version"]))
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


def produce(config_path, action):
    config = json.loads(config_path.read_text())
    root, folder = Path(config["root"]), Path(config["folder"])
    inputs = [str(config_path.relative_to(root))]
    if action == "archive":
        source = folder / "baseline-src"
        if source.exists():
            raise ValueError("Baseline archive destination already exists")
        source.mkdir()
        archive = folder / "baseline.tar"
        command(["git", "archive", "--format=tar", "--output=" + str(archive), config["baseline"]])
        command(["tar", "-xf", archive, "-C", source])
        (source / ".deps").symlink_to(root / ".deps", target_is_directory=True)
        payload = verify_git_source(root, source, config["baseline"])
    elif action == "analysis":
        expected = [str(p.relative_to(root)) for p in application_sources(root)]
        payload = []
        for variant in ("baseline", "changed"):
            probe = folder / (variant + "-release") / "analysis/selftest/selftest.json"
            inputs.append(str(probe.relative_to(root)))
            if json.loads(probe.read_text()) != {t: True for t in TOOLS}:
                raise ValueError("Analyzer self-check did not pass")
        for tool in TOOLS:
            paths = [folder / (v + "-release") / "analysis" / tool / "manifest.json" for v in ("baseline", "changed")]
            inputs.extend(str(p.relative_to(root)) for p in paths)
            value = compare_analysis(*(json.loads(p.read_text()) for p in paths), folder / "baseline-src", root, expected)
            if not value["equal"]:
                raise ValueError("Analyzer findings differ: " + repr(value))
            payload.append(value)
    elif action == "measure":
        payload = measure(config)
    elif action == "negative-controls":
        from evidence_negative_controls import run_controls
        payload = run_controls(root, folder)
    elif action == "build-info":
        payload = []
        for name, build in [("debug", root / "build/host"), ("asan", root / "build/sanitize")]+[(v, folder / (v + "-release")) for v in ("baseline", "changed")]:
            cache = build / "CMakeCache.txt"
            payload.append(dict(name=name, cache=cache.read_text(), cache_sha256=digest(cache),
                                compiler=command(["/usr/bin/c++" if sys.platform == "darwin" else "c++", "--version"])))
    elif action == "ci":
        deadline = time.monotonic() + 1700
        while True:
            payload = json.loads(command(["gh", "run", "view", str(config["ci_run"]), "--json", "status,conclusion,headSha,jobs,url"]))
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
    write_artifact(folder / (action + ".json"), action, payload, inputs)


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
    replay.add_argument("--baseline", default="origin/main")
    replay.add_argument("--check-python", default=sys.executable)
    replay.add_argument("--python39", default="python3.9")
    replay.add_argument("--analysis-python", default=sys.executable)
    replay.add_argument("--ci-run")
    producer = sub.add_parser("produce")
    producer.add_argument("config", type=Path)
    producer.add_argument("kind", choices=("archive", "analysis", "measure", "negative-controls", "build-info", "ci"))
    args = parser.parse_args()
    if args.action == "prepare":
        print(prepare(args.root, args.folder, args.baseline, args.check_python, args.python39, args.analysis_python, args.ci_run))
    else:
        produce(args.config, args.kind)


if __name__ == "__main__":
    main()
