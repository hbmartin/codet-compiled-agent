"""Shared fail-closed inputs and deterministic archives for Tiger distributions."""

import gzip
import hashlib
import json
import os
import re
import ssl
import tarfile
from pathlib import Path

from verified_sources import file_hash

CPP_NOTICES = (
    ("googletest", "LICENSE"),
    ("json", "LICENSE.MIT"),
    ("tomlplusplus", "LICENSE"),
    ("json-schema-validator", "LICENSE"),
)
RUNTIME_NOTICES = (
    ("legacy-support", "LICENSE"),
    ("zlib", "LICENSE"),
    ("openssl", "LICENSE.txt"),
    ("curl", "COPYING"),
    ("ncurses", "COPYING"),
    ("libedit", "COPYING"),
)
DOCUMENTS = (
    "PROMPTS.md",
    "TIGER-ACCEPTANCE.md",
    "MODERN-AGENT.md",
    "CODEX-TOOLS.md",
    "RELIABILITY-MODERNIZATION.md",
    "FEATURE-PORT.md",
    "REVIEW-FOLLOWUP.md",
)


def dependency_root(root):
    return Path(os.environ.get("CODET_DEPENDENCY_ROOT", root / ".deps")).resolve()


def required_bytes(path):
    try:
        data = path.read_bytes()
    except OSError as error:
        raise ValueError("Cannot read required distribution input: " + str(path)) from error
    if not data.strip():
        raise ValueError("Empty required distribution input: " + str(path))
    return data


def required_notices(root, deps=None):
    deps = deps or dependency_root(root)
    result = {
        name + ".txt": required_bytes(deps / name / relative) for name, relative in CPP_NOTICES
    }
    result.update(
        {
            name + "-" + relative: required_bytes(deps / ("tiger-" + name) / relative)
            for name, relative in RUNTIME_NOTICES
        }
    )
    header = required_bytes(deps / "tiger-sqlite/sqlite3.h")[:65536].lstrip()
    end = header.find(b"*/")
    if (
        not header.startswith(b"/*")
        or end < 0
        or b"The author disclaims copyright" not in header[:end]
    ):
        raise ValueError(
            "Missing SQLite copyright disclaimer: " + str(deps / "tiger-sqlite/sqlite3.h")
        )
    notice = re.split(rb"\n\*{20,}", header[:end], maxsplit=1)[0].rstrip()
    result["sqlite-NOTICE.txt"] = notice + b"\n*/\n"
    registry = json.loads(required_bytes(root / "licenses/distribution-notices.json"))
    expected = {"gcc-GPL-3.0.txt", "gcc-Runtime-Library-Exception-3.1.txt", "mozilla-MPL-2.0.txt"}
    if registry.get("format") != 1 or set(registry.get("notices", {})) != expected:
        raise ValueError("Incomplete distribution license registry")
    for name, identity in registry["notices"].items():
        data = required_bytes(root / "licenses" / name)
        if hashlib.sha256(data).hexdigest() != identity["sha256"]:
            raise ValueError("Distribution license digest mismatch: " + name)
        result[name] = data
    return result


def distribution_inputs(root, deps=None):
    result = {name: required_bytes(root / "docs" / name) for name in DOCUMENTS}
    for name in ("LICENSE.md", "dependencies.lock.json", "tiger-dependencies.lock.json"):
        result[name] = required_bytes(root / name)
    result["tiger-acceptance.sh"] = required_bytes(root / "scripts/tiger-acceptance.sh")
    result["tests/fixtures/events-v1.jsonl"] = required_bytes(
        root / "tests/fixtures/events-v1.jsonl"
    )
    for path in sorted((root / "licenses").rglob("*")):
        if path.is_file():
            result[path.relative_to(root).as_posix()] = required_bytes(path)
    for name, data in required_notices(root, deps).items():
        key = "licenses/" + name
        if key in result and result[key] != data:
            raise ValueError("License basename collision: " + name)
        result[key] = data
    return result


def validate_ca(root, certificate, identity=None, with_bytes=False):
    identity = (
        identity or json.loads((root / "tiger-dependencies.lock.json").read_text())["ca_bundle"]
    )
    data = required_bytes(certificate)
    if hashlib.sha256(data).hexdigest() != identity["sha256"]:
        raise ValueError("CA bundle must match the pinned Mozilla digest: " + str(certificate))
    certificates = re.findall(
        rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", data, re.S
    )
    if not certificates or data.count(b"-----BEGIN CERTIFICATE-----") != len(certificates):
        raise ValueError("Malformed Mozilla PEM certificate bundle")
    for number, pem in enumerate(certificates):
        try:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.load_verify_locations(cadata=pem.decode("ascii"))
        except (ValueError, UnicodeError, ssl.SSLError) as error:
            raise ValueError("Invalid Mozilla PEM certificate " + str(number)) from error
    result = dict(**identity, certificates=len(certificates))
    return (result, data) if with_bytes else result


def extra_notices(paths, inputs):
    result = {}
    names = {Path(name).name.casefold() for name in inputs if name.startswith("licenses/")}
    for path in paths:
        key = path.name.casefold()
        if key in names:
            raise ValueError("License basename collision: " + path.name)
        names.add(key)
        result["licenses/" + path.name] = required_bytes(path)
    return result


def write_inputs(folder, inputs):
    for name, data in inputs.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def deterministic_archive(folder, destination, epoch):
    with (
        destination.open("wb") as raw,
        gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=epoch) as zipped,
    ):
        with tarfile.open(fileobj=zipped, mode="w", format=tarfile.USTAR_FORMAT) as archive:
            for path in [folder, *sorted(folder.rglob("*"))]:
                name = (
                    folder.name
                    if path == folder
                    else folder.name + "/" + path.relative_to(folder).as_posix()
                )
                info = archive.gettarinfo(str(path), arcname=name)
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = epoch
                info.mode = 0o755 if info.isdir() or path.stat().st_mode & 0o111 else 0o644
                if info.isfile():
                    with path.open("rb") as source:
                        archive.addfile(info, source)
                else:
                    archive.addfile(info)


def publish_archive(staged, final):
    """Replace the compatibility archive/checksum pair with rollback on publication failure."""
    import tempfile

    checksum = staged.with_name(staged.name + ".sha256")
    checksum.write_text(file_hash(staged) + "  " + final.name + "\n")
    targets = ((staged, final), (checksum, final.with_name(final.name + ".sha256")))
    backups, installed = [], []
    backup = Path(tempfile.mkdtemp(prefix=".publish-", dir=final.parent))
    restored = True
    try:
        for source, target in targets:
            if target.exists() or target.is_symlink():
                saved = backup / target.name
                os.replace(target, saved)
                backups.append((saved, target))
            os.replace(source, target)
            installed.append(target)
    except BaseException as primary:
        for target in reversed(installed):
            target.unlink(missing_ok=True)
        for saved, target in reversed(backups):
            try:
                os.replace(saved, target)
            except OSError:
                restored = False
        if not restored:
            primary.recovery_directory = str(backup)
        raise
    finally:
        if restored:
            import shutil

            shutil.rmtree(backup)
