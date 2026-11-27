"""Exercise migration, fork publication, and read-only export with private state."""

import contextlib
import fcntl
import json
import os
import resource
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import uuid
from pathlib import Path

from hermetic import environment

BINARY = os.path.abspath(sys.argv.pop(1))


class StateRecovery(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / "state"
        self.home.mkdir()
        self.env = environment(self.root)
        self.env["CODET_HOME"] = str(self.home)
        self.database = self.home / "state.sqlite3"
        self.messages = [
            {"role": "user", "text": "intact transcript"},
            {"role": "assistant", "calls": [{"id": "call", "name": "read_file", "arguments": {}}]},
        ]
        with self.db() as db:
            db.executescript("""
                CREATE TABLE sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent TEXT,
                    updated INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle');
                CREATE TABLE tools(session TEXT NOT NULL,call_id TEXT NOT NULL,name TEXT NOT NULL,
                    args TEXT NOT NULL,result TEXT,status TEXT NOT NULL,PRIMARY KEY(session,call_id));
                PRAGMA user_version=1;
            """)
            db.execute("INSERT INTO sessions VALUES('parent','named parent',NULL,1,?,'idle')",
                       (json.dumps(self.messages),))
            db.execute("INSERT INTO tools VALUES('parent','call','read_file','{}',NULL,'running')")

    @contextlib.contextmanager
    def db(self, path=None):
        with contextlib.closing(sqlite3.connect(path or self.database)) as connection, connection:
            yield connection

    def cli(self, *args, **options):
        return subprocess.run([BINARY, *args], env=self.env, cwd=self.root,
                              input="", capture_output=True, text=True,
                              timeout=15, **options)

    def ok(self, *args, **options):
        result = self.cli(*args, **options)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_migrate_many_legacy_locks_with_limited_descriptors(self):
        directory = self.home / "session-locks"
        directory.mkdir()
        for index in range(100):
            (directory / f"legacy-{index}.lock").touch()
        with (directory / "legacy-95.lock").open("r+") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            refused = self.cli("sessions")
            self.assertNotEqual(refused.returncode, 0)
            self.assertIn("Stop active Codet sessions", refused.stderr)
            self.assertEqual(list(self.home.glob("state.sqlite3.pre-v8-*")), [])

        def limit_descriptors():
            resource.setrlimit(resource.RLIMIT_NOFILE, (48, 48))

        self.ok("sessions", preexec_fn=limit_descriptors)
        self.assertIn("intact transcript", self.ok("export", "--last", "--output-format", "text"))
        (backup,) = self.home.glob("state.sqlite3.pre-v8-*")
        with self.db(backup) as db:
            self.assertEqual(db.execute("PRAGMA user_version").fetchone(), (1,))
            self.assertEqual(json.loads(db.execute("SELECT messages FROM sessions").fetchone()[0]),
                             self.messages)

        self.assertEqual(str(uuid.UUID(backup.name.removeprefix("state.sqlite3.pre-v8-"))),
                         backup.name.removeprefix("state.sqlite3.pre-v8-"))
        # Restore the backup as the sole database in another home and migrate it again.
        restored = self.root / "restored"
        restored.mkdir()
        shutil.copy2(backup, restored / "state.sqlite3")
        self.env["CODET_HOME"] = str(restored)
        exported = json.loads(self.ok("export", "--last", "--output-format", "json"))
        self.assertEqual(exported["messages"][0]["text"], "intact transcript")
        self.assertEqual(json.loads(exported["messages"][-1]["text"])["metadata"]["recovery"],
                         "uncertain")


if __name__ == "__main__":
    unittest.main()
