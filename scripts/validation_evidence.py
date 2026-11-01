"""Capture command outcomes and publish source-bound validation evidence."""

import argparse
import hashlib
import json
import os
import signal
import subprocess
import tempfile
import time
import uuid
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def production_hashes(root):
    paths = [root / "CMakeLists.txt"]
    for directory in ("include", "src", "cmake"):
        paths.extend(p for p in (root / directory).rglob("*") if p.is_file())
    return {str(p.relative_to(root)): digest(p) for p in sorted(paths) if p.exists()}


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(value, stream, indent=2)
            stream.write("\n")
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def load_manifest(path):
    value = json.loads(path.read_text())
    checks = value["checks"]
    if not checks:
        raise ValueError("At least one expected check is required")
    ids = set()
    for check in checks:
        name = check["id"]
        if not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name):
            raise ValueError("Invalid check ID: " + name)
        if name in ids or any(parent not in ids for parent in check.get("requires", [])):
            raise ValueError("Duplicate check or unordered dependency: " + name)
        ids.add(name)
        if not check["argv"] or not all(isinstance(s, str) for s in check["argv"]):
            raise ValueError("Command must be a nonempty argument vector")
        if check.get("policy", "required") not in ("required", "observation"):
            raise ValueError("Unknown outcome policy")
    return value


def capture(manifest_path, folder):
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    signature = digest(manifest_path)
    run = dict(id=uuid.uuid4().hex, manifest_sha256=signature)
    atomic_json(folder / "run.json", run)
    results = {}
    for check in manifest["checks"]:
        name = check["id"]
        log = folder / (name + ".log")
        before = production_hashes(root)
        record = dict(id=name, run_id=run["id"], argv=check["argv"], cwd=str(Path(check.get("cwd", root)).resolve()),
                      manifest_sha256=signature, source_before=before,
                      started_ns=time.time_ns(), exit=None, outcome="not-run", log=log.name)
        try:
            if any(results[parent]["outcome"] != "passed" for parent in check.get("requires", [])):
                record["outcome"] = "blocked"
                log.write_text("Prerequisite did not pass; command was not launched.\n")
            else:
                with log.open("wb") as stream:
                    process = subprocess.Popen(check["argv"], cwd=record["cwd"], stdout=stream,
                                               stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        record["exit"] = process.wait(timeout=check.get("timeout_seconds", 600))
                        record["outcome"] = "passed" if record["exit"] == 0 else "failed"
                    except (subprocess.TimeoutExpired, KeyboardInterrupt) as error:
                        try:
                            os.killpg(process.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                        record["exit"] = process.wait()
                        record["outcome"] = "timeout" if isinstance(error, subprocess.TimeoutExpired) else "interrupted"
                        if isinstance(error, KeyboardInterrupt):
                            raise
        except OSError as error:
            record["outcome"] = "launch-error"
            record["error"] = str(error)
            with log.open("a") as stream:
                stream.write(str(error) + "\n")
        finally:
            record["source_after"] = production_hashes(root)
            record["finished_ns"] = time.time_ns()
            record["log_sha256"] = digest(log)
            results[name] = record
            atomic_json(folder / (name + ".json"), record)
    return results


def publish(manifest_path, folder, output, benchmark=None):
    manifest = load_manifest(manifest_path)
    current = production_hashes(Path(manifest["root"]).resolve())
    signature = digest(manifest_path)
    run = json.loads((folder / "run.json").read_text())
    if run["manifest_sha256"] != signature:
        raise ValueError("Capture manifest has changed")
    results = []
    for check in manifest["checks"]:
        record = json.loads((folder / (check["id"] + ".json")).read_text())
        if (record["id"] != check["id"] or record["run_id"] != run["id"] or
                record["argv"] != check["argv"] or
                record["cwd"] != str(Path(check.get("cwd", manifest["root"])).resolve()) or
                record["manifest_sha256"] != signature or
                record["source_before"] != current or record["source_after"] != current or
                record["log_sha256"] != digest(folder / record["log"])):
            raise ValueError("Stale or mismatched evidence: " + check["id"])
        if record["outcome"] not in ("passed", "failed") or type(record["exit"]) is not int:
            raise ValueError("Command did not complete: " + check["id"])
        if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):
            raise ValueError("Required check did not pass: " + check["id"])
        record["policy"] = check.get("policy", "required")
        record["log_tail"] = "\n".join((folder / record["log"]).read_text(errors="replace").splitlines()[-9:])
        results.append(record)
    value = dict(production_hashes=current, validation=results)
    if benchmark is not None:
        # Preserve provenance; comparisons are calculated rather than copied from its claims.
        expected = benchmark["production_hashes"]
        equivalent = expected == current
        value["source_equivalence"] = dict(production_files_unchanged_since_correctness_benchmark=equivalent,
                                           benchmark=benchmark)
        value["measurements_apply_to_current_source"] = equivalent
    atomic_json(output, value)
    return value


def blocking_processes(processes):
    found = []
    for process in processes:
        executable = Path(process["executable"]).name
        argv = process["argv"]
        if executable in ("ctest", "codet_tests", "clang-tidy", "cppcheck", "ninja"):
            found.append(process)
        elif executable.lower().startswith("python"):
            arguments = list(argv[1:])
            while arguments and arguments[0] in ("-u", "-B", "-E", "-I"):
                arguments.pop(0)
            if arguments and Path(arguments[0]).name == "analyze-modern.py":
                found.append(process)
    return found


def require_quiet_processes():
    # Separate fields avoid interpreting a shell's -c argument as an executable.
    identities = subprocess.check_output(["ps", "-axo", "pid=,comm="], text=True)
    arguments = subprocess.check_output(["ps", "-axo", "pid=,args="], text=True)
    by_pid = {}
    for line in arguments.splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) == 2:
            by_pid[fields[0]] = fields[1].split()
    processes = []
    for line in identities.splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) == 2 and fields[0] in by_pid:
            processes.append(dict(pid=fields[0], executable=fields[1], argv=by_pid[fields[0]]))
    if not processes:
        raise ValueError("Process inspection returned no usable records")
    blocked = blocking_processes(processes)
    if blocked:
        raise ValueError("Concurrent validation/build/analyzer processes: " + repr(blocked))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("capture", "publish"))
    parser.add_argument("manifest", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--benchmark", type=Path)
    args = parser.parse_args()
    if args.operation == "capture":
        results = capture(args.manifest, args.folder)
        policies = {c["id"]: c.get("policy", "required") for c in load_manifest(args.manifest)["checks"]}
        return int(any(r["outcome"] != "passed" and policies[name] == "required"
                       for name, r in results.items()))
    if args.output is None:
        parser.error("publish requires --output")
    publish(args.manifest, args.folder, args.output,
            json.loads(args.benchmark.read_text()) if args.benchmark else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
