"""Host regressions for portable Tiger packaging; never execute PPC code."""
import contextlib
import csv
import hashlib
import importlib.util
import io
import json
import os
import pathlib
import ssl
import struct
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from tls_fixture import create_certificate, openssl_executable

SCRIPTS = pathlib.Path(__file__).resolve().parents[1]/'scripts'


def load_script(name):
    spec = importlib.util.spec_from_file_location(name.replace('-', '_'), SCRIPTS/(name+'.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


portable = load_script('package-tiger-portable')
tiger = load_script('tiger')
RUNTIME_NOTICES = {
    'legacy-support': 'LICENSE', 'zlib': 'LICENSE', 'openssl': 'LICENSE.txt',
    'curl': 'COPYING', 'ncurses': 'COPYING', 'libedit': 'COPYING',
}
CPP_NOTICES = {'googletest': 'LICENSE', 'json': 'LICENSE.MIT',
               'tomlplusplus': 'LICENSE', 'json-schema-validator': 'LICENSE'}


class PortablePackaging(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='codet-portable-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.ca = self.root/'selected trust.pem'
        self.ca.write_text('-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----\n')
        self.write('LICENSE.md', 'application license')
        self.write('licenses/codex-NOTICE.txt', 'application notice')
        for name, relative in CPP_NOTICES.items():
            self.write('.deps/'+name+'/'+relative, name+' notice')
        for name, relative in RUNTIME_NOTICES.items():
            self.write('.deps/tiger-'+name+'/'+relative, name+' notice')
        self.sqlite_notice = '/* The author disclaims copyright to this source code.\nFixture blessing. */\n'
        self.write('.deps/tiger-sqlite/sqlite3.h', self.sqlite_notice+'#define SQLITE_VERSION "fixture"\n')
        self.write('build/tiger-sysroot/manifest.json', json.dumps({'prefix': '/opt/tigr'}))
        binary = self.root/'build/tiger/codet'
        binary.parent.mkdir(parents=True)
        binary.write_bytes(struct.pack('>IIII', 0xfeedface, 18, 9, 2))
        self.write('build/tiger-verification.json', json.dumps({'binaries': [
            {'name': 'codet', 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}],
            'runtime_libraries': []}))
        for name in ['d/dumb', 'v/vt100', 'x/xterm', 'x/xterm-color', 'x/xterm-256color',
                     's/screen', 's/screen-256color']:
            self.write('build/tiger-sysroot/opt/tigr/share/terminfo/'+name, 'terminal fixture')
        for name in ['dependencies.lock.json', 'tiger-dependencies.lock.json']:
            self.write(name, '{}')
        for name in ['TIGER-ACCEPTANCE.md', 'MODERN-AGENT.md', 'CODEX-TOOLS.md', 'PROMPTS.md']:
            self.write('docs/'+name, name+' fixture')
        self.write('scripts/tiger-acceptance.sh', '#!/bin/sh\nexit 0\n')
        self.write('tests/fixtures/events-v1.jsonl', '{}\n')
        (self.root/'build/dist').mkdir()
        self.archive = self.root/'build/dist/codet-tiger-ppc-portable.tar.gz'
        self.checksum = self.archive.with_name(self.archive.name+'.sha256')

    def write(self, name, text):
        path = self.root/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def tool_output(self, *args):
        if str(args[0]).endswith('otool'):
            return 'fixture:\n' if args[1] == '-L' else ''
        if args[0] == 'git':
            return 'fixture-source-commit\n' if 'rev-parse' in args else ''
        self.fail('unexpected external tool: '+repr(args))

    def package(self, *args):
        with patch.object(portable, 'ROOT', self.root), \
                patch.object(portable, 'output', side_effect=self.tool_output), \
                patch.object(sys, 'argv', ['package-tiger-portable.py', '--ca-bundle', str(self.ca), *args]), \
                contextlib.redirect_stdout(io.StringIO()):
            portable.main()

    def test_archive_contains_required_notices_and_sqlite_disclaimer(self):
        extra = self.write('extra notices/runtime exception.txt', 'extra toolchain notice')
        self.package('--extra-license', str(extra))
        with tarfile.open(self.archive) as archive:
            for name, relative in RUNTIME_NOTICES.items():
                self.assertEqual(archive.extractfile('codet-tiger/licenses/'+name+'-'+relative).read(),
                                 (name+' notice').encode())
            for name in CPP_NOTICES:
                self.assertEqual(archive.extractfile('codet-tiger/licenses/'+name+'.txt').read(),
                                 (name+' notice').encode())
            self.assertEqual(archive.extractfile('codet-tiger/licenses/sqlite-NOTICE.txt').read(),
                             self.sqlite_notice.encode())
            self.assertEqual(archive.extractfile('codet-tiger/licenses/runtime exception.txt').read(),
                             b'extra toolchain notice')
            report = json.load(archive.extractfile('codet-tiger/portable-verification.json'))
            self.assertEqual(report['source_commit'], 'fixture-source-commit')
            self.assertEqual(archive.extractfile('codet-tiger/SOURCE.patch').read(), b'')
        self.assertTrue(self.checksum.read_text().startswith(hashlib.sha256(self.archive.read_bytes()).hexdigest()))

    def test_missing_runtime_notice_preserves_existing_distribution(self):
        self.archive.write_bytes(b'previous archive')
        self.checksum.write_bytes(b'previous checksum')
        for name, relative in RUNTIME_NOTICES.items():
            with self.subTest(dependency=name):
                path = self.root/'.deps'/('tiger-'+name)/relative
                data = path.read_bytes()
                path.unlink()
                try:
                    with self.assertRaisesRegex(SystemExit, 'required dependency notice') as caught:
                        self.package()
                    self.assertIn(str(path), str(caught.exception))
                    self.assertEqual(self.archive.read_bytes(), b'previous archive')
                    self.assertEqual(self.checksum.read_bytes(), b'previous checksum')
                    self.assertEqual(sorted(p.name for p in self.archive.parent.iterdir()),
                                     [self.archive.name, self.checksum.name])
                finally:
                    path.write_bytes(data)

    def test_sqlite_notice_requires_the_leading_disclaimer(self):
        self.write('.deps/tiger-sqlite/sqlite3.h', '/* unrelated header */\nThe author disclaims copyright')
        with self.assertRaisesRegex(SystemExit, 'SQLite copyright disclaimer'):
            self.package()
        self.assertFalse(self.archive.exists())


class TigerPackagingCommand(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='codet-tiger-command-')
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)

    def test_external_files_are_mounted_readonly_and_verification_runs_first(self):
        ca = self.root/'trust, with spaces.pem'
        ca.write_text('certificate fixture')
        original = self.root/'actual-notice.txt'
        original.write_text('notice fixture')
        notice = self.root/'selected notice.txt'
        notice.symlink_to(original)
        calls = []
        with patch.object(tiger, 'ROOT', self.root), \
                patch.object(tiger.subprocess, 'check_output', return_value='sha256:fixture\n'), \
                patch.object(tiger.subprocess, 'run', side_effect=lambda command, **kwargs: calls.append(command)), \
                patch.object(sys, 'argv', ['tiger.py', 'package-portable', '--ca-bundle', str(ca),
                                          '--extra-license', str(notice)]):
            tiger.main()
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0][-1], 'scripts/verify-tiger.py')
        command = calls[1]
        self.assertIn('scripts/package-tiger-portable.py', command)
        self.assertEqual(command[command.index('--network')+1], 'none')
        self.assertIn('--read-only', command)
        mounts = {}
        for i, option in enumerate(command):
            if option == '--mount':
                fields = next(csv.reader([command[i+1]]))
                values = dict(f.split('=', 1) for f in fields if '=' in f)
                mounts[values['dst']] = (values['src'], 'readonly' in fields)
        self.assertEqual(mounts['/project'], (str(self.root), True))
        self.assertEqual(mounts['/project/build'], (str(self.root/'build'), False))
        self.assertEqual(mounts['/run/codet-packaging/ca.pem'], (str(ca.resolve()), True))
        target = command[command.index('--extra-license')+1]
        self.assertEqual(pathlib.PurePosixPath(target).name, notice.name)
        self.assertEqual(mounts[target], (str(original.resolve()), True))

    def test_invalid_options_or_files_fail_before_starting_docker(self):
        for options in [[], ['--ca-bundle', str(self.root/'absent.pem')],
                        ['--ca-bundle', str(self.root)]]:
            with self.subTest(options=options), patch.object(tiger, 'ROOT', self.root), \
                    patch.object(tiger, 'docker') as docker, contextlib.redirect_stderr(io.StringIO()), \
                    patch.object(sys, 'argv', ['tiger.py', 'package-portable', *options]):
                with self.assertRaises(SystemExit):
                    tiger.main()
                docker.assert_not_called()
        with patch.object(sys, 'argv', ['tiger.py', 'verify', '--ca-bundle', 'unused']), \
                contextlib.redirect_stderr(io.StringIO()), patch.object(tiger, 'docker') as docker:
            with self.assertRaises(SystemExit):
                tiger.main()
            docker.assert_not_called()

    def test_failed_verification_does_not_start_packaging(self):
        ca = self.root/'ca.pem'
        ca.write_text('fixture')
        with patch.object(tiger, 'ROOT', self.root), \
                patch.object(tiger, 'docker', side_effect=RuntimeError('verification failed')) as docker, \
                patch.object(sys, 'argv', ['tiger.py', 'package-portable', '--ca-bundle', str(ca)]):
            with self.assertRaisesRegex(RuntimeError, 'verification failed'):
                tiger.main()
            self.assertEqual(docker.call_count, 1)


class OpenSSLPrerequisite(unittest.TestCase):
    def test_configured_absolute_executable_generates_certificates_without_path(self):
        executable = openssl_executable()
        with tempfile.TemporaryDirectory() as temporary, \
                patch.dict(os.environ, {'PATH': temporary, 'CODET_TEST_OPENSSL': executable}):
            certificate, key = create_certificate(pathlib.Path(temporary))
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(certificate, key)
            self.assertTrue(certificate.is_file())

    def test_missing_or_invalid_executable_has_clear_prerequisite_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            for selected in ['', str(pathlib.Path(temporary)/'missing-openssl')]:
                with self.subTest(selected=selected), \
                        patch.dict(os.environ, {'PATH': temporary, 'CODET_TEST_OPENSSL': selected}):
                    with self.assertRaisesRegex(RuntimeError, 'Host TLS tests require the openssl executable'):
                        openssl_executable()


if __name__ == '__main__':
    unittest.main()
