"""Capture actual command outcomes and publish versioned, source-bound evidence."""

import argparse
import csv
import hashlib
import json
import os
import shutil
import signal
import subprocess
import tempfile
import time
import uuid
from pathlib import Path

from evidence_processes import blocking_processes, require_quiet_processes  # noqa: F401

SCHEMA_VERSION = 2


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def production_hashes(root):
    paths = [root / "CMakeLists.txt"]
    for directory in ("include", "src", "cmake"):
        paths.extend(p for p in (root / directory).rglob("*") if p.is_file())
    return {str(p.relative_to(root)): digest(p) for p in sorted(paths) if p.exists()}


def validation_hashes(root):
    """Validation inputs, deliberately separate from production equivalence."""
    paths = [root / n for n in ("Makefile", ".clang-format", ".clang-tidy", ".go-version")]
    paths.extend(root.glob("*lock.json"))
    for directory in ("tests", "scripts", "checks", "requirements", ".github/workflows"):
        paths.extend(p for p in (root / directory).rglob("*")
                     if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc")
    paths.extend((root / "docs/validation").glob("*.py"))
    return {str(p.relative_to(root)): digest(p) for p in sorted(set(paths)) if p.exists()}


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
        if (not isinstance(name, str) or not name or name == "run" or
                any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name)):
            raise ValueError("Invalid or reserved check ID: " + str(name))
        if name in ids or any(parent not in ids for parent in check.get("requires", [])):
            raise ValueError("Duplicate check or unordered dependency: " + name)
        artifact_names = [Path(a["path"]).name for a in check.get("artifacts", [])]
        if len(set(artifact_names)) != len(artifact_names):
            raise ValueError("Duplicate artifact basename: " + name)
        ids.add(name)
        if not check["argv"] or not all(isinstance(s, str) for s in check["argv"]):
            raise ValueError("Command must be a nonempty argument vector")
        if check.get("policy", "required") not in ("required", "observation"):
            raise ValueError("Unknown outcome policy")
    return value


def input_hashes(root, names):
    return {name: digest(root / name) for name in names}


def write_artifact(path, kind, payload, inputs=()):
    """Producer entry point: the capture supplies its identity, never the producer."""
    context = json.loads(os.environ["CODET_EVIDENCE_CONTEXT"])
    root = Path(context.pop("root"))
    atomic_json(path, dict(**context, kind=kind, input_hashes=input_hashes(root, inputs), payload=payload))


def validate_artifact(path, spec, context):
    value = json.loads(path.read_text())
    for key in ("schema_version", "campaign_id", "capture_run_id", "check_id", "production_hashes", "validation_hashes"):
        if value.get(key) != context[key]:
            raise ValueError("Stale artifact " + str(path) + ": " + key)
    if "payload" not in value:
        raise ValueError("Artifact payload missing: " + str(path))
    if value.get("kind") != spec["kind"]:
        raise ValueError("Artifact kind mismatch: " + str(path))
    if value.get("input_hashes") != input_hashes(Path(context["root"]), spec.get("inputs", [])):
        raise ValueError("Artifact input mismatch: " + str(path))
    return value


def _context(root, run, name, production, validation):
    return dict(schema_version=SCHEMA_VERSION, campaign_id=run["campaign_id"],
                capture_run_id=run["id"], check_id=name, root=str(root),
                production_hashes=production, validation_hashes=validation)


def _python_identity(argv, required):
    value = json.loads(subprocess.check_output(
        [argv[0], "-c", "import json,sys;print(json.dumps(dict(executable=sys.executable,version=list(sys.version_info[:3]))))"],
        text=True, timeout=5))
    if value["version"][:2] != required:
        raise ValueError("Required Python " + ".".join(map(str, required)) + "; found " + repr(value))
    return value


def capture(manifest_path, folder, *, campaign_id=None):
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    signature = digest(manifest_path)
    run = dict(schema_version=SCHEMA_VERSION, id=uuid.uuid4().hex,
               campaign_id=campaign_id or uuid.uuid4().hex, manifest_sha256=signature)
    atomic_json(folder / "run.json", run)
    results = {}
    policies = {c["id"]: c.get("policy", "required") for c in manifest["checks"]}
    for check in manifest["checks"]:
        name = check["id"]
        log = folder / (name + ".log")
        before, validation = production_hashes(root), validation_hashes(root)
        context = _context(root, run, name, before, validation)
        record = dict(schema_version=SCHEMA_VERSION, id=name, run_id=run["id"], campaign_id=run["campaign_id"],
                      argv=check["argv"], env=check.get("env", {}), cwd=str(Path(check.get("cwd", root)).resolve()),
                      manifest_sha256=signature, source_before=before, validation_before=validation,
                      started_ns=time.time_ns(), exit=None, outcome="not-run", log=log.name, artifacts=[])
        log.write_text("")
        try:
            names = sorted({n for a in check.get("artifacts", []) for n in a.get("inputs", [])})
            record["input_before"] = input_hashes(root, names)
            if any(results[parent]["outcome"] != "passed" and not (policies[parent] == "observation" and
                   results[parent]["outcome"] == "failed" and type(results[parent]["exit"]) is int)
                   for parent in check.get("requires", [])):
                record["outcome"] = "blocked"
                log.write_text("Prerequisite did not pass; command was not launched.\n")
            else:
                record["resolved_executable"] = shutil.which(check["argv"][0])
                if "python_version" in check:
                    try:
                        record["interpreter"] = _python_identity(check["argv"], check["python_version"])
                    except (ValueError, OSError, subprocess.SubprocessError) as error:
                        record["outcome"] = "unavailable"
                        log.write_text(str(error) + "\n")
                        continue
                with log.open("wb") as stream:
                    env = dict(os.environ)
                    env.update(check.get("env", {}))
                    env["CODET_EVIDENCE_CONTEXT"] = json.dumps(context)
                    process = subprocess.Popen(check["argv"], cwd=record["cwd"], stdout=stream,
                                               stderr=subprocess.STDOUT, start_new_session=True, env=env)
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
                if record["outcome"] == "passed" or (record["outcome"] == "failed" and check.get("policy") == "observation"):
                    for spec in check.get("artifacts", []):
                        path = root / spec["path"]
                        validate_artifact(path, spec, context)
                        record["artifacts"].append(dict(path=spec["path"], sha256=digest(path)))
        except OSError as error:
            record["outcome"] = "launch-error"
            record["error"] = str(error)
            with log.open("a") as stream:
                stream.write(str(error) + "\n")
        except (ValueError, KeyError) as error:
            record["outcome"] = "artifact-error"
            record["error"] = str(error)
            with log.open("a") as stream:
                stream.write(str(error) + "\n")
        finally:
            record["input_after"] = {n: digest(root / n) if (root / n).is_file() else None
                                     for n in record.get("input_before", {})}
            record["source_after"] = production_hashes(root)
            record["validation_after"] = validation_hashes(root)
            record["finished_ns"] = time.time_ns()
            record["log_sha256"] = digest(log)
            results[name] = record
            atomic_json(folder / (name + ".json"), record)
    return results


def validate_capture(manifest_path, folder):
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    current, validation = production_hashes(root), validation_hashes(root)
    signature = digest(manifest_path)
    run = json.loads((folder / "run.json").read_text())
    if run.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("Historical capture lacks current provenance; recapture required")
    if run["manifest_sha256"] != signature:
        raise ValueError("Capture manifest has changed")
    results, artifacts = [], []
    for check in manifest["checks"]:
        record = json.loads((folder / (check["id"] + ".json")).read_text())
        if (record.get("schema_version") != SCHEMA_VERSION or record["id"] != check["id"] or
                record["run_id"] != run["id"] or record["campaign_id"] != run["campaign_id"] or
                record["argv"] != check["argv"] or record.get("env") != check.get("env", {}) or
                record["cwd"] != str(Path(check.get("cwd", root)).resolve()) or
                record["manifest_sha256"] != signature or
                record["source_before"] != current or record["source_after"] != current or
                record.get("validation_before") != validation or record.get("validation_after") != validation or
                record["log_sha256"] != digest(folder / record["log"])):
            raise ValueError("Stale or mismatched evidence: " + check["id"])
        if record["outcome"] not in ("passed", "failed") or type(record["exit"]) is not int:
            raise ValueError("Command did not complete: " + check["id"] + " (" + record["outcome"] + ")")
        if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):
            raise ValueError("Required check did not pass: " + check["id"])
        names = sorted({n for a in check.get("artifacts", []) for n in a.get("inputs", [])})
        if record.get("input_before") != input_hashes(root, names) or record.get("input_after") != record["input_before"]:
            raise ValueError("Artifact producer inputs changed: " + check["id"])
        expected = check.get("artifacts", [])
        if len(record["artifacts"]) != len(expected):
            raise ValueError("Missing produced artifacts: " + check["id"])
        for index, spec in enumerate(expected):
            saved = record["artifacts"][index]
            if saved != dict(path=spec["path"], sha256=digest(root / spec["path"])):
                raise ValueError("Artifact digest mismatch: " + check["id"])
            artifacts.append(validate_artifact(root / spec["path"], spec,
                             _context(root, run, check["id"], current, validation)))
        record["policy"] = check.get("policy", "required")
        record["log_tail"] = "\n".join((folder / record["log"]).read_text(errors="replace").splitlines()[-9:])
        results.append(record)
    return dict(schema_version=SCHEMA_VERSION, campaign_id=run["campaign_id"], capture_run_id=run["id"],
                production_hashes=current, validation_hashes=validation, validation=results,
                derived_artifacts=artifacts)


def _protect_archives(path):
    if path.name in ("diagnostic-record-lifecycle-followup.json", "prompt-recovery-evidence-followup.json"):
        raise ValueError("Historical evidence must remain unchanged; select a new output")


def publish(manifest_path, folder, output, benchmark=None):
    _protect_archives(output)
    value = validate_capture(manifest_path, folder)
    if benchmark is not None:
        equivalent = benchmark["production_hashes"] == value["production_hashes"]
        value["source_equivalence"] = dict(production_files_unchanged_since_correctness_benchmark=equivalent,
                                           benchmark=benchmark)
        linked = benchmark in value["derived_artifacts"] and benchmark.get("kind") == "measure"
        value["measurements_apply_to_current_source"] = equivalent and linked
        value["source_equivalence"]["historical_unlinked_reference"] = not linked
    atomic_json(output, value)
    return value


def publish_bundle(manifest_path, folder, destination):
    """Stage immutable sidecars and report, then expose the complete directory."""
    if destination.exists():
        raise ValueError("Bundle destination already exists")
    value = validate_capture(manifest_path, folder)
    root = Path(load_manifest(manifest_path)["root"]).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".evidence-", dir=destination.parent) as temporary:
        stage = Path(temporary) / "bundle"
        stage.mkdir()
        shutil.copyfile(manifest_path, stage / "manifest.json")
        shutil.copytree(folder, stage / "capture")
        for record in value["validation"]:
            for artifact in record["artifacts"]:
                target = stage / "artifacts" / record["id"] / Path(artifact["path"]).name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(root / artifact["path"], target)
        for artifact in value["derived_artifacts"]:
            if artifact["kind"] == "measure":
                for key, name in (("rows", "samples.csv"), ("summary", "medians.csv")):
                    rows = artifact["payload"][key]
                    with (stage / name).open("w", newline="") as stream:
                        writer = csv.DictWriter(stream, fieldnames=sorted(set().union(*(r.keys() for r in rows))), lineterminator="\n")
                        writer.writeheader()
                        writer.writerows(rows)
        atomic_json(stage / "report.json", value)
        if validate_capture(stage / "manifest.json", stage / "capture") != value:
            raise ValueError("Staged capture mismatch")
        for record in value["validation"]:
            for artifact in record["artifacts"]:
                copied = stage / "artifacts" / record["id"] / Path(artifact["path"]).name
                if digest(copied) != artifact["sha256"]:
                    raise ValueError("Staged artifact digest mismatch")
        # Revalidate after copying: stale inputs or sidecars never become published evidence.
        if validate_capture(manifest_path, folder) != value:
            raise ValueError("Evidence changed during staging")
        if destination.exists():
            raise ValueError("Bundle destination already exists")
        atomic_json(stage / "inventory.json", {str(p.relative_to(stage)): digest(p)
                    for p in sorted(stage.rglob("*")) if p.is_file()})
        os.rename(stage, destination)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("capture", "publish", "bundle"))
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
        parser.error("publish/bundle requires --output")
    if args.operation == "bundle":
        publish_bundle(args.manifest, args.folder, args.output)
    else:
        publish(args.manifest, args.folder, args.output,
                json.loads(args.benchmark.read_text()) if args.benchmark else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
