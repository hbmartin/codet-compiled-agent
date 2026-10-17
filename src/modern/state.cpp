#include "codet/artifacts.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <set>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

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
    // SQLite requires this destructor sentinel to copy temporary string bytes.
    // NOLINTNEXTLINE(performance-no-int-to-ptr): SQLITE_TRANSIENT is the documented C API sentinel.
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
  fs::path home, artifact_root;
  std::map<std::string, std::weak_ptr<void>> owners;
  ~Impl() {
    if (db)
      sqlite3_close(db);
  }
};
StateStore::StateStore(const fs::path &home, const fs::path &artifact_root)
    : impl_(std::make_unique<Impl>()) {
  impl_->home = home;
  impl_->artifact_root = artifact_root.empty() ? home / "artifacts" : artifact_root;
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
  if (n > 3)
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
  if(n<3) sql(impl_->db,"PRAGMA user_version=3;");
}
StateStore::~StateStore() = default;
std::shared_ptr<void> StateStore::acquire(const std::string &id) {
  if (id.empty() || id.size() > 128 ||
      id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
          std::string::npos)
    throw Error("Invalid session lock identity");
  for (auto it = impl_->owners.begin(); it != impl_->owners.end();) {
    if (it->second.expired())
      it = impl_->owners.erase(it);
    else
      ++it;
  }
  if (const auto found = impl_->owners.find(id); found != impl_->owners.end())
    if (auto held = found->second.lock())
      return held;
  const auto directory = impl_->home / "session-locks";
  if (fs::is_symlink(directory))
    throw Error("Session lock directory must not be a symlink");
  fs::create_directories(directory);
  struct Lease {
    int fd = -1;
    ~Lease() {
      if (fd >= 0)
        ::close(fd);
    }
  };
  auto lease = std::make_shared<Lease>();
  lease->fd = ::open((directory / (id + ".lock")).c_str(),
                     O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK, 0600);
  struct stat status {};
  if (lease->fd < 0 || ::fstat(lease->fd, &status) || !S_ISREG(status.st_mode) ||
      ::fchmod(lease->fd, 0600) || ::fcntl(lease->fd, F_SETFD, FD_CLOEXEC) < 0)
    throw Error("Cannot open private session ownership lock");
  if (::flock(lease->fd, LOCK_EX | LOCK_NB)) {
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      throw Error("Session is in use by another process: " + id);
    throw Error("Cannot acquire session ownership lock: " + id);
  }
  impl_->owners[id] = lease;
  return lease;
}
Session StateStore::create(const std::string &name) {
  Session s{unique_id(), name, {}};
  save(s);
  return s;
}
Session StateStore::load(const std::string &selector, bool repair) {
  Statement q(impl_->db, "SELECT id FROM sessions WHERE id=? OR name=? "
                         "ORDER BY (id=?) DESC,updated DESC,rowid DESC LIMIT 1");
  q.text(1, selector);
  q.text(2, selector);
  q.text(3, selector);
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("Session not found: " + selector, 2);
  return load_id(column(q.p, 0), repair);
}
Session StateStore::load_id(const std::string &id, bool repair) {
  Statement q(impl_->db, "SELECT id,name,messages,status,provider,model FROM sessions WHERE id=?");
  q.text(1, id);
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
  portable_messages(s.messages, s.id);
  ensure_message_ids(s.messages);
  if (repair)
    reconcile(s, false);
  return s;
}
void StateStore::reconcile(Session &session, bool persist) {
  portable_messages(session.messages, session.id);
  ensure_message_ids(session.messages);
  auto owner = persist ? acquire(session.id) : std::shared_ptr<void>{};
  std::map<std::string, ToolCall> calls;
  std::map<std::string, Message> answers;
  for (const auto &message : session.messages) {
    if (message.role == "tool") {
      if (message.tool_call_id.empty() || !answers.emplace(message.tool_call_id, message).second)
        throw Error("Ambiguous duplicate tool results in session");
    }
    for (const auto &call : message.calls)
      if (message.role != "assistant" || call.id.empty() || !calls.emplace(call.id, call).second)
        throw Error("Ambiguous duplicate tool call ids in session");
  }
  for (const auto &answer : answers)
    if (!calls.count(answer.first))
      throw Error("Orphan tool result in session");
  auto previous = session.messages;
  sql(impl_->db, persist ? "BEGIN IMMEDIATE" : "BEGIN");
  try {
    Json stored_messages;
    if (persist) {
      Statement current(impl_->db, "SELECT messages FROM sessions WHERE id=?");
      current.text(1, session.id);
      if (sqlite3_step(current.p) != SQLITE_ROW)
        throw Error("Session no longer exists");
      auto stored = Json::parse(column(current.p, 0)).get<std::vector<Message>>();
      portable_messages(stored, session.id);
      ensure_message_ids(stored);
      stored_messages = Json(stored);
      const auto non_tools = [](const Json &messages) {
        Json history = Json::array();
        for (const auto &message : messages)
          if (message.at("role") != "tool")
            history.push_back(message);
        return history;
      };
      if (non_tools(stored_messages) != non_tools(Json(session.messages)))
        throw Error("Session modified by another process: reload before recovery");
    }
    const auto synthetic = [](const Message &message) {
      const auto value = Json::parse(message.text, nullptr, false);
      if (!value.is_object() || !value.contains("metadata") || !value["metadata"].is_object())
        return false;
      const auto status = value["metadata"].value("recovery", Json());
      return status == "uncertain" || status == "not_started";
    };
    std::vector<Message> repaired;
    std::map<std::string, std::pair<Message, std::string>> recoveries;
    for (const auto &message : session.messages) {
      if (message.role == "tool")
        continue;
      repaired.push_back(message);
      for (const auto &call : message.calls) {
        Statement record(impl_->db,
                         "SELECT result,status,name,args FROM tools WHERE session=? AND call_id=?");
        record.text(1, session.id);
        record.text(2, call.id);
        const int step = sqlite3_step(record.p);
        if (step != SQLITE_ROW && step != SQLITE_DONE)
          throw Error("Cannot read tool journal");
        const bool recorded = step == SQLITE_ROW;
        const auto status = recorded ? column(record.p, 1) : "not_started";
        if (recorded) {
          try {
            if (column(record.p, 2) != call.name ||
                Json::parse(column(record.p, 3)) != call.arguments)
              throw Error("Ambiguous journaled tool call id");
          } catch (const Json::exception &) {
            throw Error("Corrupt journaled tool arguments");
          }
        }
        Message result;
        if (recorded && status == "complete") {
          try {
            result = Json::parse(column(record.p, 0)).get<Message>();
            if (result.role != "tool" || result.tool_call_id != call.id)
              throw Error("Mismatched completed tool record");
          } catch (const std::exception &) {
            throw Error("Corrupt completed tool record");
          }
        } else if (recorded && (status == "uncertain" || status == "not_started")) {
          try {
            result = Json::parse(column(record.p, 0)).get<Message>();
            if (result.role != "tool" || result.tool_call_id != call.id)
              throw Error("Mismatched recovery record");
          } catch (const std::exception &) {
            throw Error("Corrupt recovered tool record");
          }
        } else if (answers.count(call.id) && !synthetic(answers.at(call.id))) {
          result = answers.at(call.id);
          if (!recorded)
            recoveries.emplace(call.id, std::make_pair(result, "complete"));
        } else if (!recorded && answers.count(call.id)) {
          // A branch inherits what is known about effects, not the parent's execution ownership.
          result = answers.at(call.id);
          const auto prior =
              Json::parse(result.text).at("metadata").at("recovery").get<std::string>();
          recoveries.emplace(call.id, std::make_pair(result, prior));
        } else {
          const auto recovery = recorded ? "uncertain" : "not_started";
          result.role = "tool";
          result.tool_call_id = call.id;
          result.text =
              Json({{"content",
                     recorded ? "Interrupted: completion unknown. Inspect actual state before "
                                "another action; this tool was not replayed."
                              : "Interrupted: this tool was not started and was not replayed."},
                    {"is_error", true},
                    {"metadata", {{"recovery", recovery}}}})
                  .dump();
          recoveries.emplace(call.id, std::make_pair(result, recovery));
        }
        // Presentation state is durable separately from the canonical completed payload.
        if (answers.count(call.id)) {
          const auto &answer = answers.at(call.id);
          if (!answer.message_id.empty())
            result.message_id = answer.message_id;
          if (!answer.result_file.empty())
            result.result_file = answer.result_file;
          result.images_archived = answer.images_archived;
        }
        if (result.images_archived)
          result.images.clear();
        repaired.push_back(result);
      }
    }
    portable_messages(repaired, session.id);
    ensure_message_ids(repaired);
    if (Json(repaired) == Json(session.messages) && recoveries.empty() &&
        (!persist || Json(repaired) == stored_messages)) {
      sql(impl_->db, "COMMIT");
      return;
    }
    if (persist)
      for (const auto &entry : recoveries) {
        const auto &call = calls.at(entry.first);
        Statement write(
            impl_->db,
            "INSERT INTO tools(session,call_id,name,args,result,status) VALUES(?,?,?,?,?,?) "
            "ON CONFLICT(session,call_id) DO UPDATE SET "
            "result=excluded.result,status=excluded.status "
            "WHERE tools.status!='complete'");
        write.text(1, session.id);
        write.text(2, call.id);
        write.text(3, call.name);
        write.text(4, safe_json(call.arguments).dump());
        write.text(5, Json(entry.second.first).dump());
        write.text(6, entry.second.second);
        write.run();
      }
    session.messages = std::move(repaired);
    if (persist)
      save(session);
    sql(impl_->db, "COMMIT");
  } catch (...) {
    session.messages.swap(previous);
    if (!sqlite3_get_autocommit(impl_->db))
      (void)sqlite3_exec(impl_->db, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}
Session StateStore::last() {
  Statement q(impl_->db, "SELECT id FROM sessions ORDER BY updated DESC,rowid DESC LIMIT 1");
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("No saved sessions", 2);
  return load_id(column(q.p, 0));
}
Session StateStore::fork(const Session &parent) {
  auto recovered = parent;
  portable_messages(recovered.messages, parent.id);
  ensure_message_ids(recovered.messages);
  reconcile(recovered, false);
  Session s{unique_id(), parent.name.empty() ? "" : parent.name + " (fork)", recovered.messages};
  s.provider = parent.provider;
  s.model = parent.model;
  auto owner = acquire(s.id);
  NativeFileSystem files;
  if (std::any_of(s.messages.begin(), s.messages.end(),
                  [](const auto &m) { return !m.artifact_index.empty(); }) &&
      !fs::exists(artifact_index_path(impl_->artifact_root, parent.id)))
    throw Error("Historical image index missing; restore the active artifact root");
  copy_image_history(impl_->artifact_root, files, parent.id, s.id);
  for (auto &message : s.messages)
    if (!message.artifact_index.empty())
      message.artifact_index = artifact_index_reference(s.id);
  save(s);
  reconcile(s);
  Statement q(impl_->db, "UPDATE sessions SET parent=? WHERE id=?");
  q.text(1, parent.id);
  q.text(2, s.id);
  q.run();
  return s;
}
void StateStore::save(const Session &s) {
  auto owner = acquire(s.id);
  Statement q(impl_->db, "INSERT INTO sessions(id,name,updated,messages,provider,model) "
                         "VALUES(?,?,strftime('%s','now'),?,?,?) ON CONFLICT(id) DO UPDATE SET "
                         "name=excluded.name,updated=excluded.updated,messages=excluded.messages,"
                         "provider=excluded.provider,model=excluded.model");
  q.text(1, s.id);
  q.text(2, s.name);
  auto messages = s.messages;
  portable_messages(messages, s.id);
  ensure_message_ids(messages);
  q.text(3, safe_json(Json(messages)).dump());
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
  auto owner = acquire(id);
  Statement q(impl_->db, "UPDATE sessions SET status='running' WHERE id=?");
  q.text(1, id);
  q.run();
}
void StateStore::finish_turn(const std::string &id, const std::string &status) {
  auto owner = acquire(id);
  Statement q(impl_->db, "UPDATE sessions SET status=? WHERE id=?");
  q.text(1, status);
  q.text(2, id);
  q.run();
}
void StateStore::tool_started(const std::string &id, const ToolCall &t) {
  auto owner = acquire(id);
  Statement q(
      impl_->db,
      "INSERT OR IGNORE INTO tools(session,call_id,name,args,status) VALUES(?,?,?,?,'running')");
  q.text(1, id);
  q.text(2, t.id);
  q.text(3, t.name);
  q.text(4, safe_json(t.arguments).dump());
  q.run();
  if (sqlite3_changes(impl_->db) != 1)
    throw Error("Session modified by another process: tool call already recorded");
}
void StateStore::tool_finished(const std::string &id, const ToolCall &t, const Json &result) {
  auto owner = acquire(id);
  Statement q(impl_->db, "UPDATE tools SET status='complete',result=? WHERE session=? AND "
                         "call_id=? AND status='running'");
  auto message = result.get<Message>();
  std::vector<Message> portable{message};
  portable_messages(portable, id);
  ensure_message_ids(portable);
  q.text(1, safe_json(Json(portable.front())).dump());
  q.text(2, id);
  q.text(3, t.id);
  q.run();
  if (sqlite3_changes(impl_->db) != 1)
    throw Error("Session modified by another process: tool outcome is no longer pending");
}
} // namespace codet
