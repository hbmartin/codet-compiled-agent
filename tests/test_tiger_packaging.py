"""Host distribution regressions; never execute PPC binaries."""

import contextlib
import csv
import importlib.util
import io
import json
import os
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import urllib.request
from pathlib import Path
from unittest.mock import patch

from tls_fixture import create_certificate, openssl_executable, tls_server

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
import tiger_distribution as distribution  # noqa: E402 -- standalone scripts
import tiger_provenance as provenance  # noqa: E402 -- standalone scripts
from tiger_macho import MachO  # noqa: E402 -- standalone scripts
from validation_evidence import atomic_json  # noqa: E402 -- standalone scripts
from verified_sources import file_hash  # noqa: E402 -- standalone scripts


def load_script(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), SCRIPTS / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


portable = load_script("package-tiger-portable")
tiger = load_script("tiger")
CPP = {
    "googletest": "LICENSE",
    "json": "LICENSE.MIT",
    "tomlplusplus": "LICENSE",
    "json-schema-validator": "LICENSE",
}
RUNTIME = {
    "legacy-support": "LICENSE",
    "zlib": "LICENSE",
    "openssl": "LICENSE.txt",
    "curl": "COPYING",
    "ncurses": "COPYING",
    "libedit": "COPYING",
}


class DistributionFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="codet-distribution-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.deps = self.root / ".deps"
        env = patch.dict(os.environ, CODET_DEPENDENCY_ROOT=str(self.deps))
        env.start()
        self.addCleanup(env.stop)
        self.write("LICENSE.md", "application license")
        shutil.copytree(ROOT / "licenses", self.root / "licenses")
        self.write("dependencies.lock.json", "{}")
        self.ca, _ = create_certificate(self.root)
        self.write(
            "tiger-dependencies.lock.json",
            json.dumps(
                dict(
                    ca_bundle=dict(
                        version="fixture",
                        url="https://fixture.invalid/mozilla.pem",
                        sha256=file_hash(self.ca),
                        license="MPL-2.0",
                    )
                )
            ),
        )
        for name, relative in CPP.items():
            self.write(".deps/" + name + "/" + relative, name + " notice")
        for name, relative in RUNTIME.items():
            self.write(".deps/tiger-" + name + "/" + relative, name + " notice")
        self.sqlite = "/*\n** The author disclaims copyright to this source code.\n** May you do good and not evil.\n** May you share freely.\n"
        self.write(
            ".deps/tiger-sqlite/sqlite3.h",
            self.sqlite
            + "*************************************************************************\n** Unrelated API documentation.\n*/\n",
        )
        for name in distribution.DOCUMENTS:
            self.write("docs/" + name, name + " frozen")
        self.write("scripts/tiger-acceptance.sh", "#!/bin/sh\nexit 0\n")
        self.write("tests/fixtures/events-v1.jsonl", "{}\n")
        self.write("CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n")
        self.write("src/modern/fixture.cpp", "int frozen;\n")
        self.write("build/tiger-sysroot/manifest.json", json.dumps(dict(prefix="/opt/tigr")))
        for name in [
            "d/dumb",
            "v/vt100",
            "x/xterm",
            "x/xterm-color",
            "x/xterm-256color",
            "s/screen",
            "s/screen-256color",
        ]:
            self.write("build/tiger-sysroot/opt/tigr/share/terminfo/" + name, "terminal")

        def git(argv, **kwargs):
            if "rev-parse" in argv:
                return "f" * 40 + "\n"
            if "show" in argv:
                return "1600000000\n"
            return b"frozen patch\n"

        self.git = git
        with patch.object(provenance.subprocess, "check_output", side_effect=git):
            self.folder = provenance.prepare(self.root, self.deps, "sha256:" + "d" * 64)
        record = json.loads((self.folder / "provenance.json").read_text())
        (self.folder / "native").mkdir()
        for name in provenance.BINARIES:
            (self.folder / "native" / name).write_bytes(struct.pack(">IIII", 0xFEEDFACE, 18, 9, 2))
        configuration = {}
        for name in ("CMakeCache.txt", "compile_commands.json", "Makefile"):
            path = self.folder / "native" / name
            path.write_text(name + " frozen")
            configuration[name] = provenance.store(self.folder, path.read_bytes())
        tools = {
            name: dict(path="/toolchain/" + name, sha256="a" * 64, version="fixture version")
            for name in ("gcc", "g++", "cmake", "make")
        }
        record.update(
            status="built",
            after=record["before"],
            configuration=configuration,
            compilers={name: tools[name] for name in ("gcc", "g++")},
            build_tools={name: tools[name] for name in ("cmake", "make")},
            commands=[
                dict(argv=["cmake", "configure"], exit=0),
                dict(argv=["cmake", "build"], exit=0),
            ],
            outputs={
                name: file_hash(self.folder / "native" / name) for name in provenance.BINARIES
            },
        )
        atomic_json(self.folder / "provenance.json", record)
        atomic_json(
            self.root / "build/tiger-current.json",
            dict(build_id=self.folder.name, sha256=file_hash(self.folder / "provenance.json")),
        )
        self.record = record
        self.verification = dict(
            build_id=self.folder.name,
            build_provenance_sha256=file_hash(self.folder / "provenance.json"),
            binaries=[dict(name=name, sha256=digest) for name, digest in record["outputs"].items()],
            runtime_libraries=[],
        )
        self.write("build/tiger-verification.json", json.dumps(self.verification))
        (self.root / "build/dist").mkdir()
        self.archive = self.root / "build/dist/codet-tiger-ppc-portable.tar.gz"
        self.checksum = self.archive.with_name(self.archive.name + ".sha256")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def package(self, *args):
        def output(*command):
            self.assertTrue(str(command[0]).endswith("otool"))
            return "fixture:\n" if command[1] == "-L" else ""

        with (
            patch.object(portable, "ROOT", self.root),
            patch.object(portable, "output", side_effect=output),
            patch.object(sys, "argv", ["portable", "--ca-bundle", str(self.ca), *args]),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            portable.main()


class PortablePackaging(DistributionFixture):
    def test_both_variants_use_complete_frozen_notices_and_sources(self):
        extra = self.write("extra/runtime.txt", "additional notice")
        self.package("--extra-license", str(extra))
        with tarfile.open(self.archive) as archive:
            for name in CPP:
                self.assertEqual(
                    archive.extractfile("codet-tiger/licenses/" + name + ".txt").read(),
                    (name + " notice").encode(),
                )
            for name, relative in RUNTIME.items():
                self.assertEqual(
                    archive.extractfile("codet-tiger/licenses/" + name + "-" + relative).read(),
                    (name + " notice").encode(),
                )
            for name in (
                "gcc-GPL-3.0.txt",
                "gcc-Runtime-Library-Exception-3.1.txt",
                "mozilla-MPL-2.0.txt",
            ):
                self.assertGreater(
                    len(archive.extractfile("codet-tiger/licenses/" + name).read()), 1000
                )
            notice = archive.extractfile("codet-tiger/licenses/sqlite-NOTICE.txt").read()
            self.assertIn(b"disclaims copyright", notice)
            self.assertNotIn(b"API documentation", notice)
            report = json.load(archive.extractfile("codet-tiger/portable-verification.json"))
            self.assertEqual(report["source_commit"], "f" * 40)
            self.assertEqual(
                archive.extractfile("codet-tiger/SOURCE.patch").read(), b"frozen patch\n"
            )
            self.assertIn(b"${TERMINFO:-", archive.extractfile("codet-tiger/codet").read())
        self.assertTrue(self.checksum.read_text().startswith(file_hash(self.archive)))
        with (
            patch.object(tiger, "ROOT", self.root),
            patch.object(tiger, "docker"),
            patch.object(sys, "argv", ["tiger", "package"]),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            tiger.main()
        with tarfile.open(self.root / "build/dist/codet-tiger-ppc.tar.gz") as archive:
            self.assertIn("codet-tiger/licenses/legacy-support-LICENSE", archive.getnames())
            self.assertIn(
                "codet-tiger/licenses/gcc-Runtime-Library-Exception-3.1.txt", archive.getnames()
            )

    def test_every_missing_or_empty_notice_fails_before_build_inputs_are_accepted(self):
        self.archive.write_bytes(b"previous")
        self.checksum.write_bytes(b"previous checksum")
        paths = [self.deps / name / relative for name, relative in CPP.items()]
        paths += [self.deps / ("tiger-" + name) / relative for name, relative in RUNTIME.items()]
        paths += [
            self.root / "licenses" / name
            for name in (
                "gcc-GPL-3.0.txt",
                "gcc-Runtime-Library-Exception-3.1.txt",
                "mozilla-MPL-2.0.txt",
            )
        ]
        for path in paths:
            original = path.read_bytes()
            for invalid in (None, b"", b" \n\t"):
                with self.subTest(path=path, invalid=invalid):
                    if invalid is None:
                        path.unlink()
                    else:
                        path.write_bytes(invalid)
                    try:
                        with self.assertRaisesRegex(ValueError, "required distribution input"):
                            distribution.distribution_inputs(self.root, self.deps)
                        self.assertEqual(self.archive.read_bytes(), b"previous")
                        self.assertEqual(self.checksum.read_bytes(), b"previous checksum")
                    finally:
                        path.write_bytes(original)
        sqlite = self.deps / "tiger-sqlite/sqlite3.h"
        sqlite.unlink()
        with self.assertRaisesRegex(ValueError, "sqlite3.h"):
            distribution.required_notices(self.root, self.deps)

    def test_packaging_at_another_checkout_uses_build_time_documents(self):
        self.write("docs/MODERN-AGENT.md", "changed after build")
        self.write("scripts/tiger-acceptance.sh", "changed script")
        self.package()
        with tarfile.open(self.archive) as archive:
            self.assertEqual(
                archive.extractfile("codet-tiger/MODERN-AGENT.md").read(), b"MODERN-AGENT.md frozen"
            )
            self.assertEqual(
                archive.extractfile("codet-tiger/tiger-acceptance.sh").read(),
                b"#!/bin/sh\nexit 0\n",
            )

    def test_stale_binary_archive_or_verification_preserves_previous_outputs(self):
        self.archive.write_bytes(b"previous")
        self.checksum.write_bytes(b"previous checksum")
        source = self.folder / "native/codet"
        original = source.read_bytes()
        source.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "binary changed"):
            self.package()
        source.write_bytes(original)
        data = self.folder / "inputs" / self.record["distribution"]["MODERN-AGENT.md"]
        data.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "archived input changed"):
            self.package()
        self.assertEqual(self.archive.read_bytes(), b"previous")
        self.assertEqual(self.checksum.read_bytes(), b"previous checksum")

    def test_certificates_and_expected_digest_are_both_checked(self):
        distribution.validate_ca(self.root, self.ca)
        wrong = self.write(
            "wrong.pem",
            "-----BEGIN CERTIFICATE-----\nnot a certificate\n-----END CERTIFICATE-----\n",
        )
        with self.assertRaisesRegex(ValueError, "pinned Mozilla digest"):
            distribution.validate_ca(self.root, wrong)
        identity = dict(sha256=file_hash(wrong))
        with self.assertRaisesRegex(ValueError, "Invalid Mozilla PEM"):
            distribution.validate_ca(self.root, wrong, identity)

    def test_archive_is_reproducible_and_publication_failure_rolls_back(self):
        self.package()
        before = self.archive.read_bytes()
        sum_before = self.checksum.read_bytes()
        os.utime(self.folder / "native/codet", (1, 1))
        self.package()
        self.assertEqual(self.archive.read_bytes(), before)
        real = os.replace

        def replace(source, target):
            if (
                Path(source).suffix == ".sha256"
                and Path(target) == self.checksum
                and Path(source).parent.name.startswith("portable-")
            ):
                raise OSError("publish fault")
            return real(source, target)

        with (
            patch.object(distribution.os, "replace", side_effect=replace),
            self.assertRaisesRegex(OSError, "publish fault"),
        ):
            self.package()
        self.assertEqual(self.archive.read_bytes(), before)
        self.assertEqual(self.checksum.read_bytes(), sum_before)


class TigerPackagingCommand(DistributionFixture):
    def test_worktree_dependencies_are_explicit_and_external_files_mounted_readonly(self):
        extra = self.write("external/notice, with spaces.txt", "additional notice")
        calls = []
        with (
            patch.object(tiger, "ROOT", self.root),
            patch.object(
                tiger.subprocess, "run", side_effect=lambda argv, **kw: calls.append(argv)
            ),
            patch.object(
                sys,
                "argv",
                [
                    "tiger",
                    "package-portable",
                    "--ca-bundle",
                    str(self.ca),
                    "--extra-license",
                    str(extra),
                ],
            ),
        ):
            tiger.main()
        self.assertEqual(len(calls), 2)
        command = calls[1]
        mounts = {}
        for i, arg in enumerate(command):
            if arg == "--mount":
                fields = next(csv.reader([command[i + 1]]))
                values = dict(x.split("=", 1) for x in fields if "=" in x)
                mounts[values["dst"]] = (values["src"], "readonly" in fields)
        self.assertEqual(mounts["/codet-deps"], (str(self.deps.resolve()), True))
        self.assertEqual(mounts["/project"], (str(self.root), True))
        self.assertEqual(mounts["/run/codet-packaging/ca.pem"], (str(self.ca.resolve()), True))
        self.assertEqual(command[command.index("--network") + 1], "none")
        self.assertEqual(command[command.index("--entrypoint") + 2], "sha256:" + "d" * 64)

    def test_bad_paths_notices_and_ca_fail_before_docker(self):
        missing = self.root / "missing.pem"
        loop = self.root / "loop"
        loop.symlink_to(loop)
        for path in (missing, loop, self.root):
            with (
                self.subTest(path=path),
                patch.object(tiger, "ROOT", self.root),
                patch.object(tiger, "docker") as docker,
                patch.object(sys, "argv", ["tiger", "package-portable", "--ca-bundle", str(path)]),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                with self.assertRaises(SystemExit):
                    tiger.main()
                docker.assert_not_called()
        collision = self.write("extra/gcc-GPL-3.0.txt", "collision")
        with (
            patch.object(tiger, "ROOT", self.root),
            patch.object(tiger, "docker") as docker,
            patch.object(
                sys,
                "argv",
                [
                    "tiger",
                    "package-portable",
                    "--ca-bundle",
                    str(self.ca),
                    "--extra-license",
                    str(collision),
                ],
            ),
            contextlib.redirect_stderr(io.StringIO()),
        ):
            with self.assertRaises(SystemExit):
                tiger.main()
            docker.assert_not_called()


class TigerBuildProvenance(DistributionFixture):
    def prepare(self):
        with patch.object(provenance.subprocess, "check_output", side_effect=self.git):
            return provenance.prepare(self.root, self.deps, "sha256:" + "d" * 64)

    def test_changed_sources_and_documents_are_rejected_before_configure(self):
        for name in ("src/modern/fixture.cpp", "docs/MODERN-AGENT.md"):
            with self.subTest(name=name):
                folder = self.prepare()
                path = self.root / name
                before = path.read_bytes()
                path.write_bytes(b"changed")
                try:
                    with (
                        patch.object(provenance, "run") as run,
                        self.assertRaisesRegex(ValueError, "changed before configure"),
                    ):
                        provenance.build(self.root, folder)
                    run.assert_not_called()
                finally:
                    path.write_bytes(before)

    def test_failed_build_cannot_replace_the_previously_verified_build(self):
        folder = self.prepare()
        previous = (self.root / "build/tiger-current.json").read_bytes()

        def compiler(path):
            return dict(path=str(path), sha256="b" * 64, version="fixture")

        with (
            patch.object(provenance, "compiler", side_effect=compiler),
            patch.object(
                provenance, "run", side_effect=subprocess.CalledProcessError(1, ["cmake"])
            ) as run,
        ):
            with self.assertRaises(subprocess.CalledProcessError):
                provenance.build(self.root, folder)
            self.assertEqual(run.call_args.kwargs["timeout"], 600)
        self.assertEqual(json.loads((folder / "provenance.json").read_text())["status"], "prepared")
        self.assertEqual((self.root / "build/tiger-current.json").read_bytes(), previous)
        self.assertEqual(provenance.load(self.root)[0], self.folder)

    def test_successful_mock_build_binds_compilers_and_detects_during_build_changes(self):
        for mutate in (False, True):
            with self.subTest(mutate=mutate):
                folder = self.prepare()
                source = self.root / "src/modern/fixture.cpp"
                before = source.read_bytes()

                def compiler(path):
                    return dict(path=str(path), sha256="b" * 64, version="fixture")

                def run(argv, folder=folder, mutate=mutate, source=source, **kwargs):
                    output = folder / "native"
                    if "-S" in argv:
                        output.mkdir()
                        (output / "CMakeCache.txt").write_text(
                            "CMAKE_HOME_DIRECTORY:INTERNAL="
                            + str(self.root)
                            + "\nCMAKE_C_COMPILER:FILEPATH=/usr/local/bin/powerpc-apple-darwin8-gcc\nCMAKE_CXX_COMPILER:FILEPATH=/usr/local/bin/powerpc-apple-darwin8-g++\n"
                        )
                        for name in ("compile_commands.json", "Makefile"):
                            (output / name).write_text("configured")
                    else:
                        for name in provenance.BINARIES:
                            (output / name).write_bytes(b"new binary")
                        if mutate:
                            source.write_bytes(b"changed during build")
                    return subprocess.CompletedProcess(argv, 0)

                try:
                    with (
                        patch.object(provenance, "compiler", side_effect=compiler),
                        patch.object(provenance, "run", side_effect=run),
                    ):
                        if mutate:
                            with self.assertRaisesRegex(ValueError, "changed during build"):
                                provenance.build(self.root, folder)
                            self.assertEqual(
                                json.loads((folder / "provenance.json").read_text())["status"],
                                "prepared",
                            )
                        else:
                            provenance.build(self.root, folder)
                            record = json.loads((folder / "provenance.json").read_text())
                            self.assertEqual(record["before"], record["after"])
                            self.assertEqual(record["status"], "built")
                            self.assertEqual(set(record["outputs"]), set(provenance.BINARIES))
                finally:
                    source.write_bytes(before)

    def test_modified_cache_and_incomplete_metadata_cannot_be_packaged(self):
        cache = self.folder / "native/CMakeCache.txt"
        cache.write_text("different compiler")
        with self.assertRaisesRegex(ValueError, "configuration changed"):
            provenance.load(self.root)
        cache.write_text("CMakeCache.txt frozen")
        record = json.loads((self.folder / "provenance.json").read_text())
        record["compilers"] = {}
        atomic_json(self.folder / "provenance.json", record)
        atomic_json(
            self.root / "build/tiger-current.json",
            dict(build_id=self.folder.name, sha256=file_hash(self.folder / "provenance.json")),
        )
        with self.assertRaisesRegex(ValueError, "Incomplete Tiger configuration"):
            provenance.load(self.root)

    def test_worktree_mounts_resolve_external_dependencies_without_container_git(self):
        with tempfile.TemporaryDirectory() as external:
            target = Path(external) / "dependencies"
            self.deps.rename(target)
            self.deps.symlink_to(target, target_is_directory=True)
            self.write(".git", "gitdir: /outside/worktree/metadata")
            calls = []
            with (
                patch.object(tiger, "ROOT", self.root),
                patch.object(
                    tiger.subprocess, "run", side_effect=lambda argv, **kw: calls.append(argv)
                ),
                patch.object(
                    sys, "argv", ["tiger", "package-portable", "--ca-bundle", str(self.ca)]
                ),
            ):
                tiger.main()
            command = calls[-1]
            self.assertIn(tiger.bind_mount(target.resolve(), "/codet-deps", True), command)
            self.assertFalse(any("git" == token for token in command))

    def test_missing_legacy_provenance_is_an_identifying_error(self):
        (self.root / "build/tiger-current.json").unlink()
        with self.assertRaisesRegex(ValueError, "source-bound Tiger build"):
            provenance.load(self.root)


class MachOInspection(unittest.TestCase):
    def test_cached_linkage_is_invalidated_after_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "binary"
            path.write_bytes(b"fixture")
            calls = []

            def output(*argv):
                calls.append(argv)
                return (
                    "binary:\n/first/lib.dylib\n"
                    if len(calls) == 1
                    else "binary:\n/second/lib.dylib\n"
                )

            inspection = MachO(output, "tool-")
            self.assertEqual(inspection.dependencies(path), ["/first/lib.dylib"])
            self.assertEqual(inspection.dependencies(path), ["/first/lib.dylib"])
            self.assertEqual(len(calls), 1)
            inspection.invalidate(path)
            self.assertEqual(inspection.dependencies(path), ["/second/lib.dylib"])
            self.assertEqual(len(calls), 2)


class TLSFixtureReliability(unittest.TestCase):
    def test_stalled_handshake_does_not_block_other_connections(self):
        with tempfile.TemporaryDirectory() as temporary, tls_server(Path(temporary)) as server:
            with socket.create_connection(server.server_address, timeout=3):
                context = ssl.create_default_context(cafile=str(server.certificate))
                request = urllib.request.Request(
                    "https://127.0.0.1:" + str(server.server_port) + "/v1/chat/completions",
                    data=b"{}",
                )
                with urllib.request.urlopen(request, context=context, timeout=3) as response:
                    self.assertIn(b"trusted TLS fixture", response.read())
                self.assertEqual(server.requests, ["/v1/chat/completions"])

    def test_cleanup_cannot_replace_original_failure(self):
        original = ValueError("primary fixture failure")
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError) as caught:
                with tls_server(Path(temporary)) as server:
                    close = server.server_close

                    def failing_close():
                        close()
                        raise RuntimeError("secondary close failure")

                    server.server_close = failing_close
                    raise original
            self.assertIs(caught.exception, original)


class TigerLauncher(DistributionFixture):
    def test_external_symlink_preserves_working_directory_and_explicit_terminal_data(self):
        self.package()
        extract = self.root / "extracted"
        extract.mkdir()
        with tarfile.open(self.archive) as archive:
            for item in archive:
                if item.isfile():
                    target = extract / item.name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(archive.extractfile(item).read())
                    target.chmod(item.mode)
        binary = extract / "codet-tiger/bin/codet"
        binary.write_text('#!/bin/sh\nprintf "%s\\n" "$PWD" "$TERMINFO" "$CODET_CA_BUNDLE"\n')
        binary.chmod(0o755)
        link = self.root / "external launcher"
        link.symlink_to(extract / "codet-tiger/codet")
        env = dict(os.environ, TERMINFO="/chosen/terminfo", CODET_CA_BUNDLE="/chosen/runtime.pem")
        result = subprocess.run(
            [str(link)],
            cwd=self.root,
            text=True,
            capture_output=True,
            env=env,
            timeout=5,
            check=True,
        )
        self.assertEqual(
            result.stdout.splitlines(),
            [str(self.root.resolve()), "/chosen/terminfo", "/chosen/runtime.pem"],
        )


class OpenSSLPrerequisite(unittest.TestCase):
    def test_configured_absolute_executable_generates_certificates_without_path(self):
        executable = openssl_executable()
        with (
            tempfile.TemporaryDirectory() as temporary,
            patch.dict(os.environ, PATH=temporary, CODET_TEST_OPENSSL=executable),
        ):
            certificate, key = create_certificate(Path(temporary))
            ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER).load_cert_chain(certificate, key)

    def test_missing_executable_has_clear_execution_time_error(self):
        with (
            tempfile.TemporaryDirectory() as temporary,
            patch.dict(os.environ, PATH=temporary, CODET_TEST_OPENSSL=""),
        ):
            with self.assertRaisesRegex(RuntimeError, "Host TLS tests require"):
                openssl_executable()


if __name__ == "__main__":
    unittest.main()
