"""Shared declared carrier identity; independent of variant implementation defaults."""
import re
from pathlib import PurePosixPath

from evidence_inputs import sha256


def validate_carrier(value):
    if not isinstance(value, dict) or set(value) != {"path", "archive", "transport"}:
        raise ValueError("Missing or malformed shared carrier descriptor")
    def relative(name):
        if (not isinstance(name, str) or not name or PurePosixPath(name).is_absolute() or
                any(part in ("", ".", "..") for part in name.split("/")) or "\\" in name):
            raise ValueError("Invalid shared carrier path: " + repr(name))
        return name
    package = relative(value["path"])
    for role in ("archive", "transport"):
        item = value[role]
        if (not isinstance(item, dict) or set(item) != {"path", "sha256"} or
                not isinstance(item.get("sha256"), str) or re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is None):
            raise ValueError("Invalid shared carrier identity: " + role)
        if not relative(item["path"]).startswith(package + "/"):
            raise ValueError("Shared carrier input escapes its package")
    if value["transport"]["path"] != package + "/transport.json":
        raise ValueError("Invalid shared carrier transport path")
    return value


def describe_carrier(root, package):
    from evidence_historical import carrier_metadata
    archive, _, _, signature = carrier_metadata(root / package, root / "unused-carrier-destination")
    value = dict(path=package, archive=dict(path=str(archive.relative_to(root)), sha256=signature),
                 transport=dict(path=package + "/transport.json", sha256=sha256(root / package / "transport.json")))
    check_carrier(root, value)
    return value


def check_carrier(root, value):
    from evidence_bundle import safe_member
    validate_carrier(value)
    for role in ("archive", "transport"):
        item = value[role]
        if sha256(safe_member(root, item["path"])) != item["sha256"]:
            raise ValueError("Shared carrier identity changed: " + role)
    return safe_member(root, value["path"])


def selected_carrier_inputs(config):
    if "historical-extraction" not in config.get("cases", {}).get("tooling", []):
        return []
    value = validate_carrier(config.get("carrier"))
    return [value[role]["path"] for role in ("archive", "transport")]


def verify_carrier_rows(rows, carrier, *, historical=False):
    for row in rows:
        if row.get("harness") != "tooling" or row.get("workload") != "historical-extraction":
            continue
        if historical and row.get("extraction_metric_version") == 1:
            continue  # Preserve archived observations; never invent a missing identity.
        value = validate_carrier(carrier)
        expected = dict(carrier_path=value["path"], carrier_archive_sha256=value["archive"]["sha256"],
                        carrier_transport_sha256=value["transport"]["sha256"])
        if any(row.get(key) != content for key, content in expected.items()):
            raise ValueError("Extraction sample differs from its declared shared carrier")


def verify_archived_carrier(settings, payload, entries, check_id, *, folder):
    if not any(row.get("extraction_metric_version") in (2, 3, 4) for row in payload["rows"]):
        return
    if payload.get("carrier") != settings.get("carrier"):
        raise ValueError("Measurement shared carrier differs from archived configuration")
    names = selected_carrier_inputs(settings)
    if len(names) != 2:
        raise ValueError("Measurement extraction carrier declarations are incomplete")
    for position, role in enumerate(("archive", "transport")):
        name = names[position]
        item = entries.get((check_id, "input", name))
        if not item or not item.get("storage") or item["sha256"] != settings["carrier"][role]["sha256"]:
            raise ValueError("Shared carrier input missing, unarchived, or mismatched: " + name)

    from evidence_bundle import safe_member
    archive = entries[(check_id, "input", names[0])]
    archive_bytes = safe_member(folder, archive["storage"]).stat().st_size
    if any(row.get("extraction_metric_version") in (3, 4) and row.get("archive_bytes") != archive_bytes for row in payload["rows"]):
        raise ValueError("Extraction archive byte counter differs from its archived carrier")
