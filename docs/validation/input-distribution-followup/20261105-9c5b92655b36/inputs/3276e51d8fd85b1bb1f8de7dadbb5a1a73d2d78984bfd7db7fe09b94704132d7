#!/usr/bin/env python3
"""Package verified Tiger outputs with executable-relative libraries and TLS trust."""
import argparse
import hashlib
import json
import pathlib
import shutil
import struct
import subprocess
import tarfile
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = "/usr/local/bin/powerpc-apple-darwin8-"
SDK = pathlib.Path("/usr/local/MacOSX10.4u.sdk")


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dependencies(path):
    return [line.strip().split(" ", 1)[0]
            for line in output(TOOLS + "otool", "-L", path).splitlines()[1:]]


def required_notices():
    """Read every required notice before a distribution can be published."""
    paths = {}
    for name, relative in [("googletest", "LICENSE"), ("json", "LICENSE.MIT"),
                           ("tomlplusplus", "LICENSE"), ("json-schema-validator", "LICENSE")]:
        paths[name + ".txt"] = ROOT / ".deps" / name / relative
    for name, relative in [("legacy-support", "LICENSE"), ("zlib", "LICENSE"),
                           ("openssl", "LICENSE.txt"), ("curl", "COPYING"),
                           ("ncurses", "COPYING"), ("libedit", "COPYING")]:
        paths[name + "-" + relative] = ROOT / ".deps" / ("tiger-" + name) / relative
    notices = {}
    for name, path in paths.items():
        try:
            data = path.read_bytes()
        except OSError as error:
            raise SystemExit("Cannot read required dependency notice: " + str(path)) from error
        if not data.strip():
            raise SystemExit("Empty required dependency notice: " + str(path))
        notices[name] = data
    sqlite = ROOT / ".deps/tiger-sqlite/sqlite3.h"
    try:
        with sqlite.open("rb") as source:
            header = source.read(65536).lstrip()
    except OSError as error:
        raise SystemExit("Cannot read SQLite notice: " + str(sqlite)) from error
    end = header.find(b"*/")
    if not header.startswith(b"/*") or end == -1 or b"The author disclaims copyright" not in header[:end]:
        raise SystemExit("Missing SQLite copyright disclaimer: " + str(sqlite))
    notices["sqlite-NOTICE.txt"] = header[:end + 2] + b"\n"
    return notices


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--ca-bundle", required=True, type=pathlib.Path)
    parser.add_argument("--extra-license", action="append", default=[], type=pathlib.Path)
    args = parser.parse_args()
    certificate = args.ca_bundle.resolve()
    if b"-----BEGIN CERTIFICATE-----" not in certificate.read_bytes():
        raise SystemExit("CA bundle contains no PEM certificates")
    notices = required_notices()
    original = json.loads((ROOT / "build/tiger-verification.json").read_text())
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
            source = ROOT / "build/tiger" / record["name"]
            if digest(source) != record["sha256"]:
                raise SystemExit("Binary changed since verification: " + str(source))
            target = package / "bin" / source.name
            shutil.copy2(source, target)
            binaries.append(target)
        verified_libraries = {(sysroot / record["name"]).resolve(): record
                              for record in original["runtime_libraries"]}
        pending = []
        for binary in binaries:
            pending.extend((sysroot / dependency.lstrip("/")).resolve()
                           for dependency in dependencies(binary)
                           if dependency.startswith(prefix + "/"))
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
            shutil.copy2(source, target)
            libraries[source] = target
            pending.extend((sysroot / dependency.lstrip("/")).resolve()
                           for dependency in dependencies(source)
                           if dependency.startswith(prefix + "/"))
        for target in libraries.values():
            subprocess.run([TOOLS + "install_name_tool", "-id",
                            "@executable_path/../lib/" + target.name, str(target)], check=True)
        for target in binaries + list(libraries.values()):
            changes = []
            for dependency in dependencies(target):
                if dependency.startswith(prefix + "/"):
                    source = (sysroot / dependency.lstrip("/")).resolve()
                    if source not in libraries:
                        raise SystemExit("Unbundled library: " + dependency)
                    changes += ["-change", dependency,
                                "@executable_path/../lib/" + libraries[source].name]
            if changes:
                subprocess.run([TOOLS + "install_name_tool", *changes, str(target)], check=True)
        report = {"target_runtime_verified": False, "binaries": [], "runtime_libraries": [],
                  "ca_bundle_sha256": digest(certificate)}
        for target in binaries + list(libraries.values()):
            with target.open("rb") as source:
                header = source.read(16)
            magic, cpu, subtype, kind = struct.unpack(">IIII", header)
            subtypes = (9,) if target in binaries else (0, 9)
            if (magic, cpu) != (0xfeedface, 18) or subtype not in subtypes or kind not in (2, 6):
                raise SystemExit("Unexpected PPC750 Mach-O: " + str(target))
            imports = dependencies(target)
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
            if "LC_RPATH" in output(TOOLS + "otool", "-l", target):
                raise SystemExit("Tiger does not support LC_RPATH")
            record = {"name": str(target.relative_to(package)), "sha256": digest(target),
                      "bytes": target.stat().st_size, "dependencies": imports}
            report["binaries" if target in binaries else "runtime_libraries"].append(record)
        shutil.copy2(certificate, package / "share/curl/curl-ca-bundle.crt")
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
        for name in ["d/dumb", "v/vt100", "x/xterm", "x/xterm-color",
                     "x/xterm-256color", "s/screen", "s/screen-256color"]:
            if not (terminal_root / name).is_file():
                raise SystemExit("Missing required terminal entry: " + name)
        for binary in binaries:
            launcher = package / binary.name
            launcher.write_text(
                '#!/bin/sh\n'
                'bundle=$(CDPATH= cd -P "$(dirname "$0")" && pwd) || exit 1\n'
                'CODET_CA_BUNDLE=${CODET_CA_BUNDLE:-"$bundle/share/curl/curl-ca-bundle.crt"}\n'
                'TERMINFO="$bundle/share/terminfo"\n'
                'export CODET_CA_BUNDLE TERMINFO\n'
                'exec "$bundle/bin/' + binary.name + '" "$@"\n')
            launcher.chmod(0o755)
        shutil.copy2(ROOT / "LICENSE.md", package / "LICENSE.md")
        shutil.copytree(ROOT / "licenses", package / "licenses")
        for license_file in args.extra_license:
            target = package / "licenses" / license_file.name
            if target.exists():
                raise SystemExit("License basename collision: " + license_file.name)
            shutil.copy2(license_file, target)
        for name, data in notices.items():
            target = package / "licenses" / name
            if target.exists():
                raise SystemExit("License basename collision: " + name)
            target.write_bytes(data)
        for name in ["dependencies.lock.json", "tiger-dependencies.lock.json"]:
            shutil.copy2(ROOT / name, package / name)
        shutil.copy2(sysroot / "manifest.json", package / "sysroot-manifest.json")
        for name in ["TIGER-ACCEPTANCE.md", "MODERN-AGENT.md", "CODEX-TOOLS.md", "PROMPTS.md"]:
            shutil.copy2(ROOT / "docs" / name, package / name)
        shutil.copy2(ROOT / "scripts/tiger-acceptance.sh", package / "tiger-acceptance.sh")
        (package / "tests/fixtures").mkdir(parents=True)
        shutil.copy2(ROOT / "tests/fixtures/events-v1.jsonl", package / "tests/fixtures/events-v1.jsonl")
        git = ["git", "-c", "safe.directory=" + str(ROOT), "-C", ROOT]
        report["source_commit"] = output(*git, "rev-parse", "HEAD").strip()
        patch = output(*git, "diff", "--binary", "HEAD")
        (package / "SOURCE.patch").write_text(patch)
        shutil.copy2(__file__, package / "package-tiger-portable.py")
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
            "Dependency source URLs and hashes are in the lock files.\n")
        (package / "portable-verification.json").write_text(json.dumps(report, indent=2) + "\n")
        archive = pathlib.Path(temporary) / "codet-tiger-ppc-portable.tar.gz"
        with tarfile.open(archive, "w:gz", format=tarfile.USTAR_FORMAT) as bundle:
            bundle.add(package, arcname=package.name)
        final = dist / archive.name
        archive.replace(final)
        (dist / (final.name + ".sha256")).write_text(digest(final) + "  " + final.name + "\n")
        print(final)


if __name__ == "__main__":
    main()
