"""Capture actual command outcomes and publish versioned, source-bound evidence."""

import argparse
import json
import os
import signal
import subprocess
import tempfile
import time
import uuid
from pathlib import Path

from evidence_inputs import (
    directory_hashes,
    environment,
    executable,
    file_hashes,
    file_specs,
    output_hashes,
    role_hashes,
    sha256,
    source_inputs,
    verify_tools,
    working_directory,
)
from evidence_processes import blocking_processes, require_quiet_processes  # noqa: F401

SCHEMA_VERSION = 4


def digest(path):
    return sha256(path)


def production_hashes(root):
    return role_hashes(root, "production")


def validation_hashes(root):
    return role_hashes(root, "validation")


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
    if not isinstance(value.get("root"), str) or not value["root"]:
        raise ValueError("Manifest root must be a path")
    for env in [value.get("environment", {})] + [c.get("env", {}) for c in checks]:
        if not isinstance(env, dict) or any(not isinstance(k, str) or not isinstance(v, str) for k, v in env.items()):
            raise ValueError("Environment must contain explicit string settings")
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
        if not isinstance(check["argv"], list) or not check["argv"] or not all(isinstance(s, str) for s in check["argv"]):
            raise ValueError("Command must be a nonempty argument vector")
        if not check["argv"][0] or not isinstance(check.get("cwd", "."), str):
            raise ValueError("Invalid command executable or working directory")
        for field in ("source_inputs", "outputs", "artifacts"):
            specs = check.get(field, [])
            if not isinstance(specs, list) or any(not isinstance(s, dict) or not isinstance(s.get("path"), str) or not s["path"] for s in specs):
                raise ValueError("Invalid " + field + " declaration: " + name)
            if len({s["path"] for s in specs}) != len(specs):
                raise ValueError("Duplicate " + field + " declaration: " + name)
        if check.get("policy", "required") not in ("required", "observation"):
            raise ValueError("Unknown outcome policy")
    return value


def input_hashes(root, names):
    return file_hashes(root, names)


def write_artifact(path, kind, payload, inputs=()):
    """Producer entry point: the capture supplies its identity, never the producer."""
    inputs = tuple(inputs)
    context = json.loads(os.environ["CODET_EVIDENCE_CONTEXT"])
    root = Path(context.pop("root"))
    declared = context.pop("declared_artifacts", None)
    if declared is not None and not any((root / a["path"]).resolve() == path.resolve() and a["kind"] == kind and
                                        set(a.get("inputs", [])) == set(inputs) for a in declared):
        raise ValueError("Captured artifact is not declared by its manifest: " + str(path))
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


def _python_identity(argv, required, env=None):
    value = json.loads(subprocess.check_output(
        [argv[0], "-c", "import json,sys;print(json.dumps(dict(executable=sys.executable,version=list(sys.version_info[:3]))))"],
        text=True, timeout=5, env=env))
    if value["version"][:2] != required:
        raise ValueError("Required Python " + ".".join(map(str, required)) + "; found " + repr(value))
    return value


def capture(manifest_path, folder, *, campaign_id=None):
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    verify_tools(manifest)
    if manifest.get("single_attempt") and (folder / "run.json").exists():
        raise ValueError("Managed campaign capture already attempted; use a fresh campaign")
    folder.mkdir(parents=True, exist_ok=True)
    signature = digest(manifest_path)
    run = dict(schema_version=SCHEMA_VERSION, id=uuid.uuid4().hex,
               campaign_id=campaign_id or uuid.uuid4().hex, manifest_sha256=signature, canonical_root=str(root))
    atomic_json(folder / "run.json", run)
    results = {}
    policies = {c["id"]: c.get("policy", "required") for c in manifest["checks"]}
    for check in manifest["checks"]:
        name = check["id"]
        log = folder / (name + ".log")
        before, validation = production_hashes(root), validation_hashes(root)
        context = _context(root, run, name, before, validation)
        context["declared_artifacts"] = check.get("artifacts", [])
        record = dict(schema_version=SCHEMA_VERSION, id=name, run_id=run["id"], campaign_id=run["campaign_id"],
                      argv=check["argv"], env=check.get("env", {}), cwd=working_directory(manifest, check),
                      manifest_sha256=signature, source_before=before, validation_before=validation,
                      started_ns=time.time_ns(), exit=None, outcome="not-run", log=log.name, artifacts=[], outputs=[])
        record["canonical_cwd"] = record["cwd"]
        record["cwd_spec"] = check.get("cwd", ".")
        log.write_text("")
        phase = "artifact"
        try:
            if any(results[parent]["outcome"] != "passed" and not (policies[parent] == "observation" and
                   results[parent]["outcome"] == "failed" and type(results[parent]["exit"]) is int)
                   for parent in check.get("requires", [])):
                record["outcome"] = "blocked"
                log.write_text("Prerequisite did not pass; command was not launched.\n")
            else:
                for parent in check.get("requires", []):
                    for output in results[parent]["outputs"]:
                        if not (root / output["path"]).is_file() or digest(root / output["path"]) != output["sha256"]:
                            raise ValueError("Prerequisite output changed: " + parent + ": " + output["path"])
                record["input_before"] = input_hashes(root, file_specs(check))
                record["build_inputs_before"] = source_inputs(root, check.get("source_inputs", [{"path": "."}]))
                record["external_inputs_before"] = directory_hashes(root, check.get("directory_inputs", []))
                env = environment(manifest, check)
                env["CODET_EVIDENCE_CONTEXT"] = json.dumps(context)
                record["effective_environment"] = dict(env)
                phase = "launch"
                try:
                    record["executable"] = executable(check["argv"], env, record["cwd"])
                except FileNotFoundError as error:
                    if "python_version" not in check:
                        raise
                    record["outcome"] = "unavailable"
                    log.write_text(str(error) + "\n")
                    continue
                record["resolved_executable"] = record["executable"]["path"]
                argv = [record["resolved_executable"]] + check["argv"][1:]
                record["executed_argv"] = argv
                if "python_version" in check:
                    try:
                        record["interpreter"] = _python_identity(argv, check["python_version"], env)
                    except (ValueError, OSError, subprocess.SubprocessError) as error:
                        record["outcome"] = "unavailable"
                        log.write_text(str(error) + "\n")
                        continue
                with log.open("wb") as stream:
                    env["CODET_EVIDENCE_CONTEXT"] = json.dumps(context)
                    process = subprocess.Popen(argv, cwd=record["cwd"], stdout=stream,
                                               stderr=subprocess.STDOUT, start_new_session=True, env=env)
                    phase = "execution"
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
                phase = "artifact"
                if record["outcome"] == "passed" or (record["outcome"] == "failed" and check.get("policy") == "observation"):
                    record["outputs"] = output_hashes(root, check.get("outputs", []))
                    for spec in check.get("artifacts", []):
                        path = root / spec["path"]
                        if not path.is_file():
                            raise ValueError("Artifact not produced: " + spec["path"])
                        validate_artifact(path, spec, context)
                        record["artifacts"].append(dict(path=spec["path"], sha256=digest(path)))
        except OSError as error:
            record["outcome"] = "launch-error" if phase == "launch" else ("execution-error" if phase == "execution" else "artifact-error")
            record["error"] = str(error)
            with log.open("a") as stream:
                stream.write(str(error) + "\n")
        except (ValueError, KeyError, subprocess.SubprocessError) as error:
            record["outcome"] = "artifact-error"
            record["error"] = str(error)
            with log.open("a") as stream:
                stream.write(str(error) + "\n")
        finally:
            record["input_after"] = {n: digest(root / n) if (root / n).is_file() else None
                                     for n in record.get("input_before", {})}
            record["source_after"] = production_hashes(root)
            record["validation_after"] = validation_hashes(root)
            if "build_inputs_before" in record:
                try:
                    record["build_inputs_after"] = source_inputs(root, check.get("source_inputs", [{"path": "."}]))
                    record["external_inputs_after"] = directory_hashes(root, check.get("directory_inputs", []))
                except (ValueError, OSError, subprocess.SubprocessError) as error:
                    record["outcome"] = "artifact-error"
                    record["error"] = str(error)
            record["finished_ns"] = time.time_ns()
            record["log_sha256"] = digest(log)
            results[name] = record
            atomic_json(folder / (name + ".json"), record)
    return results


def validate_capture(manifest_path, folder):
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    verify_tools(manifest)
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
                record["cwd"] != working_directory(manifest, check) or
                record["manifest_sha256"] != signature or
                record["source_before"] != current or record["source_after"] != current or
                record.get("validation_before") != validation or record.get("validation_after") != validation or
                record["log_sha256"] != digest(folder / record["log"])):
            raise ValueError("Stale or mismatched evidence: " + check["id"])
        if record["outcome"] not in ("passed", "failed") or type(record["exit"]) is not int:
            raise ValueError("Command did not complete: " + check["id"] + " (" + record["outcome"] + ")")
        if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):
            raise ValueError("Required check did not pass: " + check["id"])
        names = file_specs(check)
        if record.get("input_before") != input_hashes(root, names) or record.get("input_after") != record["input_before"]:
            raise ValueError("Artifact producer inputs changed: " + check["id"])
        if (record.get("build_inputs_before") != source_inputs(root, check.get("source_inputs", [{"path": "."}])) or
                record.get("build_inputs_after") != record["build_inputs_before"]):
            raise ValueError("Build source inputs changed: " + check["id"])
        env = environment(manifest, check)
        context = _context(root, run, check["id"], current, validation)
        context["declared_artifacts"] = check.get("artifacts", [])
        env["CODET_EVIDENCE_CONTEXT"] = json.dumps(context)
        if record.get("effective_environment") != env:
            raise ValueError("Effective check environment changed: " + check["id"])
        if record.get("executable") != executable(check["argv"], env, record["cwd"]):
            raise ValueError("Check executable changed: " + check["id"])
        if record.get("executed_argv") != [record["executable"]["path"]] + check["argv"][1:]:
            raise ValueError("Executed argument vector mismatch: " + check["id"])
        if record.get("outputs") != output_hashes(root, check.get("outputs", [])):
            raise ValueError("Produced outputs changed: " + check["id"])
        if (record.get("external_inputs_before") != directory_hashes(root, check.get("directory_inputs", [])) or
                record.get("external_inputs_after") != record["external_inputs_before"]):
            raise ValueError("External build inputs changed: " + check["id"])
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
    if path.exists() and any(p.name == "validation" and p.parent.name == "docs" for p in path.resolve().parents):
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
    from evidence_bundle import publish_audit_bundle
    return publish_audit_bundle(manifest_path, folder, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("capture", "publish", "bundle", "verify-bundle"))
    parser.add_argument("manifest", type=Path)
    parser.add_argument("folder", type=Path, nargs="?")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--benchmark", type=Path)
    args = parser.parse_args()
    if args.operation == "verify-bundle":
        from evidence_bundle import verify_bundle
        result = verify_bundle(args.manifest)
        print(json.dumps(result, indent=2))
        return 0 if result.get("verified") and result.get("eligible_current") else 1
    if args.folder is None:
        parser.error("capture/publish/bundle requires a folder")
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
        # A hard link installs the completed temporary report without a clobber race.
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=args.output.parent) as temporary:
            staged = Path(temporary) / "report.json"
            _protect_archives(args.output)
            publish(args.manifest, args.folder, staged,
                    json.loads(args.benchmark.read_text()) if args.benchmark else None)
            os.link(staged, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
