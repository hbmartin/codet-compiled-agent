"""Capture source-bound Tiger builds and archive immutable distribution inputs."""

import argparse
import hashlib
import json
import re
import subprocess
import uuid
from pathlib import Path

from evidence_inputs import role_hashes
from process_supervision import run
from tiger_distribution import dependency_root, distribution_inputs
from validation_evidence import atomic_json
from verified_sources import file_hash, tree_hashes

BINARIES = ("codet", "codet_tests", "codet_persistence_bench")


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


def prepare(root, deps, image):
    folder = root / "build/tiger-builds" / uuid.uuid4().hex
    folder.mkdir(parents=True)
    before = snapshots(root, deps)
    git = ["git", "-C", str(root)]
    commit = subprocess.check_output([*git, "rev-parse", "HEAD"], text=True, timeout=30).strip()
    epoch = int(
        subprocess.check_output([*git, "show", "-s", "--format=%ct", "HEAD"], text=True, timeout=30)
    )
    patch = subprocess.check_output([*git, "diff", "--binary", "HEAD"], timeout=30)
    inputs = distribution_inputs(root, deps)
    inputs["SOURCE.patch"] = patch
    index = {name: store(folder, data) for name, data in inputs.items()}
    source_index = {name: store(folder, (root / name).read_bytes()) for name in before["sources"]}
    record = dict(
        format=1,
        status="prepared",
        build_id=folder.name,
        source_commit=commit,
        source_epoch=epoch,
        toolchain_image=image,
        before=before,
        distribution=index,
        source_snapshot=source_index,
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


def build(root, folder):
    deps = dependency_root(root)
    record = json.loads((folder / "provenance.json").read_text())
    if (
        record.get("format") != 1
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
    for argv in (args, ["/usr/bin/cmake", "--build", str(output), "-j4", "--target", *BINARIES]):
        result = run(argv, cwd=root, timeout=600, check=True)
        commands.append(dict(argv=argv, exit=result.returncode))
    after = snapshots(root, deps)
    if (
        after != record["before"]
        or tools != {name: compiler(Path(prefix + name)) for name in tools}
        or build_tools != {name: compiler(Path("/usr/bin") / name) for name in build_tools}
    ):
        raise ValueError("Tiger source, dependency, sysroot or compiler changed during build")
    cache = (output / "CMakeCache.txt").read_text()

    def cached(key):
        return next(
            line.split("=", 1)[1] for line in cache.splitlines() if line.startswith(key + ":")
        )

    if Path(cached("CMAKE_HOME_DIRECTORY")).resolve() != root.resolve():
        raise ValueError("Tiger build used another source tree")
    for key, name in (("CMAKE_C_COMPILER", "gcc"), ("CMAKE_CXX_COMPILER", "g++")):
        if compiler(Path(cached(key))) != tools[name]:
            raise ValueError("Tiger cached compiler identity mismatch")
    record.update(
        status="built",
        after=after,
        compilers=tools,
        build_tools=build_tools,
        commands=commands,
        outputs={name: file_hash(output / name) for name in BINARIES},
        configuration={
            name: store(folder, (output / name).read_bytes())
            for name in ("CMakeCache.txt", "compile_commands.json", "Makefile")
        },
    )
    atomic_json(folder / "provenance.json", record)


def load(root):
    try:
        pointer = json.loads((root / "build/tiger-current.json").read_text())
    except FileNotFoundError as error:
        raise ValueError(
            "A new source-bound Tiger build is required; run scripts/tiger.py build"
        ) from error
    identifier = pointer.get("build_id", "")
    if not isinstance(identifier, str) or not re.fullmatch("[0-9a-f]{32}", identifier):
        raise ValueError("Invalid Tiger build identity")
    folder = root / "build/tiger-builds" / identifier
    path = folder / "provenance.json"
    if file_hash(path) != pointer.get("sha256"):
        raise ValueError("Tiger build provenance changed")
    record = json.loads(path.read_text())
    if (
        record.get("format") != 1
        or record.get("status") != "built"
        or record.get("build_id") != identifier
    ):
        raise ValueError("A new source-bound Tiger build is required")
    if record["before"] != record["after"] or set(record["outputs"]) != set(BINARIES):
        raise ValueError("Incomplete Tiger build provenance")
    if (
        set(record.get("configuration", {}))
        != {"CMakeCache.txt", "compile_commands.json", "Makefile"}
        or set(record.get("compilers", {})) != {"gcc", "g++"}
        or set(record.get("build_tools", {})) != {"cmake", "make"}
        or len(record.get("commands", [])) != 2
        or any(command.get("exit") != 0 for command in record["commands"])
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
    current_sysroot = tree_hashes(root / "build/tiger-sysroot")
    if current_sysroot != record["before"]["sysroot"]:
        raise ValueError("Tiger sysroot changed since build")
    return folder, record


def archived_inputs(root):
    folder, record = load(root)
    return {
        name: (folder / "inputs" / digest).read_bytes()
        for name, digest in record["distribution"].items()
    }


def copy_audit_inputs(root, destination):
    import shutil

    folder, record = load(root)
    data = destination / "build-inputs"
    data.mkdir()
    for digest in set().union(
        *(
            set(record[name].values())
            for name in ("distribution", "source_snapshot", "configuration")
        )
    ):
        shutil.copy2(folder / "inputs" / digest, data / digest)
    atomic_json(destination / "build-provenance.json", record)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["build"])
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    build(Path(__file__).resolve().parents[1], args.folder)
