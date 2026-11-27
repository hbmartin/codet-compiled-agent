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
import time
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
        self.indexes = self.home / "artifacts" / "indexes"
        self.fork = ("exec", "fork", "parent", "--provider", "ollama", "--json")
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

    def snapshot(self):
        tables = ("sessions", "tools", "provider_parts", "provider_revisions",
                  "session_messages", "session_revisions", "session_references")
        with self.db() as db:
            return {table: db.execute(f"SELECT * FROM {table} ORDER BY rowid").fetchall()
                    for table in tables}

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

    def test_export_recovers_without_reading_or_mutating_corrupt_provider_cache(self):
        self.ok("sessions")
        with self.db() as db:
            db.execute("INSERT INTO provider_parts VALUES('parent',0,'{broken')")
        before = self.snapshot()
        for selector in ("parent", "named parent", "--last"):
            exported = json.loads(self.ok("export", selector, "--output-format", "json"))
            self.assertEqual(exported["id"], "parent")
            self.assertEqual(exported["messages"][0]["text"], "intact transcript")
            tool = exported["messages"][-1]
            self.assertEqual(tool["tool_call_id"], "call")
            self.assertEqual(json.loads(tool["text"])["metadata"]["recovery"], "uncertain")
        self.assertIn("intact transcript", self.ok("export", "--last", "--output-format", "text"))
        for command in (self.fork, ("exec", "resume", "parent", "--provider", "ollama", "continue")):
            strict = self.cli(*command)
            self.assertNotEqual(strict.returncode, 0)
            self.assertIn("Corrupt provider conversation", strict.stderr)
        self.assertEqual(self.snapshot(), before)

    def test_export_reports_corrupt_journal_or_transcript_without_mutation(self):
        self.ok("sessions")
        with self.db() as db:
            db.execute("UPDATE tools SET status='complete',result='{broken'")
        for expected in ("Corrupt completed tool record", "Corrupt session messages"):
            with self.subTest(expected=expected):
                before = self.snapshot()
                exported = self.cli("export", "parent", "--output-format", "json")
                self.assertNotEqual(exported.returncode, 0)
                self.assertIn(expected, exported.stderr)
                self.assertEqual(exported.stdout, "")
                self.assertEqual(self.snapshot(), before)
                with self.db() as db:
                    db.execute("UPDATE session_messages SET payload='{broken' WHERE position=0")

    def index_fixture(self):
        self.ok("sessions")
        self.indexes.mkdir(parents=True)
        ledger = self.indexes / "parent.jsonl"
        ledger.write_text(json.dumps({"version": 2, "kind": "header", "session_id": "parent"})+"\n")
        with self.db() as db:
            db.execute("INSERT INTO session_references VALUES('parent','artifact','inherited')")
            db.execute("""INSERT INTO provider_parts VALUES('parent',0,'{"provider":"ollama"}')""")
        return ledger

    def test_failed_forks_roll_back_database_and_child_ledger_then_retry(self):
        parent = self.index_fixture()
        original = parent.read_bytes()
        before = self.snapshot()
        # Deferred foreign keys fail at commit, after every child write has succeeded.
        with self.db() as db:
            db.execute("""CREATE TABLE deferred_failure(id TEXT REFERENCES sessions(id)
                          DEFERRABLE INITIALLY DEFERRED)""")
        for event, action in [
                ("INSERT ON session_messages", "SELECT RAISE(ABORT,'message failure')"),
                ("INSERT ON provider_parts", "SELECT RAISE(ABORT,'provider failure')"),
                ("INSERT ON session_references", "SELECT RAISE(ABORT,'reference failure')"),
                ("INSERT ON tools", "SELECT RAISE(ABORT,'journal failure')"),
                ("UPDATE OF parent ON sessions", "SELECT RAISE(ABORT,'parent failure')"),
                ("UPDATE OF parent ON sessions", "INSERT INTO deferred_failure VALUES('absent')")]:
            with self.subTest(event=event, action=action):
                with self.db() as db:
                    db.execute(f"CREATE TRIGGER interrupt_fork AFTER {event} BEGIN {action}; END")
                failed = self.cli(*self.fork)
                with self.db() as db:
                    db.execute("DROP TRIGGER interrupt_fork")
                self.assertNotEqual(failed.returncode, 0, failed.stdout)
                expected = ("FOREIGN KEY constraint" if action.startswith("INSERT")
                            else action.split("'")[1])
                self.assertIn(expected, failed.stderr)
                self.assertEqual(self.snapshot(), before)
                self.assertEqual(list(self.indexes.iterdir()), [parent])
                self.assertEqual(parent.read_bytes(), original)
                self.assertEqual(list(self.home.glob(".pending-fork-*")), [])
        # Copying a future ledger writes the child file before format validation fails.
        future_header = original.replace(b'"version": 2', b'"version": 99')
        parent.write_bytes(future_header)
        failed = self.cli(*self.fork)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn("Unsupported artifact index version", failed.stderr)
        self.assertEqual(parent.read_bytes(), future_header)
        self.assertEqual(list(self.indexes.iterdir()), [parent])
        self.assertEqual(self.snapshot(), before)
        parent.write_bytes(original)
        self.ok(*self.fork)
        with self.db() as db:
            child, = db.execute("SELECT id FROM sessions WHERE parent='parent'").fetchone()
            self.assertEqual(db.execute("SELECT status FROM tools WHERE session=?", (child,)).fetchone(),
                             ("uncertain",))
            self.assertEqual(db.execute("SELECT path FROM session_references WHERE session=?", (child,)).fetchone(),
                             ("inherited",))
        self.assertEqual(json.loads((self.indexes / f"{child}.jsonl").read_text())["session_id"], child)
        self.assertEqual(parent.read_bytes(), original)

        # A crash just after commit leaves a marker for an already visible child.
        marker = self.home / f".pending-fork-{child}"
        marker.write_text("relative artifacts")
        self.ok("sessions")
        self.assertFalse(marker.exists())
        self.assertTrue((self.indexes / f"{child}.jsonl").is_file())

    def test_interrupted_stream_only_cleans_child_staging_under_matching_root(self):
        parent = self.index_fixture()
        child = str(uuid.uuid4())
        marker = self.home / f".pending-fork-{child}"
        staging = self.indexes / f".fork-{child}"
        unfinished = staging / "indexes" / ".codet-write-during-copy"
        unfinished.parent.mkdir(parents=True)
        unfinished.write_text("partial observation")
        other_writer = self.indexes / ".codet-write-other"
        other_writer.write_text("another active writer")
        marker.write_text("relative other-artifacts")
        self.ok("sessions")
        self.assertTrue(marker.is_file())
        self.assertTrue(unfinished.is_file())
        marker.write_text("relative artifacts")
        self.ok("sessions")
        self.assertFalse(marker.exists())
        self.assertFalse(staging.exists())
        self.assertTrue(parent.is_file())
        self.assertEqual(other_writer.read_text(), "another active writer")

    def test_killed_fork_removes_only_uncommitted_child_ledger_on_reopen(self):
        parent = self.index_fixture()
        original = parent.read_bytes()
        with self.db() as db:
            db.execute("BEGIN IMMEDIATE")
            with subprocess.Popen([BINARY, *self.fork], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  env=self.env, cwd=self.root) as child:
                try:
                    child.stdin.close()
                    child.stdin = None
                    deadline = time.monotonic() + 4
                    pending = []
                    while not pending and child.poll() is None and time.monotonic() < deadline:
                        pending = [path for path in self.indexes.glob("*.jsonl") if path != parent]
                        if not pending:
                            time.sleep(0.01)
                    self.assertEqual(len(pending), 1)
                    self.ok("sessions")  # The owner is live and its ledger is still needed.
                    self.assertTrue(pending[0].exists())
                finally:
                    child.kill()
                    child.communicate(timeout=5)
        self.ok("sessions")
        self.assertEqual(list(self.indexes.iterdir()), [parent])
        self.assertEqual(parent.read_bytes(), original)
        with self.db() as db:
            self.assertEqual(db.execute("SELECT id FROM sessions").fetchall(), [("parent",)])


if __name__ == "__main__":
    unittest.main()
