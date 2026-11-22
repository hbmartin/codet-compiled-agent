"""Audit an unchanged, transported historical v5 campaign with the current verifier."""
import json
import shutil
import tarfile
from pathlib import Path

from evidence_bundle import safe_member, valid_digest, verify_bundle
from evidence_inputs import sha256

PACKAGE = "docs/validation/configuration-input-preflight-followup/68a48c6fee9743c08d6a5e9f21d0aeef"


def audit_package(package, destination):
    package, destination = Path(package), Path(destination)
    metadata = json.loads((package / "transport.json").read_text())
    archive = safe_member(package, metadata["archive"]["path"])
    signature = valid_digest(metadata["archive"]["sha256"])
    if sha256(archive) != signature or archive.stat().st_size != metadata["archive"]["bytes"]:
        raise ValueError("Historical carrier digest or size mismatch")
    expanded = metadata["expanded"]
    prefix = expanded["prefix"]
    safe_member(destination, prefix)
    if "/" in prefix or prefix != metadata["capture_run_id"]:
        raise ValueError("Invalid historical carrier prefix")
    destination.mkdir(parents=True, exist_ok=False)
    files, size, seen = 0, 0, set()
    with tarfile.open(archive, "r:gz") as stream:
        for member in stream:
            target = safe_member(destination, member.name)
            if (not member.isfile() or member.name in seen or not member.name.startswith(prefix + "/") or
                    member.size < 0 or size + member.size > expanded["bytes"] or files + 1 > expanded["files"]):
                raise ValueError("Invalid historical carrier member: " + member.name)
            seen.add(member.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            with stream.extractfile(member) as source, target.open("xb") as output:
                shutil.copyfileobj(source, output)
            files += 1
            size += member.size
    bundle = safe_member(destination, prefix)
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
    return dict(archive_sha256=signature, expanded_files=files, expanded_bytes=size,
                strict_rejected=True, strict_error=strict_error, historical_result=result)
