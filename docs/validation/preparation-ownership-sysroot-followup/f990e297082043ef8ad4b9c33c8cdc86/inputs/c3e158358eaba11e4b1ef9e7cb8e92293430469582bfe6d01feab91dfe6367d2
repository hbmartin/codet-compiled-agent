"""Capture source-bound Tiger builds and archive immutable distribution inputs."""

import argparse
import hashlib
import json
import re
import subprocess
import uuid
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from compiled_recipe import compiled_recipe
from dependency_paths import dependency_root
from evidence_inputs import role_hashes
from process_supervision import run
from tiger_distribution import distribution_inputs
from validation_evidence import atomic_json
from verified_sources import file_hash, tree_hashes

BINARIES = ("codet", "codet_tests", "codet_persistence_bench")
CONFIGURATION = {"CMakeCache.txt", "compile_commands.json", "Makefile", "codet-compiler-identity.json"}
BUILD_FORMAT = 3
BUILD_TIMEOUT = 1800


def positive_seconds(value):
    result = int(value)
    if result <= 0:
        raise ValueError("Build timeout must be positive")
    return result


def configured_compilers(root, output, tools):
    try:
        value = json.loads((output / "codet-compiler-identity.json").read_text())
        if (value["format"] != 1 or Path(value["source"]).resolve() != root.resolve()
                or Path(value["binary"]).resolve() != output.resolve()
                or value["build_type"] != "Release" or value["generator"] != "Unix Makefiles"
                or set(value["compilers"]) != {"C", "CXX"}):
            raise ValueError("Tiger configured source/build identity mismatch")
        for language, name in (("C", "gcc"), ("CXX", "g++")):
            entry = value["compilers"][language]
            if (not entry["id"] or not entry["version"] or not Path(entry["path"]).is_absolute()
                    or compiler(Path(entry["path"])) != tools[name]):
                raise ValueError("Tiger configured compiler identity mismatch")
    except (OSError, KeyError, TypeError) as error:
        raise ValueError("Missing or invalid Tiger compiler identity artifact") from error
    return value



def snapshots(root, deps):
    source = role_hashes(root, "build")
    dependencies = {
        name: tree_hashes(deps / name)
        for name in ("googletest", "json", "tomlplusplus", "json-schema-validator")
    }
    if any(not value for value in dependencies.values()):
        raise ValueError("Missing Tiger build dependency inputs")
    return dict(
        sources=source,
        dependencies=dependencies,
        distribution={
            name: hashlib.sha256(data).hexdigest()
            for name, data in distribution_inputs(root, deps).items()
        },
        sysroot=tree_hashes(root / "build/tiger-sysroot"),
    )


def store(folder, data):
    digest = hashlib.sha256(data).hexdigest()
    target = folder / "inputs" / digest
    target.parent.mkdir(exist_ok=True)
    if target.exists() and target.read_bytes() != data:
        raise ValueError("Tiger input digest collision")
    if not target.exists():
        target.write_bytes(data)
    return digest


def freeze_json(value):
    if isinstance(value, dict):
        return MappingProxyType({key: freeze_json(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(freeze_json(item) for item in value)
    return value


SYSROOT_RECOVERY = " Rebuild with python3 scripts/tiger.py sysroot, then create a fresh Tiger application build."


def validated_manifest(data, lock=None):
    try:
        value = json.loads(data)
        if (value['sysroot_format'] != 2 or not isinstance(value['recipe'], dict)
                or set(value['recipe']) != {'dependencies', 'patches'}
                or any(not isinstance(x, list) for x in value['recipe'].values())
                or not isinstance(value['prefix'], str) or not value['prefix'].startswith('/')
                or '..' in Path(value['prefix']).parts):
            raise ValueError('Invalid Tiger sysroot recipe manifest.' + SYSROOT_RECOVERY)
        recipe = compiled_recipe(lock) if lock is not None else None
        if lock is not None and value['recipe'] != recipe:
            raise ValueError('Tiger sysroot recipe does not match the dependency lock.' + SYSROOT_RECOVERY)
    except (KeyError, TypeError, json.JSONDecodeError) as error:
        raise ValueError('Invalid Tiger sysroot recipe manifest.' + SYSROOT_RECOVERY) from error
    return freeze_json(value)


def prepare(root, deps, image):
    before = snapshots(root, deps)
    identity = before['sysroot'].get('manifest.json')
    if not identity:
        raise ValueError('Missing Tiger sysroot manifest: ' + str(root / 'build/tiger-sysroot/manifest.json') + '.' + SYSROOT_RECOVERY)
    manifest = checked_bytes(root / 'build/tiger-sysroot/manifest.json', identity)
    inputs = distribution_inputs(root, deps)
    if {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()} != before['distribution']:
        raise ValueError('Tiger distribution inputs changed during preparation')
    validated_manifest(manifest, json.loads(inputs['tiger-dependencies.lock.json']))
    git = ["git", "-C", str(root)]
    commit = subprocess.check_output([*git, "rev-parse", "HEAD"], text=True, timeout=30).strip()
    epoch = int(
        subprocess.check_output([*git, "show", "-s", "--format=%ct", "HEAD"], text=True, timeout=30)
    )
    patch = subprocess.check_output([*git, "diff", "--binary", "HEAD"], timeout=30)
    inputs["SOURCE.patch"] = patch
    folder = root / "build/tiger-builds" / uuid.uuid4().hex
    folder.mkdir(parents=True)
    index = {name: store(folder, data) for name, data in inputs.items()}
    source_index = {name: store(folder, (root / name).read_bytes()) for name in before["sources"]}
    record = dict(
        format=BUILD_FORMAT,
        status="prepared",
        build_id=folder.name,
        source_commit=commit,
        source_epoch=epoch,
        toolchain_image=image,
        before=before,
        distribution=index,
        source_snapshot=source_index,
        sysroot_manifest=store(folder, manifest),
        root_provenance=str(root),
        dependency_provenance=str(deps),
    )
    atomic_json(folder / "provenance.json", record)
    return folder


def compiler(path):
    return dict(
        path=str(path),
        sha256=file_hash(path),
        version=run(
            [str(path), "--version"], timeout=30, capture_output=True, text=True, check=True
        ).stdout,
    )


def build(root, folder, build_timeout=BUILD_TIMEOUT):
    build_timeout = positive_seconds(build_timeout)
    deps = dependency_root(root)
    record = json.loads((folder / "provenance.json").read_text())
    if (
        record.get("format") != BUILD_FORMAT
        or record.get("status") != "prepared"
        or (folder / "native").exists()
    ):
        raise ValueError("Tiger build requires a fresh prepared provenance record")
    if snapshots(root, deps) != record["before"]:
        raise ValueError("Tiger inputs changed before configure")
    prefix = "/usr/local/bin/powerpc-apple-darwin8-"
    tools = {name: compiler(Path(prefix + name)) for name in ("gcc", "g++")}
    build_tools = {name: compiler(Path("/usr/bin") / name) for name in ("cmake", "make")}
    output = folder / "native"
    args = [
        "/usr/bin/cmake",
        "-S",
        str(root),
        "-B",
        str(output),
        "-G",
        "Unix Makefiles",
        "-DCMAKE_MAKE_PROGRAM=/usr/bin/make",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=" + str(root / "cmake/tiger.cmake"),
        "-DCODET_DEPENDENCY_ROOT=" + str(deps),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    import os

    args.append("-DCODET_TIGERPORTS_PREFIX=" + os.environ.get("TIGERPORTS_PREFIX", "/opt/tigr"))
    commands = []
    try:
        for argv, budget in ((args, 600), (["/usr/bin/cmake", "--build", str(output), "-j4", "--target", *BINARIES], build_timeout)):
            command = dict(argv=argv, timeout=budget)
            commands.append(command)
            try:
                result = run(argv, cwd=root, timeout=budget, check=True)
                command.update(exit=result.returncode, outcome="passed")
            except BaseException as error:
                command.update(exit=getattr(error, "returncode", None),
                               outcome="timeout" if isinstance(error, subprocess.TimeoutExpired)
                               else "interrupted" if isinstance(error, KeyboardInterrupt)
                               else "failed", error=str(error), cleanup=getattr(error, "process_cleanup", None))
                raise
        after = snapshots(root, deps)
        if (after != record["before"]
                or tools != {name: compiler(Path(prefix + name)) for name in tools}
                or build_tools != {name: compiler(Path("/usr/bin") / name) for name in build_tools}):
            raise ValueError("Tiger source, dependency, sysroot or compiler changed during build")
        configured_compilers(root, output, tools)
    except BaseException:
        record.update(status="failed", commands=commands, build_timeout=build_timeout)
        try:
            atomic_json(folder / "provenance.json", record)
        except OSError:
            pass # Preserve the original configure/build failure.
        raise
    record.update(
        status="built",
        build_timeout=build_timeout,
        after=after,
        compilers=tools,
        build_tools=build_tools,
        commands=commands,
        outputs={name: file_hash(output / name) for name in BINARIES},
        configuration={
            name: store(folder, (output / name).read_bytes())
            for name in sorted(CONFIGURATION)
        },
    )
    atomic_json(folder / "provenance.json", record)


def _load(root, build_id=None, provenance_sha256=None):
    try:
        pointer = (dict(build_id=build_id, sha256=provenance_sha256) if build_id is not None
                   else json.loads((root / "build/tiger-current.json").read_text()))
    except FileNotFoundError as error:
        raise ValueError(
            "A new source-bound Tiger build is required; run scripts/tiger.py build"
        ) from error
    identifier = pointer.get("build_id", "")
    if not isinstance(identifier, str) or not re.fullmatch("[0-9a-f]{32}", identifier):
        raise ValueError("Invalid Tiger build identity")
    folder = root / "build/tiger-builds" / identifier
    path = folder / "provenance.json"
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != pointer.get("sha256"):
        raise ValueError("Tiger build provenance changed")
    record = json.loads(data)
    if (
        record.get("format") != BUILD_FORMAT
        or record.get("status") != "built"
        or record.get("build_id") != identifier
    ):
        raise ValueError("A new source-bound Tiger build is required")
    if record["before"] != record["after"] or set(record["outputs"]) != set(BINARIES):
        raise ValueError("Incomplete Tiger build provenance")
    if (
        set(record.get("configuration", {}))
        != CONFIGURATION
        or set(record.get("compilers", {})) != {"gcc", "g++"}
        or set(record.get("build_tools", {})) != {"cmake", "make"}
        or len(record.get("commands", [])) != 2
        or any(command.get("exit") != 0 or command.get("outcome") != "passed"
               or type(command.get("timeout")) is not int or command["timeout"] <= 0
               for command in record["commands"])
        or type(record.get("build_timeout")) is not int or record["build_timeout"] <= 0
    ):
        raise ValueError("Incomplete Tiger configuration, tool or command provenance")
    for tool in [*record["compilers"].values(), *record["build_tools"].values()]:
        if (
            not tool.get("path")
            or not tool.get("version")
            or not re.fullmatch("[0-9a-f]{64}", tool.get("sha256", ""))
        ):
            raise ValueError("Incomplete Tiger executable identity")
    if (
        not re.fullmatch("[0-9a-f]{40}", record.get("source_commit", ""))
        or not re.fullmatch("sha256:[0-9a-f]{64}", record.get("toolchain_image", ""))
        or type(record.get("source_epoch")) is not int
        or not 0 <= record["source_epoch"] <= 0xFFFFFFFF
    ):
        raise ValueError("Invalid Tiger source or toolchain identity")

    for name, digest in record["outputs"].items():
        if file_hash(folder / "native" / name) != digest:
            raise ValueError("Tiger binary changed since build: " + name)
    for registry in ("distribution", "source_snapshot", "configuration"):
        for name, digest in record[registry].items():
            if (
                not isinstance(name, str)
                or Path(name).is_absolute()
                or any(x in ("", ".", "..") for x in name.split("/"))
            ):
                raise ValueError("Invalid Tiger archived input path")
            if (
                not re.fullmatch("[0-9a-f]{64}", digest)
                or file_hash(folder / "inputs" / digest) != digest
            ):
                raise ValueError("Tiger archived input changed: " + name)
    for name, digest in record["configuration"].items():
        if file_hash(folder / "native" / name) != digest:
            raise ValueError("Tiger configuration changed since build: " + name)
    if record["source_snapshot"] != record["before"]["sources"]:
        raise ValueError("Tiger source snapshot identity mismatch")
    if {
        name: digest for name, digest in record["distribution"].items() if name != "SOURCE.patch"
    } != record["before"]["distribution"]:
        raise ValueError("Tiger distribution snapshot identity mismatch")
    return folder, record, pointer["sha256"], data


@dataclass(frozen=True)
class BuildContext:
    root: Path
    folder: Path
    provenance_sha256: str
    _record: bytes
    manifest: object

    @property
    def record(self):
        return json.loads(self._record) # Callers cannot mutate the pinned snapshot.

    def verify_sysroot(self):
        if tree_hashes(self.root / "build/tiger-sysroot") != self.record["before"]["sysroot"]:
            raise ValueError("Tiger sysroot changed since build")

    def copy_sysroot_file(self, relative, destination):
        expected = self.record["before"]["sysroot"][relative]
        copy_checked(self.root / "build/tiger-sysroot" / relative, destination, expected)

    def verify_copied_tree(self, relative, destination):
        prefix = relative.rstrip("/") + "/"
        expected = {name[len(prefix):]: digest for name, digest in
                    self.record["before"]["sysroot"].items() if name.startswith(prefix)}
        if not expected or tree_hashes(destination) != expected:
            raise ValueError("Tiger copied sysroot tree differs from build: " + relative)

    @property
    def verification(self):
        return self.folder / "verification.json"

    def verified_report(self):
        report = json.loads(self.verification.read_text())
        if (report.get("build_id") != self.folder.name
                or report.get("build_provenance_sha256") != self.provenance_sha256
                or {item["name"]: item["sha256"] for item in report["binaries"]} != self.record["outputs"]):
            raise ValueError("Static verification belongs to another Tiger build")
        return report


def resolve(root, build_id=None, provenance_sha256=None):
    if (build_id is None) != (provenance_sha256 is None):
        raise ValueError("Both Tiger build ID and provenance digest are required")
    folder, record, digest, data = _load(root, build_id, provenance_sha256)
    manifest_digest = record.get('sysroot_manifest')
    if not manifest_digest or manifest_digest != record['before']['sysroot'].get('manifest.json'):
        raise ValueError('Tiger build lacks a pinned sysroot manifest')
    manifest_bytes = checked_bytes(folder / 'inputs' / manifest_digest, manifest_digest)
    lock_digest = record['distribution'].get('tiger-dependencies.lock.json')
    if not lock_digest:
        raise ValueError('Tiger build lacks an archived dependency lock.' + SYSROOT_RECOVERY)
    lock = json.loads(checked_bytes(folder / 'inputs' / lock_digest, lock_digest))
    context = BuildContext(root, folder, digest, data, validated_manifest(manifest_bytes, lock))
    context.verify_sysroot()
    return context


def load(root):
    context = resolve(root)
    return context.folder, context.record


def archived_inputs(root, context=None):
    context = context or resolve(root)
    return {name: checked_bytes(context.folder / "inputs" / digest, digest)
            for name, digest in context.record["distribution"].items()}


def checked_bytes(path, digest):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError("Tiger input changed while copying: " + str(path))
    return data


def copy_checked(source, destination, digest):
    import shutil
    shutil.copy2(source, destination)
    if file_hash(destination) != digest:
        raise ValueError("Tiger input changed while copying: " + str(source))


def copy_audit_inputs(root, destination, context=None):
    context = context or resolve(root)
    record = context.record
    data = destination / "build-inputs"
    data.mkdir()
    for digest in set().union(*(set(record[name].values())
                               for name in ("distribution", "source_snapshot", "configuration"))) | {record["sysroot_manifest"]}:
        copy_checked(context.folder / "inputs" / digest, data / digest, digest)
    (destination / "build-provenance.json").write_bytes(context._record)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["build"])
    parser.add_argument("folder", type=Path)
    parser.add_argument("--build-timeout", type=positive_seconds, default=BUILD_TIMEOUT)
    args = parser.parse_args()
    build(Path(__file__).resolve().parents[1], args.folder, args.build_timeout)
