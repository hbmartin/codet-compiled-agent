#include "codet/core.hpp"
#include <set>
#include <sqlite3.h>
#include <sys/stat.h>

namespace codet {
namespace {
struct Statement {
  sqlite3_stmt *p = nullptr;
  Statement(sqlite3 *db, const char *sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &p, nullptr) != SQLITE_OK)
      throw Error(sqlite3_errmsg(db));
  }
  ~Statement() {
    sqlite3_finalize(p);
  }
  void text(int n, const std::string &v) {
    if (sqlite3_bind_text(p, n, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK)
      throw Error("SQLite bind failed");
  }
  void run() {
    if (sqlite3_step(p) != SQLITE_DONE)
      throw Error(sqlite3_errmsg(sqlite3_db_handle(p)));
  }
};
void sql(sqlite3 *db, const char *s) {
  if (sqlite3_exec(db, s, nullptr, nullptr, nullptr) != SQLITE_OK)
    throw Error(sqlite3_errmsg(db));
}
std::string column(sqlite3_stmt *p, int n) {
  auto *s = sqlite3_column_text(p, n);
  return s ? reinterpret_cast<const char *>(s) : "";
}
} // namespace
struct StateStore::Impl {
  sqlite3 *db = nullptr;
  ~Impl() {
    if (db)
      sqlite3_close(db);
  }
};
StateStore::StateStore(const fs::path &home) : impl_(std::make_unique<Impl>()) {
  if (fs::is_symlink(home))
    throw Error("State directory must not be a symlink");
  fs::create_directories(home);
  if (chmod(home.c_str(), 0700) != 0)
    throw Error("Cannot protect state directory");
  const auto path = home / "state.sqlite3";
  if (fs::is_symlink(path))
    throw Error("State database must not be a symlink");
  if (sqlite3_open(path.c_str(), &impl_->db) != SQLITE_OK)
    throw Error("Cannot open session database");
  if (chmod(path.c_str(), 0600) != 0)
    throw Error("Cannot protect session database");
  sqlite3_busy_timeout(impl_->db, 5000);
  Statement version(impl_->db, "PRAGMA user_version");
  if (sqlite3_step(version.p) != SQLITE_ROW)
    throw Error("Cannot read state schema");
  const int n = sqlite3_column_int(version.p, 0);
  if (n > 2)
    throw Error("Session database was created by a newer Codet version");
  sql(impl_->db, "PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
  sql(impl_->db,
      "CREATE TABLE IF NOT EXISTS sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent "
      "TEXT,updated INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle');"
      "CREATE TABLE IF NOT EXISTS tools(session TEXT NOT NULL,call_id TEXT NOT NULL,name TEXT NOT "
      "NULL,args TEXT NOT NULL,result TEXT,status TEXT NOT NULL,PRIMARY "
      "KEY(session,call_id),FOREIGN KEY(session) REFERENCES sessions(id));");
  if (n < 2)
    sql(impl_->db,
        "BEGIN IMMEDIATE; ALTER TABLE sessions ADD COLUMN provider TEXT NOT NULL DEFAULT '';ALTER "
        "TABLE sessions ADD COLUMN model TEXT NOT NULL DEFAULT '';PRAGMA user_version=2;COMMIT;");
}
StateStore::~StateStore() = default;
Session StateStore::create(const std::string &name) {
  Session s{unique_id(), name, {}};
  save(s);
  return s;
}
Session StateStore::load(const std::string &id) {
  Statement q(impl_->db, "SELECT id,name,messages,status,provider,model FROM sessions WHERE id=? "
                         "OR name=? ORDER BY updated DESC LIMIT 1");
  q.text(1, id);
  q.text(2, id);
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("Session not found: " + id, 2);
  Session s{column(q.p, 0), column(q.p, 1), {}};
  s.provider = column(q.p, 4);
  s.model = column(q.p, 5);
  try {
    s.messages = Json::parse(column(q.p, 2)).get<std::vector<Message>>();
  } catch (const std::exception &) {
    throw Error("Corrupt session messages");
  }
  if (column(q.p, 3) != "complete" && column(q.p, 3) != "idle") {
    std::vector<ToolCall> pending;
    std::set<std::string> answered;
    for (const auto &m : s.messages)
      if (m.role == "tool")
        answered.insert(m.tool_call_id);
    for (const auto &m : s.messages)
      for (const auto &t : m.calls)
        if (!answered.count(t.id))
          pending.push_back(t);
    for (const auto &t : pending) {
      Statement tool(impl_->db, "SELECT result,status FROM tools WHERE session=? AND call_id=?");
      tool.text(1, s.id);
      tool.text(2, t.id);
      if (sqlite3_step(tool.p) == SQLITE_ROW && column(tool.p, 1) == "complete") {
        try {
          s.messages.push_back(Json::parse(column(tool.p, 0)).get<Message>());
          continue;
        } catch (const std::exception &) {
          throw Error("Corrupt completed tool record");
        }
      }
      Message missing;
      missing.role = "tool";
      missing.tool_call_id = t.id;
      missing.text = "Interrupted: completion unknown. Inspect actual state before taking another "
                     "action. This tool was not replayed.";
      s.messages.push_back(missing);
    }
    // Never resume by issuing an unrecorded external action a second time.
    Message note;
    note.role = "system";
    note.text = "Previous run did not complete. Check actual files and process state before taking "
                "further action; previous unfinished tools were not replayed.";
    s.messages.push_back(note);
  }
  return s;
}
Session StateStore::last() {
  Statement q(impl_->db, "SELECT id FROM sessions ORDER BY updated DESC,rowid DESC LIMIT 1");
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("No saved sessions", 2);
  return load(column(q.p, 0));
}
Session StateStore::fork(const Session &parent) {
  Session s{unique_id(), parent.name.empty() ? "" : parent.name + " (fork)", parent.messages};
  s.provider = parent.provider;
  s.model = parent.model;
  save(s);
  Statement q(impl_->db, "UPDATE sessions SET parent=? WHERE id=?");
  q.text(1, parent.id);
  q.text(2, s.id);
  q.run();
  return s;
}
void StateStore::save(const Session &s) {
  Statement q(impl_->db, "INSERT INTO sessions(id,name,updated,messages,provider,model) "
                         "VALUES(?,?,strftime('%s','now'),?,?,?) ON CONFLICT(id) DO UPDATE SET "
                         "name=excluded.name,updated=excluded.updated,messages=excluded.messages,"
                         "provider=excluded.provider,model=excluded.model");
  q.text(1, s.id);
  q.text(2, s.name);
  q.text(3, Json(s.messages).dump());
  q.text(4, s.provider);
  q.text(5, s.model);
  q.run();
}
Json StateStore::list() {
  Statement q(
      impl_->db,
      "SELECT id,name,parent,updated,status FROM sessions ORDER BY updated DESC,rowid DESC");
  Json out = Json::array();
  int result;
  while ((result = sqlite3_step(q.p)) == SQLITE_ROW)
    out.push_back({{"id", column(q.p, 0)},
                   {"name", column(q.p, 1)},
                   {"parent", column(q.p, 2)},
                   {"updated", sqlite3_column_int64(q.p, 3)},
                   {"status", column(q.p, 4)}});
  if (result != SQLITE_DONE)
    throw Error("Cannot list sessions");
  return out;
}
void StateStore::begin_turn(const std::string &id) {
  Statement q(impl_->db, "UPDATE sessions SET status='running' WHERE id=?");
  q.text(1, id);
  q.run();
}
void StateStore::finish_turn(const std::string &id, const std::string &status) {
  Statement q(impl_->db, "UPDATE sessions SET status=? WHERE id=?");
  q.text(1, status);
  q.text(2, id);
  q.run();
}
void StateStore::tool_started(const std::string &id, const ToolCall &t) {
  Statement q(impl_->db,
              "INSERT INTO tools(session,call_id,name,args,status) VALUES(?,?,?,?,'running')");
  q.text(1, id);
  q.text(2, t.id);
  q.text(3, t.name);
  q.text(4, t.arguments.dump());
  q.run();
}
void StateStore::tool_finished(const std::string &id, const ToolCall &t, const Json &result) {
  Statement q(impl_->db,
              "UPDATE tools SET status='complete',result=? WHERE session=? AND call_id=?");
  q.text(1, result.dump());
  q.text(2, id);
  q.text(3, t.id);
  q.run();
}
} // namespace codet
