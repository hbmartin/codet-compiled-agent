import importlib.util
import io
import pathlib
import struct
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = pathlib.Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
from verified_sources import extract_verified, file_hash, tree_hashes
spec = importlib.util.spec_from_file_location('verify_tiger', SCRIPTS / 'verify-tiger.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class BuildIntegrity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)

    def archive(self, version, members):
        archive = self.root / (version + '.tar.gz')
        with tarfile.open(archive, 'w:gz') as package:
            for name, content in members.items():
                item = tarfile.TarInfo(version + '/' + name)
                data = content.encode()
                item.size = len(data)
                package.addfile(item, io.BytesIO(data))
        return archive

    def test_clean_extraction_removes_mutations_and_previous_version_files(self):
        target = self.root / 'source'
        first = self.archive('v1', {'code.c': 'first', 'obsolete.c': 'old'})
        extract_verified(first, file_hash(first), target)
        (target / 'code.c').write_text('modified extracted cache')
        second = self.archive('v2', {'code.c': 'new'})
        extract_verified(second, file_hash(second), target)
        self.assertEqual((target / 'code.c').read_text(), 'new')
        self.assertFalse((target / 'obsolete.c').exists())
        before = tree_hashes(target)
        with self.assertRaises(ValueError):
            extract_verified(first, '0' * 64, target)
        self.assertEqual(tree_hashes(target), before)

    def test_archive_escape_and_symlink_destinations_fail(self):
        archive = self.archive('v1', {'../escaped': 'bad'})
        with self.assertRaises(ValueError):
            extract_verified(archive, file_hash(archive), self.root / 'source')
        valid = self.archive('v2', {'code.c': 'code'})
        destination = self.root / 'link'
        destination.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(ValueError):
            extract_verified(valid, file_hash(valid), destination)

    def runtime(self):
        for name in ['lib/libcurl.dylib', 'lib/libsqlite3.dylib', 'lib/libz.dylib',
                     'lib/libncurses.dylib', 'lib/libedit.dylib',
                     'libexec/openssl3/lib/libssl.dylib', 'libexec/openssl3/lib/libcrypto.dylib']:
            path = self.root / 'opt/tigr' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(struct.pack('>II', 0xfeedface, 18))

    def test_empty_or_missing_runtime_libraries_fail(self):
        with self.assertRaises(SystemExit):
            verify.runtime_libraries(self.root, '/opt/tigr')
        self.runtime()
        (self.root / 'opt/tigr/lib/libedit.dylib').unlink()
        with self.assertRaises(SystemExit):
            verify.runtime_libraries(self.root, '/opt/tigr')

    def test_runtime_closure_architecture_and_links(self):
        self.runtime()
        with patch.object(verify, 'output', return_value='library:\n /usr/lib/libSystem.B.dylib (compatibility version 1.0)\n'):
            self.assertEqual(len(verify.runtime_libraries(self.root, '/opt/tigr')), 7)
            with self.assertRaises(SystemExit):
                verify.runtime_libraries(self.root, '/opt/tigr', self.root / 'missing-sdk')
        with patch.object(verify, 'output', return_value='library:\n /opt/tigr/lib/missing.dylib (compatibility version 1.0)\n'):
            with self.assertRaises(SystemExit):
                verify.runtime_libraries(self.root, '/opt/tigr')
        library = self.root / 'opt/tigr/lib/libedit.dylib'
        library.write_bytes(struct.pack('>II', 0xfeedface, 7))
        with patch.object(verify, 'output', return_value='library:\n'):
            with self.assertRaises(SystemExit):
                verify.runtime_libraries(self.root, '/opt/tigr')
        library.unlink()
        library.symlink_to('missing.dylib')
        with self.assertRaises(SystemExit):
            verify.runtime_libraries(self.root, '/opt/tigr')


if __name__ == '__main__':
    unittest.main()
