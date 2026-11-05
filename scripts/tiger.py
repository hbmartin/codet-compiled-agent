#!/usr/bin/env python3
"""Build, verify and package the modern PPC agent without executing it."""

import argparse
import csv
import io
import json
import os
import subprocess
import tempfile
from pathlib import Path

import tiger_provenance
from tiger_distribution import (
    dependency_root,
    deterministic_archive,
    extra_notices,
    publish_archive,
    validate_ca,
    write_inputs,
)
from validation_evidence import atomic_json
from verified_sources import file_hash

ROOT = Path(__file__).resolve().parents[1]
IMAGE = os.environ.get("CODET_TOOLCHAIN_IMAGE", "codet-tiger-build:gcc14")


def bind_mount(source, target, readonly=False):
    fields = ["type=bind", "src=" + str(source), "dst=" + target]
    if readonly:
        fields.append("readonly")
    stream = io.StringIO()
    csv.writer(stream, lineterminator="").writerow(fields)
    return stream.getvalue()


def image_identity():
    return subprocess.check_output(
        ["docker", "image", "inspect", IMAGE, "--format", "{{.Id}}"], text=True, timeout=30
    ).strip()


def docker(args, mounts=(), image=None):
    identity = image or image_identity()
    command = [
        "docker",
        "run",
        "--rm",
        "--pull=never",
        "--platform",
        "linux/amd64",
        "--network",
        "none",
        "--read-only",
        "--cap-drop",
        "ALL",
        "--security-opt",
        "no-new-privileges",
        "--tmpfs",
        "/tmp:rw,noexec,nosuid,size=256m",
        "--mount",
        bind_mount(ROOT, "/project", True),
        "--mount",
        bind_mount(ROOT / "build", "/project/build"),
        "--mount",
        bind_mount(dependency_root(ROOT), "/codet-deps", True),
        "--env",
        "CODET_DEPENDENCY_ROOT=/codet-deps",
        "--env",
        "CODET_TOOLCHAIN_ID=" + identity,
    ]
    for source, target in mounts:
        command += ["--mount", bind_mount(source, target, True)]
    for key in ("TIGERPORTS_PREFIX", "JOBS"):
        if key in os.environ:
            command += ["--env", key + "=" + os.environ[key]]
    command += ["--workdir", "/project", "--entrypoint", args[0], identity, *args[1:]]
    subprocess.run(command, check=True)


def selected_file(parser, path):
    try:
        source = path.resolve(strict=True)
    except (OSError, RuntimeError) as error:
        parser.error("Cannot read packaging file: " + str(path) + ": " + str(error))
    if not source.is_file() or not os.access(source, os.R_OK):
        parser.error("Packaging file is not readable: " + str(path))
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action", choices=["image", "sysroot", "build", "verify", "package", "package-portable"]
    )
    parser.add_argument("--ca-bundle", type=Path)
    parser.add_argument("--extra-license", action="append", default=[], type=Path)
    parser.add_argument("--build-timeout", type=tiger_provenance.positive_seconds,
                        default=tiger_provenance.BUILD_TIMEOUT)
    args = parser.parse_args()
    if args.action != "package-portable" and (args.ca_bundle is not None or args.extra_license):
        parser.error("Packaging file options require package-portable")
    (ROOT / "build").mkdir(exist_ok=True)
    if args.action == "image":
        subprocess.run(
            [
                "docker",
                "build",
                "--platform",
                "linux/amd64",
                "-t",
                IMAGE,
                "-f",
                str(ROOT / "cmake/Dockerfile.tiger"),
                str(ROOT / "cmake"),
            ],
            check=True,
        )
        return
    if args.action == "sysroot":
        docker(["/usr/bin/python3", "scripts/build-tiger-sysroot.py"])
        return
    if args.action == "build":
        identity = image_identity()
        folder = tiger_provenance.prepare(ROOT, dependency_root(ROOT), identity)
        docker(
            [
                "/usr/bin/python3",
                "scripts/tiger_provenance.py",
                "build",
                str(folder.relative_to(ROOT)),
                "--build-timeout", str(args.build_timeout),
            ],
            image=identity,
        )
        record = json.loads((folder / "provenance.json").read_text())
        if record["status"] != "built":
            raise ValueError("Tiger build did not finish")
        atomic_json(
            ROOT / "build/tiger-current.json",
            dict(build_id=folder.name, sha256=file_hash(folder / "provenance.json")),
        )
        print(folder / "native")
        return
    try:
        context = tiger_provenance.resolve(ROOT)
        folder, record = context.folder, context.record
        selected = ["--build-id", folder.name, "--provenance-sha256", context.provenance_sha256]
        inputs = tiger_provenance.archived_inputs(ROOT, context)
        mounts = []
        command = []
        if args.action == "package-portable":
            certificate = selected_file(
                parser, args.ca_bundle or dependency_root(ROOT) / "tiger-cacert.pem"
            )
            validate_ca(
                ROOT, certificate, json.loads(inputs["tiger-dependencies.lock.json"])["ca_bundle"]
            )
            extras = [selected_file(parser, path) for path in args.extra_license]
            extra_notices(extras, inputs)
            mounts = [(certificate, "/run/codet-packaging/ca.pem")]
            command = [
                "/usr/bin/python3",
                "scripts/package-tiger-portable.py",
                *selected,
                "--ca-bundle",
                "/run/codet-packaging/ca.pem",
            ]
            for index, path in enumerate(extras):
                target = "/run/codet-packaging/licenses/" + str(index) + "/" + path.name
                mounts.append((path, target))
                command += ["--extra-license", target]
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))
    docker(["/usr/bin/python3", "scripts/verify-tiger.py", *selected], image=record["toolchain_image"])
    if args.action == "verify":
        return
    if args.action == "package-portable":
        docker(command, mounts, image=record["toolchain_image"])
        return
    dist = ROOT / "build/dist"
    dist.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tigerports-", dir=dist) as temporary:
        package = Path(temporary) / "codet-tiger"
        package.mkdir()
        write_inputs(package, inputs)
        import shutil

        for name in tiger_provenance.BINARIES:
            tiger_provenance.copy_checked(folder / "native" / name, package / name,
                                           record["outputs"][name])
        atomic_json(package / "static-verification.json", context.verified_report())
        shutil.copy2(ROOT / "build/tiger-sysroot/manifest.json", package / "sysroot-manifest.json")
        tiger_provenance.copy_audit_inputs(ROOT, package, context)
        staged = Path(temporary) / "codet-tiger-ppc.tar.gz"
        deterministic_archive(package, staged, record["source_epoch"])
        final = dist / staged.name
        context.verify_sysroot()
        publish_archive(staged, final)
        print(final)


if __name__ == "__main__":
    main()
