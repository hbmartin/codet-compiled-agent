#!/usr/bin/env python3
"""Package verified Tiger outputs with executable-relative libraries and TLS trust."""

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import tempfile

import tiger_provenance
from tiger_distribution import (
    dependency_root,
    deterministic_archive,
    extra_notices,
    publish_archive,
    validate_ca,
    write_inputs,
)
from tiger_distribution import required_notices as shared_required_notices
from tiger_macho import MachO

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = "/usr/local/bin/powerpc-apple-darwin8-"
SDK = pathlib.Path("/usr/local/MacOSX10.4u.sdk")


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True, timeout=30)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def required_notices():
    return shared_required_notices(ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ca-bundle", type=pathlib.Path)
    parser.add_argument("--extra-license", action="append", default=[], type=pathlib.Path)
    parser.add_argument("--build-id")
    parser.add_argument("--provenance-sha256")
    args = parser.parse_args()
    inspection = MachO(output, TOOLS)
    context = tiger_provenance.resolve(ROOT, args.build_id, args.provenance_sha256)
    build_folder, provenance = context.folder, context.record
    inputs = tiger_provenance.archived_inputs(ROOT, context)
    certificate = (args.ca_bundle or dependency_root(ROOT) / "tiger-cacert.pem").resolve(
        strict=True
    )
    ca, certificate_data = validate_ca(
        ROOT,
        certificate,
        json.loads(inputs["tiger-dependencies.lock.json"])["ca_bundle"],
        with_bytes=True,
    )
    extras = extra_notices(args.extra_license, inputs)
    original = context.verified_report()
    sysroot = ROOT / "build/tiger-sysroot"
    manifest = json.loads((sysroot / "manifest.json").read_text())
    prefix = manifest["prefix"]
    dist = ROOT / "build/dist"
    dist.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="portable-", dir=dist) as temporary:
        package = pathlib.Path(temporary) / "codet-tiger"
        (package / "bin").mkdir(parents=True)
        (package / "lib").mkdir()
        (package / "share/curl").mkdir(parents=True)
        binaries = []
        libraries = {}
        for record in original["binaries"]:
            source = build_folder / "native" / record["name"]
            if digest(source) != record["sha256"]:
                raise SystemExit("Binary changed since verification: " + str(source))
            target = package / "bin" / source.name
            tiger_provenance.copy_checked(source, target, record["sha256"])
            binaries.append(target)
        verified_libraries = {
            (sysroot / record["name"]).resolve(): record for record in original["runtime_libraries"]
        }
        pending = []
        for binary in binaries:
            pending.extend(
                (sysroot / dependency.lstrip("/")).resolve()
                for dependency in inspection.dependencies(binary)
                if dependency.startswith(prefix + "/")
            )
        while pending:
            source = pending.pop()
            if source in libraries:
                continue
            if source not in verified_libraries:
                raise SystemExit("Unverified runtime library: " + str(source))
            record = verified_libraries[source]
            if digest(source) != record["sha256"]:
                raise SystemExit("Library changed since verification: " + str(source))
            target = package / "lib" / source.name
            if target.exists():
                raise SystemExit("Runtime library basename collision: " + source.name)
            tiger_provenance.copy_checked(source, target, record["sha256"])
            libraries[source] = target
            pending.extend(
                (sysroot / dependency.lstrip("/")).resolve()
                for dependency in inspection.dependencies(source)
                if dependency.startswith(prefix + "/")
            )
        for target in libraries.values():
            subprocess.run(
                [
                    TOOLS + "install_name_tool",
                    "-id",
                    "@executable_path/../lib/" + target.name,
                    str(target),
                ],
                check=True,
                timeout=30,
            )
            inspection.invalidate(target)
        for target in binaries + list(libraries.values()):
            changes = []
            for dependency in inspection.dependencies(target):
                if dependency.startswith(prefix + "/"):
                    source = (sysroot / dependency.lstrip("/")).resolve()
                    if source not in libraries:
                        raise SystemExit("Unbundled library: " + dependency)
                    changes += [
                        "-change",
                        dependency,
                        "@executable_path/../lib/" + libraries[source].name,
                    ]
            if changes:
                subprocess.run(
                    [TOOLS + "install_name_tool", *changes, str(target)], check=True, timeout=30
                )
            inspection.invalidate(target)
        report = {
            "target_runtime_verified": False,
            "binaries": [],
            "runtime_libraries": [],
            "ca_bundle_sha256": ca["sha256"],
            "ca_bundle": ca,
            "build_id": provenance["build_id"],
            "build_provenance_sha256": original["build_provenance_sha256"],
        }
        for target in binaries + list(libraries.values()):
            inspection.architecture(target, (9,) if target in binaries else (0, 9))
            imports = inspection.dependencies(target)
            for dependency in imports:
                if dependency.startswith("@executable_path/"):
                    resolved = (package / "bin" / dependency.split("/", 1)[1]).resolve()
                    if not resolved.is_relative_to(package.resolve()) or not resolved.is_file():
                        raise SystemExit("Unresolved portable dependency: " + dependency)
                elif dependency.startswith(("/usr/lib/", "/System/Library/Frameworks/")):
                    if not (SDK / dependency.lstrip("/")).is_file():
                        raise SystemExit("Dependency missing from Tiger SDK: " + dependency)
                else:
                    raise SystemExit("Nonportable dependency: " + dependency)
            if "LC_RPATH" in inspection.load_commands(target):
                raise SystemExit("Tiger does not support LC_RPATH")
            record = {
                "name": str(target.relative_to(package)),
                "sha256": digest(target),
                "bytes": target.stat().st_size,
                "dependencies": imports,
            }
            report["binaries" if target in binaries else "runtime_libraries"].append(record)
        (package / "share/curl/curl-ca-bundle.crt").write_bytes(certificate_data)
        terminfo = sysroot / prefix.lstrip("/") / "share/terminfo"
        if not terminfo.is_dir():
            raise SystemExit("Missing bundled terminfo")
        shutil.copytree(terminfo, package / "share/terminfo", symlinks=True)
        terminal_root = (package / "share/terminfo").resolve()
        report["excluded_terminfo_aliases"] = []
        for path in terminal_root.rglob("*"):
            if not path.is_symlink():
                continue
            try:
                valid = path.resolve().is_relative_to(terminal_root) and path.resolve().is_file()
            except (OSError, RuntimeError):
                valid = False
            if not valid:
                report["excluded_terminfo_aliases"].append(str(path.relative_to(terminal_root)))
                path.unlink()
        # Case-insensitive staging can collapse obsolete aliases onto their own
        # targets. Exclude those broken links, but require common Mac terminals.
        for name in [
            "d/dumb",
            "v/vt100",
            "x/xterm",
            "x/xterm-color",
            "x/xterm-256color",
            "s/screen",
            "s/screen-256color",
        ]:
            if not (terminal_root / name).is_file():
                raise SystemExit("Missing required terminal entry: " + name)
        for binary in binaries:
            launcher = package / binary.name
            launcher.write_text(
                "#!/bin/sh\n"
                "entry=$0\n"
                'case "$entry" in /*) ;; *) entry=$PWD/$entry ;; esac\n'
                "links=0\n"
                'while [ -L "$entry" ]; do\n'
                '  links=$((links + 1)); [ "$links" -le 40 ] || exit 1\n'
                '  target=$(/usr/bin/readlink "$entry") || exit 1\n'
                '  case "$target" in /*) entry=$target ;; *) entry=$(dirname "$entry")/$target ;; esac\n'
                "done\n"
                'bundle=$(CDPATH= cd -P "$(dirname "$entry")" && pwd) || exit 1\n'
                'CODET_CA_BUNDLE=${CODET_CA_BUNDLE:-"$bundle/share/curl/curl-ca-bundle.crt"}\n'
                'TERMINFO=${TERMINFO:-"$bundle/share/terminfo"}\n'
                "export CODET_CA_BUNDLE TERMINFO\n"
                'exec "$bundle/bin/' + binary.name + '" "$@"\n'
            )
            launcher.chmod(0o755)
        write_inputs(package, inputs)
        write_inputs(package, extras)
        shutil.copy2(sysroot / "manifest.json", package / "sysroot-manifest.json")
        tiger_provenance.copy_audit_inputs(ROOT, package, context)
        report["source_commit"] = provenance["source_commit"]
        (package / "README.txt").write_text(
            "Codet for Mac OS X 10.4 PowerPC (32-bit, G3/G4/G5)\n\n"
            "Extract this folder and run ./codet --help from Terminal.\n"
            "Run ./codet -C /path/to/project --ui line to start.\n"
            "The launcher sets bundled TLS trust and terminal data. Keep the folder together.\n"
            "TigerPorts and administrator installation are not required.\n"
            "Provide an API key or import credentials as described in MODERN-AGENT.md.\n"
            "TLS verification stays enabled. CODET_CA_BUNDLE can select another PEM trust bundle.\n\n"
            "This is a cross-built candidate. PPC execution and live-provider acceptance\n"
            "have not been verified. Run sh ./tiger-acceptance.sh on the Tiger Mac.\n"
            "See the portable package instructions in TIGER-ACCEPTANCE.md.\n\n"
            "Source: https://github.com/hbmartin/codet-compiled-agent\n"
            "Commit: " + report["source_commit"] + "\n"
            "SOURCE.patch records tracked source changes used for this build.\n"
            "Dependency source URLs and hashes are in the lock files.\n"
        )
        (package / "portable-verification.json").write_text(json.dumps(report, indent=2) + "\n")
        archive = pathlib.Path(temporary) / "codet-tiger-ppc-portable.tar.gz"
        deterministic_archive(package, archive, provenance["source_epoch"])
        final = dist / archive.name
        context.verify_sysroot()
        publish_archive(archive, final)
        print(final)


if __name__ == "__main__":
    main()
