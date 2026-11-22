"""Audit an unchanged, transported historical v5 campaign with the current verifier."""
import json
import shutil
import tarfile
import tempfile
from pathlib import Path

from evidence_bundle import rename_exclusive, safe_member, valid_digest, verify_bundle
from evidence_inputs import sha256

PACKAGE = "docs/validation/configuration-input-preflight-followup/68a48c6fee9743c08d6a5e9f21d0aeef"


def carrier_metadata(package, destination):
    path = package / "transport.json"
    try:
        metadata = json.loads(path.read_text())
        if not isinstance(metadata, dict):
            raise ValueError("expected object")
        archive, expanded = metadata["archive"], metadata["expanded"]
        if not isinstance(archive, dict) or not isinstance(expanded, dict):
            raise ValueError("archive and expanded must be objects")
        for section, fields in ((archive, ("bytes",)), (expanded, ("files", "bytes"))):
            for field in fields:
                if type(section.get(field)) is not int or section[field] <= 0:
                    raise ValueError(("archive" if section is archive else "expanded") + "." + field + " must be a positive integer")
        signature = valid_digest(archive["sha256"])
        valid_digest(expanded["inventory_sha256"])
        archive_path = safe_member(package, archive["path"])
        prefix = expanded["prefix"]
        safe_member(destination, prefix)
        if "/" in prefix or prefix != metadata["capture_run_id"]:
            raise ValueError("invalid historical carrier prefix")
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise ValueError("Invalid historical carrier metadata " + str(path) + ": " + str(error)) from error
    return archive_path, archive, expanded, signature


def audit_package(package, destination):
    package, destination = Path(package), Path(destination)
    archive, archive_info, expanded, signature = carrier_metadata(package, destination)
    if sha256(archive) != signature or archive.stat().st_size != archive_info["bytes"]:
        raise ValueError("Historical carrier digest or size mismatch")
    if destination.exists() or destination.is_symlink():
        raise FileExistsError("Historical audit destination already exists: " + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".historical-", dir=destination.parent) as temporary:
        stage = Path(temporary) / "expanded"
        stage.mkdir()
        prefix = expanded["prefix"]
        files, size, seen = 0, 0, set()
        with tarfile.open(archive, "r:gz") as stream:
            for member in stream:
                target = safe_member(stage, member.name)
                if (not member.isfile() or member.name in seen or not member.name.startswith(prefix + "/") or
                        member.size < 0 or size + member.size > expanded["bytes"] or files + 1 > expanded["files"]):
                    raise ValueError("Invalid historical carrier member: " + member.name)
                seen.add(member.name)
                target.parent.mkdir(parents=True, exist_ok=True)
                with stream.extractfile(member) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)
                files += 1
                size += member.size
        bundle = safe_member(stage, prefix)
        if (files, size) != (expanded["files"], expanded["bytes"]) or sha256(bundle / "inventory.json") != expanded["inventory_sha256"]:
            raise ValueError("Historical carrier expansion mismatch")
        try:
            verify_bundle(bundle)
        except ValueError as error:
            strict_error = str(error)
            if "Invalid preparation measurement counters" not in strict_error:
                raise
        else:
            raise ValueError("Historical metric-v1 fixture unexpectedly met live acceptance")
        result = verify_bundle(bundle, historical=True)
        if not result["verified"] or result["eligible_current"] or not result.get("historical"):
            raise ValueError("Historical audit did not preserve acceptance boundaries")
        rename_exclusive(stage, destination)
    return dict(archive_sha256=signature, expanded_files=files, expanded_bytes=size,
                strict_rejected=True, strict_error=strict_error, historical_result=result)
