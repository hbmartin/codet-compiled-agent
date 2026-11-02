"""Immutable audit bundles, independently verifiable without a checkout."""

import csv
import json
import os
import shutil
import tempfile
from pathlib import Path

from evidence_benchmarks import verify_samples
from evidence_inputs import file_specs


def _csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=sorted(set().union(*(r.keys() for r in rows))), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


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
                    sidecars.append(dict(artifact=target, config=spec.get("inputs", [None])[0], paths=paths))
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
        os.rename(stage, destination)
    return value


def verify_bundle(folder):
    """Audit recorded identities and archived material; does not rerun binaries."""
    from validation_evidence import SCHEMA_VERSION, digest
    inventory = json.loads((folder / "inventory.json").read_text())
    actual = {str(p.relative_to(folder)): digest(p) for p in sorted(folder.rglob("*"))
              if p.is_file() and p != folder / "inventory.json"}
    if actual != inventory:
        raise ValueError("Audit inventory mismatch")
    manifest = json.loads((folder / "manifest.json").read_text())
    run = json.loads((folder / "capture/run.json").read_text())
    report = json.loads((folder / "report.json").read_text())
    if run.get("schema_version") != SCHEMA_VERSION or run["manifest_sha256"] != digest(folder / "manifest.json"):
        raise ValueError("Audit capture identity mismatch")
    if (report["capture_run_id"], report["campaign_id"]) != (run["id"], run["campaign_id"]):
        raise ValueError("Audit report identity mismatch")
    index = json.loads((folder / "inputs.json").read_text())
    entries = {}
    for item in index:
        key = (item["check_id"], item["role"], item["path"])
        if key in entries:
            raise ValueError("Duplicate audit input")
        if item["storage"] is not None:
            if item["storage"] != "inputs/" + item["sha256"] or digest(folder / item["storage"]) != item["sha256"]:
                raise ValueError("Audit input digest mismatch")
        entries[key] = item
    required = {}
    expected_measurements = set()
    derived = []
    records = {}
    if len(report["validation"]) != len(manifest["checks"]):
        raise ValueError("Audit check coverage mismatch")
    for position, check in enumerate(manifest["checks"]):
        published = report["validation"][position]
        name = check["id"]
        record = json.loads((folder / "capture" / (name + ".json")).read_text())
        if (record["id"] != name or record["run_id"] != run["id"] or record["campaign_id"] != run["campaign_id"] or
                record["manifest_sha256"] != run["manifest_sha256"] or record["argv"] != check["argv"] or
                record["env"] != check.get("env", {}) or record["cwd"] != str(Path(check.get("cwd", manifest["root"])).resolve()) or
                record["source_before"] != report["production_hashes"] or record["source_after"] != record["source_before"] or
                record["validation_before"] != report["validation_hashes"] or record["validation_after"] != record["validation_before"] or
                record["build_inputs_before"] != record["build_inputs_after"] or record["input_before"] != record["input_after"] or
                record["external_inputs_before"] != record["external_inputs_after"] or
                record["log_sha256"] != digest(folder / "capture" / record["log"])):
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
        records[name] = record
        if set(record["build_inputs_before"]) != {s["path"] for s in check.get("source_inputs", [{"path": "."}])}:
            raise ValueError("Audit source coverage mismatch")
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
            path = folder / "artifacts" / name / Path(spec["path"]).name
            if saved["path"] != spec["path"] or digest(path) != saved["sha256"]:
                raise ValueError("Audit artifact digest mismatch")
            envelope = json.loads(path.read_text())
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
        envelope = json.loads((folder / measurement["artifact"]).read_text())
        config = entries[(envelope["check_id"], "input", measurement["config"])]
        settings = json.loads((folder / config["storage"]).read_text())
        payload = envelope["payload"]
        verify_samples(payload["rows"], payload["summary"], settings["cases"], sets=settings["sets"], measured=settings["measured"])
        if len(measurement["paths"]) != 2:
            raise ValueError("Audit measurement sidecar coverage mismatch")
        for position, key in enumerate(("rows", "summary")):
            path = measurement["paths"][position]
            with (folder / path).open(newline="") as stream:
                actual_rows = list(csv.DictReader(stream))
            fields = set().union(*(r.keys() for r in payload[key]))
            if actual_rows != [{k: str(row.get(k, "")) for k in fields} for row in payload[key]]:
                raise ValueError("Audit measurement sidecar mismatch")
    return dict(verified=True, schema_version=SCHEMA_VERSION, campaign_id=run["campaign_id"],
                checks=len(records), audit_inputs=len(index), reran_binaries=False)
