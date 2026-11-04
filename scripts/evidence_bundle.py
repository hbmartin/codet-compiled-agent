"""Immutable audit bundles, independently verifiable without a checkout."""

import csv
import ctypes
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

from evidence_benchmarks import verify_samples
from evidence_inputs import environment, file_specs, role_selected, write_csv
from evidence_paths import verify_working_provenance


def safe_member(folder, name):
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError("Invalid audit lookup path: " + repr(name))
    path = Path(name)
    if path.is_absolute() or any(part in ("", ".", "..") for part in name.split("/")):
        raise ValueError("Invalid audit lookup path: " + name)
    current = folder
    for part in path.parts:
        current = current / part
        if current.is_symlink():
            raise ValueError("Audit lookup contains symlink: " + name)
    return current


def valid_digest(value):
    if not isinstance(value, str) or re.fullmatch(r"[0-9a-f]{64}", value) is None:
        raise ValueError("Invalid audit digest")
    return value


_csv = write_csv


def rename_exclusive(source, destination):
    library = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin":
        rename = getattr(library, "renamex_np", None)
        if rename is None:
            raise ValueError("Atomic immutable publication unavailable in this libc")
        rename.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        args = [os.fsencode(source), os.fsencode(destination), 4]  # SDK RENAME_EXCL
    elif sys.platform.startswith("linux"):
        rename = getattr(library, "renameat2", None)
        if rename is None:
            raise ValueError("Atomic immutable publication unavailable in this libc")
        rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        args = [-100, os.fsencode(source), -100, os.fsencode(destination), 1]  # AT_FDCWD, RENAME_NOREPLACE
    else:
        raise ValueError("Atomic immutable publication unsupported on " + sys.platform)
    rename.restype = ctypes.c_int
    if rename(*args):
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code), str(destination))


def publish_audit_bundle(manifest_path, folder, destination):
    from validation_evidence import atomic_json, digest, load_manifest, validate_capture
    if destination.exists():
        raise ValueError("Bundle destination already exists")
    value = validate_capture(manifest_path, folder)
    manifest = load_manifest(manifest_path)
    root = Path(manifest["root"]).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".evidence-", dir=destination.parent) as temporary:
        stage = Path(temporary) / "bundle"
        (stage / "capture").mkdir(parents=True)
        shutil.copyfile(manifest_path, stage / "manifest.json")
        shutil.copyfile(folder / "run.json", stage / "capture/run.json")
        index, sidecars = [], []
        def archive(check, role, name, path, expected, retained=True):
            if digest(path) != expected:
                raise ValueError("Audit input changed while staging: " + name)
            storage = "inputs/" + expected if retained else None
            if storage and not (stage / storage).exists():
                (stage / storage).parent.mkdir(exist_ok=True)
                shutil.copyfile(path, stage / storage)
            index.append(dict(check_id=check, role=role, path=name, sha256=expected, storage=storage))
        for role, hashes in (("production", value["production_hashes"]), ("validation", value["validation_hashes"])):
            for filename, content in hashes.items():
                archive("run", "fingerprint:" + role, filename, root / filename, content)
        for position, check in enumerate(manifest["checks"]):
            record = value["validation"][position]
            name = check["id"]
            shutil.copyfile(folder / (name + ".json"), stage / "capture" / (name + ".json"))
            shutil.copyfile(folder / record["log"], stage / "capture" / record["log"])
            for source, files in record["build_inputs_before"].items():
                for filename, signature in files.items():
                    archive(name, "source:" + source, filename, root / source / filename, signature)
            specs = file_specs(check)
            for filename, signature in record["input_before"].items():
                archive(name, "input", filename, root / filename, signature, specs[filename].get("archive", True))
            for output in record["outputs"]:
                archive(name, "output", output["path"], root / output["path"], output["sha256"], output["archive"])
            for position, spec in enumerate(check.get("artifacts", [])):
                artifact = record["artifacts"][position]
                target = "artifacts/" + name + "/" + Path(artifact["path"]).name
                (stage / target).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(root / artifact["path"], stage / target)
                envelope = json.loads((stage / target).read_text())
                if envelope["kind"] == "measure":
                    paths = []
                    for key, filename in (("rows", "samples.csv"), ("summary", "medians.csv")):
                        relative = "measurements/" + name + "/" + Path(artifact["path"]).name + "/" + filename
                        _csv(stage / relative, envelope["payload"][key])
                        paths.append(relative)
                    sidecars.append(dict(artifact=target, config=next(iter(spec.get("inputs", [])), None), paths=paths))
        atomic_json(stage / "inputs.json", index)
        atomic_json(stage / "measurements.json", sidecars)
        atomic_json(stage / "report.json", value)
        if validate_capture(manifest_path, folder) != value:
            raise ValueError("Evidence changed during staging")
        atomic_json(stage / "inventory.json", {str(p.relative_to(stage)): digest(p)
                    for p in sorted(stage.rglob("*")) if p.is_file()})
        verify_bundle(stage)
        if destination.exists():
            raise ValueError("Bundle destination already exists")
        rename_exclusive(stage, destination)
    return value


def verify_bundle(folder):
    """Audit recorded identities and archived material; does not rerun binaries."""
    from validation_evidence import SCHEMA_VERSION, digest, load_manifest
    inventory = json.loads(safe_member(folder, "inventory.json").read_text())
    if not isinstance(inventory, dict):
        raise ValueError("Invalid audit inventory")
    for name, value in inventory.items():
        valid_digest(value)
        safe_member(folder, name)
    if any(p.is_symlink() for p in folder.rglob("*")):
        raise ValueError("Audit bundle contains symlink")
    actual = {str(p.relative_to(folder)): digest(p) for p in sorted(folder.rglob("*"))
              if p.is_file() and p != folder / "inventory.json"}
    if actual != inventory:
        raise ValueError("Audit inventory mismatch")
    manifest = load_manifest(folder / "manifest.json")
    run = json.loads((folder / "capture/run.json").read_text())
    report = json.loads((folder / "report.json").read_text())
    if run.get("schema_version") != SCHEMA_VERSION:
        return dict(verified=False, inventory_verified=True, historical=True, eligible_current=False,
                    schema_version=run.get("schema_version"),
                    limitations=["Historical format does not establish v5 resolution chains and consumer provenance; no provenance was backfilled."])
    for key in ("id", "campaign_id"):
        if not isinstance(run.get(key), str) or re.fullmatch(r"[A-Za-z0-9_-]{1,128}", run[key]) is None:
            raise ValueError("Invalid audit run identity: " + key)
    if report.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("Audit report format mismatch")
    for role, hashes in (("production", report["production_hashes"]), ("validation", report["validation_hashes"])):
        if not isinstance(hashes, dict):
            raise ValueError("Invalid audit fingerprint map")
        for name, content in hashes.items():
            if Path(name).is_absolute() or any(part in ("", ".", "..") for part in name.split("/")) or not role_selected(name, role):
                raise ValueError("Invalid audit fingerprint path")
            valid_digest(content)
    if run["manifest_sha256"] != digest(folder / "manifest.json"):
        raise ValueError("Audit capture identity mismatch")
    if (report["capture_run_id"], report["campaign_id"]) != (run["id"], run["campaign_id"]):
        raise ValueError("Audit report identity mismatch")
    index = json.loads((folder / "inputs.json").read_text())
    entries = {}
    for item in index:
        valid_digest(item["sha256"])
        if item["check_id"] not in {"run", *(c["id"] for c in manifest["checks"])}:
            raise ValueError("Unknown audit input producer")
        key = (item["check_id"], item["role"], item["path"])
        if key in entries:
            raise ValueError("Duplicate audit input")
        if item["storage"] is not None:
            if item["storage"] != "inputs/" + item["sha256"] or digest(safe_member(folder, item["storage"])) != item["sha256"]:
                raise ValueError("Audit input digest mismatch")
        entries[key] = item
    required = {("run", "fingerprint:" + role, path): (content, True)
                for role, hashes in (("production", report["production_hashes"]), ("validation", report["validation_hashes"]))
                for path, content in hashes.items()}
    expected_measurements = set()
    derived = []
    records = {}
    if len(report["validation"]) != len(manifest["checks"]):
        raise ValueError("Audit check coverage mismatch")
    for position, check in enumerate(manifest["checks"]):
        published = report["validation"][position]
        name = check["id"]
        record = json.loads((folder / "capture" / (name + ".json")).read_text())
        verify_working_provenance(manifest, run, record)
        if (record.get("schema_version") != SCHEMA_VERSION or record["id"] != name or record["run_id"] != run["id"] or record["campaign_id"] != run["campaign_id"] or
                record["manifest_sha256"] != run["manifest_sha256"] or record["argv"] != check["argv"] or
                record["env"] != check.get("env", {}) or record["cwd"] != record.get("canonical_cwd") or record.get("cwd_spec") != check.get("cwd", ".") or
                record["source_before"] != report["production_hashes"] or record["source_after"] != record["source_before"] or
                record["validation_before"] != report["validation_hashes"] or record["validation_after"] != record["validation_before"] or
                record["build_inputs_before"] != record["build_inputs_after"] or record["input_before"] != record["input_after"] or
                record["external_inputs_before"] != record["external_inputs_after"] or
                record["log_sha256"] != digest(safe_member(folder, "capture/" + record["log"]))):
            raise ValueError("Audit check provenance mismatch: " + name)
        if any(record[k] != published[k] for k in record):
            raise ValueError("Audit report/check mismatch: " + name)
        if record["outcome"] not in ("passed", "failed") or type(record["exit"]) is not int:
            raise ValueError("Audit command did not complete: " + name)
        if check.get("policy", "required") == "required" and (record["outcome"] != "passed" or record["exit"] != 0):
            raise ValueError("Audit required check failed: " + name)
        for parent in check.get("requires", []):
            if parent not in records:
                raise ValueError("Audit prerequisite missing")
        expected_env = environment(manifest, check)
        expected_env["CODET_EVIDENCE_CONTEXT"] = json.dumps(dict(schema_version=SCHEMA_VERSION,
            campaign_id=run["campaign_id"], capture_run_id=run["id"], check_id=name,
            root=run["canonical_root"], production_hashes=report["production_hashes"], validation_hashes=report["validation_hashes"],
            declared_artifacts=check.get("artifacts", [])))
        if record.get("effective_environment") != expected_env:
            raise ValueError("Audit effective environment mismatch: " + name)
        records[name] = record
        if set(record["build_inputs_before"]) != {s["path"] for s in check.get("source_inputs", [{"path": "."}])}:
            raise ValueError("Audit source coverage mismatch")
        if set(record["input_before"]) != set(file_specs(check)):
            raise ValueError("Audit declared input coverage mismatch: " + name)
        current_sources = record["build_inputs_before"].get(".")
        if current_sources is not None:
            for label, hashes in (("production", report["production_hashes"]), ("validation", report["validation_hashes"])):
                if {path: value for path, value in current_sources.items() if role_selected(path, label)} != hashes:
                    raise ValueError("Audit archived " + label + " fingerprint mismatch: " + name)
        if set(record["external_inputs_before"]) != set(check.get("directory_inputs", [])):
            raise ValueError("Audit external input coverage mismatch")
        if [o["path"] for o in record["outputs"]] != [o["path"] for o in check.get("outputs", [])]:
            raise ValueError("Audit output coverage mismatch")
        for source, files in record["build_inputs_before"].items():
            for path, signature in files.items():
                required[(name, "source:" + source, path)] = (signature, True)
        for path, signature in record["input_before"].items():
            required[(name, "input", path)] = (signature, file_specs(check)[path].get("archive", True))
        for output in record["outputs"]:
            required[(name, "output", output["path"])] = (output["sha256"], output["archive"])
        if len(record["artifacts"]) != len(check.get("artifacts", [])):
            raise ValueError("Audit artifact coverage mismatch")
        for position, spec in enumerate(check.get("artifacts", [])):
            saved = record["artifacts"][position]
            path = safe_member(folder, "artifacts/" + name + "/" + Path(spec["path"]).name)
            if saved["path"] != spec["path"] or digest(path) != saved["sha256"]:
                raise ValueError("Audit artifact digest mismatch")
            envelope = json.loads(path.read_text())
            if envelope["kind"] == "hash-profile":
                from evidence_hash_experiment import verify_experiment
                verify_experiment(envelope["payload"])
                if not envelope["payload"].get("profiles"):
                    raise ValueError("Hash experiment profiling evidence missing")
                for filename, signature in envelope["payload"]["code_hashes"]["baseline"].items():
                    matches = [output for output in record["outputs"] if output["path"].endswith("/hash-correctness/scripts/" + filename)]
                    if len(matches) != 1 or not matches[0]["archive"] or matches[0]["sha256"] != signature:
                        raise ValueError("Hash experiment correctness code mismatch")
                for filename, signature in envelope["payload"]["code_hashes"]["changed"].items():
                    if report["validation_hashes"].get("scripts/" + filename) != signature:
                        raise ValueError("Hash experiment candidate code mismatch")
            derived.append(envelope)
            expected = dict(schema_version=SCHEMA_VERSION, campaign_id=run["campaign_id"], capture_run_id=run["id"],
                            check_id=name, production_hashes=report["production_hashes"], validation_hashes=report["validation_hashes"])
            if any(envelope.get(k) != v for k, v in expected.items()) or envelope.get("kind") != spec["kind"]:
                raise ValueError("Audit artifact producer mismatch")
            if envelope["input_hashes"] != {p: record["input_before"][p] for p in spec.get("inputs", [])}:
                raise ValueError("Audit artifact inputs mismatch")
            if envelope["kind"] == "measure":
                expected_measurements.add(str(path.relative_to(folder)))
    if set(entries) != set(required):
        raise ValueError("Audit input coverage mismatch")
    if derived != report["derived_artifacts"]:
        raise ValueError("Audit derived report mismatch")
    for key, (signature, retained) in required.items():
        if entries[key]["sha256"] != signature or bool(entries[key]["storage"]) != retained:
            raise ValueError("Audit input binding mismatch")
    measurements = json.loads((folder / "measurements.json").read_text())
    if len(measurements) != len(expected_measurements) or {m["artifact"] for m in measurements} != expected_measurements:
        raise ValueError("Audit measurement coverage mismatch")
    for measurement in measurements:
        artifact_path = safe_member(folder, measurement["artifact"])
        if measurement["artifact"] not in expected_measurements:
            raise ValueError("Unknown measurement artifact")
        envelope = json.loads(artifact_path.read_text())
        config = entries.get((envelope["check_id"], "input", measurement.get("config")))
        if not config or not config.get("storage"):
            raise ValueError("Measurement configuration missing or unarchived: " + measurement["artifact"])
        try:
            settings = json.loads(safe_member(folder, config["storage"]).read_text())
            if (not isinstance(settings, dict) or not isinstance(settings.get("cases"), dict) or
                    type(settings.get("sets")) is not int or settings["sets"] < 1 or
                    type(settings.get("measured")) is not int or settings["measured"] < 1 or
                    not settings["cases"] or any(not isinstance(v, list) or not v or
                        any(not isinstance(n, str) or not n for n in v) for v in settings["cases"].values())):
                raise ValueError("Missing or malformed measurement settings")
        except (ValueError, TypeError) as error:
            raise ValueError("Malformed measurement configuration: " + measurement["artifact"]) from error
        payload = envelope["payload"]
        verify_samples(payload["rows"], payload["summary"], settings["cases"], sets=settings["sets"], measured=settings["measured"])
        if len(measurement["paths"]) != 2:
            raise ValueError("Audit measurement sidecar coverage mismatch")
        for position, key in enumerate(("rows", "summary")):
            path = measurement["paths"][position]
            expected_path = "measurements/" + envelope["check_id"] + "/" + artifact_path.name + "/" + ("samples.csv" if position == 0 else "medians.csv")
            if path != expected_path:
                raise ValueError("Measurement sidecar namespace mismatch")
            with safe_member(folder, path).open(newline="") as stream:
                actual_rows = list(csv.DictReader(stream))
            fields = set().union(*(r.keys() for r in payload[key]))
            if actual_rows != [{k: str(row.get(k, "")) for k in fields} for row in payload[key]]:
                raise ValueError("Audit measurement sidecar mismatch")
    return dict(verified=True, eligible_current=True, schema_version=SCHEMA_VERSION, campaign_id=run["campaign_id"],
                checks=len(records), audit_inputs=len(index), reran_binaries=False)
