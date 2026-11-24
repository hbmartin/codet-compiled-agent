"""Audit an unchanged, transported historical v5 campaign with the current verifier."""
import json
import shutil
import tarfile
import tempfile
from contextlib import contextmanager
from pathlib import Path

from evidence_bundle import rename_exclusive, safe_member, valid_digest, verify_bundle
from evidence_inputs import sha256
from process_supervision import cleanup_interruptions, interruption

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



class HistoricalStage:
    """One invocation owns both its first failure and irrevocable publication."""
    def __init__(self, path):
        self.path = path
        self.result = None
        self.failure = None
        self.failure_traceback = None
        self.failure_context = None
        self.observation = dict(interruptions=[], failures=[])

    def publish(self, stage, destination, result):
        with cleanup_interruptions(self.observation):
            rename_exclusive(stage, destination)
            self.result = result
            result["publication"] = dict(committed=True, destination=str(destination),
                                         interruptions=self.observation["interruptions"])

    def remember(self, error, phase):
        if self.failure is None:
            self.failure = error
            self.failure_traceback = error.__traceback__
            self.failure_context = error.__context__
        elif error is not self.failure:
            if isinstance(error, KeyboardInterrupt):
                self.observation["interruptions"].append(dict(exception=type(error).__name__))
            else:
                self.observation.setdefault("failure", repr(error))
                self.observation["failures"].append(dict(phase=phase, error=repr(error)))
        self.failure.historical_cleanup = self.observation
        if self.result is not None:
            self.failure.historical_publication = self.result

    def raise_failure(self):
        # Do not replace an original explicit cause with a cleanup/teardown error.
        # Re-raising inside a handler would also make that later error its implicit
        # context, reversing causality; restore the context recorded at registration.
        try:
            raise self.failure.with_traceback(self.failure_traceback)
        finally:
            self.failure.__context__ = self.failure_context

    def recover(self, error):
        """Pure committed interruptions are observations, never replacements for failures."""
        if self.failure is not None:
            self.remember(error, "outer recovery")
            self.raise_failure()
        if self.result is None:
            return False
        error.historical_publication = self.result
        if not isinstance(error, KeyboardInterrupt):
            return False
        self.observation["interruptions"].append(dict(exception=type(error).__name__))
        return True


@contextmanager
def historical_staging(parent):
    owner = HistoricalStage(None)
    try:
        with interruption():
            temporary = tempfile.TemporaryDirectory(prefix=".historical-", dir=parent)
            owner.path = temporary.name
            try:
                yield owner
            except BaseException as error:
                if owner.result is not None and isinstance(error, KeyboardInterrupt):
                    owner.observation["interruptions"].append(dict(exception=type(error).__name__))
                else:
                    owner.remember(error, "operation")
                    raise
            finally:
                with cleanup_interruptions(owner.observation):
                    try:
                        temporary.cleanup()
                    except BaseException as cleanup:
                        if owner.failure is None:
                            # Register before leaving signal deferral: teardown may interrupt.
                            owner.remember(cleanup, "cleanup")
                            raise
                        owner.remember(cleanup, "cleanup")
                if owner.failure is not None:
                    owner.remember(owner.failure, "cleanup complete")
                elif owner.observation["interruptions"] and owner.result is None:
                    raise KeyboardInterrupt("Interrupted during historical staging cleanup")
    except BaseException as error:
        if owner.failure is None and not isinstance(error, KeyboardInterrupt):
            owner.remember(error, "scope teardown")
        if not owner.recover(error):
            raise


def audit_package(package, destination):
    package, destination = Path(package), Path(destination)
    archive, archive_info, expanded, signature = carrier_metadata(package, destination)
    if sha256(archive) != signature or archive.stat().st_size != archive_info["bytes"]:
        raise ValueError("Historical carrier digest or size mismatch")
    if destination.exists() or destination.is_symlink():
        raise FileExistsError("Historical audit destination already exists: " + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    owner = None
    try:
        with historical_staging(destination.parent) as owner:
            stage = Path(owner.path) / "expanded"
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
            receipt = dict(archive_sha256=signature, expanded_files=files, expanded_bytes=size,
                           strict_rejected=True, strict_error=strict_error, historical_result=result)
            owner.publish(stage, destination, receipt)
        return receipt
    except BaseException as error:
        if owner is not None and owner.recover(error):
            return owner.result
        raise
