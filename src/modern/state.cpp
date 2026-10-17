#include "codet/artifacts.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <set>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace codet {
namespace {
Json stored_json(const std::string &bytes) {
  // The JSON lexer treats NUL as EOF even when the string has trailing bytes.
  if (bytes.find('\0') != std::string::npos)
    throw Error("Stored JSON contains an embedded NUL");
  return Json::parse(bytes);
}
constexpr size_t provider_cache_limit = size_t{8} * 1024 * 1024;
[[noreturn]] void corrupt_provider() {
  throw Error(
      "Corrupt provider conversation cache; transcript preserved; explicit repair required");
}
Json provider_json(const std::string &text) {
  try {
    return stored_json(text);
  } catch (const Json::exception &) {
    corrupt_provider();
  } catch (const Error &) {
    corrupt_provider();
  }
}
void validate_provider_part(const Json &part) {
  if (!part.is_object() || !part.contains("wire") || !part.at("wire").is_array() ||
      (part.contains("source") && !part.at("source").is_string()))
    corrupt_provider();
  for (const auto &message : part.at("wire")) {
    if (!message.is_object() || !message.contains("role") || !message.at("role").is_string() ||
        !message.contains("content") ||
        (!message.at("content").is_array() && !message.at("content").is_string()) ||
        (message.at("role") == "assistant" && message.at("content").empty()))
      corrupt_provider();
  }
}
const Json &provider_messages(const Json &state) {
  static const Json empty = Json::array();
  if (state.is_null())
    return empty;
  if (!state.is_object())
    corrupt_provider();
  const auto found = state.find("messages");
  if (found == state.end())
    return empty;
  if (!found->is_array())
    corrupt_provider();
  return *found;
}
Json provider_header(const Json &state) {
  if (state.is_null())
    return nullptr;
  if (!state.is_object())
    corrupt_provider();
  for (const auto *key : {"provider", "model", "account", "endpoint", "artifacts", "captures",
                          "system", "latest_system"})
    if (state.contains(key) && !state.at(key).is_string())
      corrupt_provider();
  if (state.contains("tools") && !state.at("tools").is_array())
    corrupt_provider();
  Json header = Json::object();
  for (auto i = state.begin(); i != state.end(); ++i)
    if (i.key() != "messages")
      header[i.key()] = i.value();
  return header;
}
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
struct Transaction {
  sqlite3 *db;
  bool owner, finished = false;
  explicit Transaction(sqlite3 *connection)
      : db(connection), owner(sqlite3_get_autocommit(connection) != 0) {
    if (owner)
      sql(db, "BEGIN IMMEDIATE");
  }
  ~Transaction() {
    if (owner && !finished)
      (void)sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
  }
  void commit() {
    if (owner)
      sql(db, "COMMIT");
    finished = true;
  }
};
std::string column(sqlite3_stmt *p, int n) {
  auto *s = sqlite3_column_text(p, n);
  return s ? std::string(reinterpret_cast<const char *>(s),
                         static_cast<size_t>(sqlite3_column_bytes(p, n)))
           : std::string{};
}
} // namespace
struct StateStore::Impl {
  sqlite3 *db = nullptr;
  fs::path home, artifact_root;
  std::map<std::string, std::weak_ptr<void>> owners;
  std::map<std::string, std::set<std::string>> known_references;
  std::string cached_session;
  Json cached_provider = nullptr;
  size_t cached_bytes = 0, pending_provider_bytes = 0;
  void cache_provider(const Session &session, size_t bytes) {
    cached_session.clear();
    cached_provider = nullptr;
    cached_bytes = 0;
    if (bytes <= provider_cache_limit) {
      cached_provider = session.provider_state;
      cached_session = session.id;
      cached_bytes = bytes;
    }
  }
  std::string durability;
  uint64_t serialized = 0;
  ~Impl() {
    if (db)
      sqlite3_close(db);
  }
};
StateStore::StateStore(const fs::path &home, const fs::path &artifact_root,
                       const std::string &durability)
    : impl_(std::make_unique<Impl>()) {
  impl_->home = home;
  impl_->durability = durability;
  if (durability != "durable" && durability != "fast")
    throw Error("Invalid durability mode", 2);
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
  sql(impl_->db, "PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
  const auto version = [&] {
    Statement query(impl_->db, "PRAGMA user_version");
    if (sqlite3_step(query.p) != SQLITE_ROW)
      throw Error("Cannot read state schema");
    const auto value = sqlite3_column_int(query.p, 0);
    if (value > 6)
      throw Error("Session database was created by a newer Codet version");
    return value;
  };
  struct MigrationLocks {
    std::vector<int> descriptors;
    ~MigrationLocks() {
      for (int fd : descriptors)
        ::close(fd);
    }
  } migration_locks;
  const auto acquire_migration = [&] {
    if (!migration_locks.descriptors.empty())
      return;
    const auto migration_path = home / ".migration.lock";
    const int migration_fd =
        ::open(migration_path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK, 0600);
    struct stat migration_stat{};
    if (migration_fd < 0)
      throw Error("Cannot open database migration lock");
    migration_locks.descriptors.push_back(migration_fd);
    if (::fstat(migration_fd, &migration_stat) || !S_ISREG(migration_stat.st_mode) ||
        migration_stat.st_uid != ::geteuid() || ::fchmod(migration_fd, 0600) ||
        ::fcntl(migration_fd, F_SETFD, FD_CLOEXEC) < 0)
      throw Error("Database migration lock must be an owned regular file");
    if (::flock(migration_fd, LOCK_EX)) {
      if (errno == EINTR)
        throw Error("Interrupted waiting for database migration", 130);
      throw Error("Cannot acquire database migration lock");
    }
  };
  if (version() < 6) {
    acquire_migration();
    const auto old_version = version();
    if (old_version > 0 && old_version < 6) {
      const auto locks = home / "session-locks";
      if (fs::exists(locks))
        for (const auto &entry : fs::directory_iterator(locks)) {
          const int fd = ::open(entry.path().c_str(), O_RDWR | O_NOFOLLOW | O_NONBLOCK);
          if (fd < 0)
            throw Error("Cannot inspect session ownership before migration");
          const bool busy = ::flock(fd, LOCK_EX | LOCK_NB) != 0;
          if (busy) {
            ::close(fd);
            throw Error("Stop active Codet sessions before upgrading the database");
          }
          migration_locks.descriptors.push_back(fd);
        }
      const auto target = home / ("state.sqlite3.pre-v6-" + unique_id());
      sqlite3 *backup_db = nullptr;
      if (sqlite3_open(target.c_str(), &backup_db) != SQLITE_OK) {
        if (backup_db)
          sqlite3_close(backup_db);
        throw Error("Cannot create migration backup");
      }
      const auto protection = ::chmod(target.c_str(), 0600);
      auto *backup = sqlite3_backup_init(backup_db, "main", impl_->db, "main");
      const int copied = backup ? sqlite3_backup_step(backup, -1) : SQLITE_ERROR;
      if (backup)
        sqlite3_backup_finish(backup);
      sqlite3_close(backup_db);
      if (protection || copied != SQLITE_DONE)
        throw Error("Cannot complete migration backup");
    }
    if (version() < 6) {
      Transaction migration(impl_->db);
      const int n = version(); // Another starter may have migrated while we waited for the lock.
      sql(impl_->db,
          "CREATE TABLE IF NOT EXISTS sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent "
          "TEXT,updated INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT "
          "'idle');"
          "CREATE TABLE IF NOT EXISTS tools(session TEXT NOT NULL,call_id TEXT NOT NULL,name TEXT "
          "NOT "
          "NULL,args TEXT NOT NULL,result TEXT,status TEXT NOT NULL,PRIMARY "
          "KEY(session,call_id),FOREIGN KEY(session) REFERENCES sessions(id));");
      if (n < 2)
        sql(impl_->db,
            "ALTER TABLE sessions ADD COLUMN provider TEXT NOT NULL DEFAULT '';ALTER "
            "TABLE sessions ADD COLUMN model TEXT NOT NULL DEFAULT '';PRAGMA user_version=2;");
      if (n < 3)
        sql(impl_->db, "PRAGMA user_version=3;");
      if (n < 4)
        sql(impl_->db, "ALTER TABLE sessions ADD COLUMN reasoning_effort TEXT;"
                       "PRAGMA user_version=4;");
      if (n < 5) {
        sql(impl_->db,
            "CREATE TABLE session_references("
            "session TEXT NOT NULL,kind TEXT NOT NULL,path TEXT NOT NULL,"
            "PRIMARY KEY(session,kind,path),FOREIGN KEY(session) REFERENCES sessions(id));");
        {
          Statement sessions(impl_->db, "SELECT id,messages FROM sessions");
          int status;
          while ((status = sqlite3_step(sessions.p)) == SQLITE_ROW) {
            std::vector<Message> parsed;
            try {
              parsed = stored_json(column(sessions.p, 1)).get<std::vector<Message>>();
            } catch (const Error &) {
              continue;
            } catch (const Json::exception &) {
              continue; // Loading this session still reports its corrupt transcript.
            }
            remember_references(column(sessions.p, 0), parsed);
          }
          if (status != SQLITE_DONE)
            throw Error("Cannot backfill session references: " +
                        std::string(sqlite3_errmsg(impl_->db)));
          Statement journal(impl_->db, "SELECT session,result FROM tools WHERE result IS NOT NULL");
          while ((status = sqlite3_step(journal.p)) == SQLITE_ROW) {
            Message parsed;
            try {
              parsed = stored_json(column(journal.p, 1)).get<Message>();
            } catch (const Error &) {
              continue;
            } catch (const Json::exception &) {
              continue; // Reconciliation retains its existing corrupt-record handling.
            }
            remember_references(column(journal.p, 0), {parsed});
          }
          if (status != SQLITE_DONE)
            throw Error("Cannot backfill journal references: " +
                        std::string(sqlite3_errmsg(impl_->db)));
          sql(impl_->db, "PRAGMA user_version=5;");
          impl_->known_references.clear();
        }
      }
      if (n < 6) {
        sql(impl_->db,
            "ALTER TABLE sessions ADD COLUMN message_format INTEGER NOT NULL DEFAULT 0;"
            "CREATE TABLE session_messages(session TEXT NOT NULL,position INTEGER NOT NULL,"
            "payload TEXT NOT NULL,PRIMARY KEY(session,position),"
            "FOREIGN KEY(session) REFERENCES sessions(id));"
            "CREATE TABLE provider_parts(session TEXT NOT NULL,position INTEGER NOT NULL,"
            "payload TEXT NOT NULL,PRIMARY KEY(session,position),"
            "FOREIGN KEY(session) REFERENCES sessions(id));");
        Statement sessions(impl_->db, "SELECT id,messages FROM sessions");
        int status;
        while ((status = sqlite3_step(sessions.p)) == SQLITE_ROW) {
          std::vector<Message> messages;
          try {
            messages = stored_json(column(sessions.p, 1)).get<std::vector<Message>>();
          } catch (const Error &) {
            continue;
          } catch (const Json::exception &) {
            continue;
          }
          const auto id = column(sessions.p, 0);
          ensure_message_ids(messages);
          for (size_t i = 0; i < messages.size(); ++i) {
            Statement insert(impl_->db, "INSERT INTO session_messages VALUES(?,?,?)");
            insert.text(1, id);
            sqlite3_bind_int64(insert.p, 2, static_cast<sqlite3_int64>(i));
            insert.text(3, safe_json(Json(messages[i])).dump());
            insert.run();
          }
          Statement mark(impl_->db,
                         "UPDATE sessions SET message_format=1,messages='[]' WHERE id=?");
          mark.text(1, id);
          mark.run();
        }
        if (status != SQLITE_DONE)
          throw Error("Cannot migrate session messages");
        sql(impl_->db, "PRAGMA user_version=6");
      }
      migration.commit();
    }
  }
  bool wal = false;
  {
    Statement mode(impl_->db, "PRAGMA journal_mode");
    if (sqlite3_step(mode.p) != SQLITE_ROW)
      throw Error("Cannot read database journal mode");
    wal = column(mode.p, 0) == "wal";
  }
  if (!wal) {
    acquire_migration();
    // A concurrent initializer may have changed journal mode while we waited.
    bool changed = false;
    {
      Statement mode(impl_->db, "PRAGMA journal_mode");
      if (sqlite3_step(mode.p) != SQLITE_ROW)
        throw Error("Cannot read database journal mode");
      changed = column(mode.p, 0) == "wal";
    }
    if (!changed)
      sql(impl_->db, "PRAGMA journal_mode=WAL");
  }
  sql(impl_->db, durability == "fast" ? "PRAGMA synchronous=NORMAL" : "PRAGMA synchronous=FULL");
}
StateStore::~StateStore() {
  if (impl_->db && impl_->durability == "fast") {
    try {
      sync();
    } catch (const std::exception &e) {
      std::fprintf(stderr, "Session synchronization failed: %s\n", e.what());
    }
  }
}
uint64_t StateStore::serialized_bytes() const {
  return impl_->serialized;
}
void StateStore::sync() {
  sql(impl_->db, "PRAGMA synchronous=FULL");
  try {
    Transaction barrier(impl_->db);
    sql(impl_->db, "CREATE TABLE IF NOT EXISTS durability_barrier(value INTEGER);"
                   "INSERT INTO durability_barrier VALUES(1); DELETE FROM durability_barrier;");
    barrier.commit();
  } catch (...) {
    if (impl_->durability == "fast")
      (void)sqlite3_exec(impl_->db, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
    throw;
  }
  if (impl_->durability == "fast")
    sql(impl_->db, "PRAGMA synchronous=NORMAL");
}

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
  struct stat status{};
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
  update_metadata(s);
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
  Statement q(impl_->db,
              "SELECT id,name,messages,status,provider,model,reasoning_effort,message_format FROM "
              "sessions WHERE id=?");
  q.text(1, id);
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("Session not found: " + id, 2);
  Session s{column(q.p, 0), column(q.p, 1), {}};
  s.provider = column(q.p, 4);
  s.model = column(q.p, 5);
  if (sqlite3_column_type(q.p, 6) != SQLITE_NULL && !column(q.p, 6).empty())
    s.effort = column(q.p, 6);
  try {
    if (sqlite3_column_int(q.p, 7) == 0)
      s.messages = stored_json(column(q.p, 2)).get<std::vector<Message>>();
    else {
      Statement rows(
          impl_->db,
          "SELECT position,payload FROM session_messages WHERE session=? ORDER BY position");
      rows.text(1, s.id);
      int status;
      while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
        if (sqlite3_column_int64(rows.p, 0) != static_cast<sqlite3_int64>(s.messages.size()))
          throw Error("Noncontiguous session messages");
        s.messages.push_back(stored_json(column(rows.p, 1)).get<Message>());
      }
      if (status != SQLITE_DONE)
        throw Error("Cannot load session messages");
    }
  } catch (const std::exception &) {
    throw Error("Corrupt session messages");
  }
  {
    Statement parts(
        impl_->db, "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position");
    parts.text(1, s.id);
    impl_->cached_session.clear();
    impl_->cached_provider = nullptr;
    impl_->cached_bytes = 0;
    int status;
    size_t position = 0, bytes = 0;
    while ((status = sqlite3_step(parts.p)) == SQLITE_ROW) {
      if (sqlite3_column_int64(parts.p, 0) != static_cast<sqlite3_int64>(position))
        corrupt_provider();
      const auto payload = column(parts.p, 1);
      bytes += payload.size();
      auto part = provider_json(payload);
      if (position == 0) {
        if (!part.is_null() && (!part.is_object() || part.contains("messages")))
          corrupt_provider();
        s.provider_state = provider_header(part);
      } else {
        validate_provider_part(part);
        if (!s.provider_state.is_object())
          corrupt_provider();
        s.provider_state["messages"].push_back(std::move(part));
      }
      ++position;
    }
    if (status != SQLITE_DONE)
      throw Error("Cannot load provider conversation: " + std::string(sqlite3_errmsg(impl_->db)));
    if (s.provider_state.is_object() && !s.provider_state.contains("messages"))
      s.provider_state["messages"] = Json::array();
    impl_->cache_provider(s, bytes);
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
      auto stored = load_id(session.id, false).messages;
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
                stored_json(column(record.p, 3)) != call.arguments)
              throw Error("Ambiguous journaled tool call id");
          } catch (const Json::exception &) {
            throw Error("Corrupt journaled tool arguments");
          }
        }
        Message result;
        if (recorded && status == "complete") {
          try {
            result = stored_json(column(record.p, 0)).get<Message>();
            if (result.role != "tool" || result.tool_call_id != call.id)
              throw Error("Mismatched completed tool record");
          } catch (const std::exception &) {
            throw Error("Corrupt completed tool record");
          }
        } else if (recorded && (status == "uncertain" || status == "not_started")) {
          try {
            result = stored_json(column(record.p, 0)).get<Message>();
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
          if (answer.previewed && !answer.result_file.empty()) {
            const auto preview = Json::parse(answer.text, nullptr, false);
            if (preview.is_object() &&
                preview.value("metadata", Json::object()).value("projection_truncated", false))
              result.text = answer.text;
          }
          result.previewed = answer.previewed;
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
    if (persist) {
      remember_references(session.id, session.messages);
      save(session);
    }
    sql(impl_->db, "COMMIT");
  } catch (...) {
    session.messages.swap(previous);
    if (!sqlite3_get_autocommit(impl_->db))
      (void)sqlite3_exec(impl_->db, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}
Session StateStore::last(bool repair) {
  Statement q(impl_->db, "SELECT id FROM sessions ORDER BY updated DESC,rowid DESC LIMIT 1");
  if (sqlite3_step(q.p) != SQLITE_ROW)
    throw Error("No saved sessions", 2);
  return load_id(column(q.p, 0), repair);
}
Session StateStore::fork(const Session &parent) {
  auto recovered = parent;
  portable_messages(recovered.messages, parent.id);
  ensure_message_ids(recovered.messages);
  reconcile(recovered, false);
  Session s{unique_id(), parent.name.empty() ? "" : parent.name + " (fork)", recovered.messages};
  s.provider = parent.provider;
  s.model = parent.model;
  s.effort = parent.effort;
  s.provider_state = parent.provider_state;
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
  remember_references(s.id, s.messages);
  Statement refs(impl_->db, "INSERT OR IGNORE INTO session_references(session,kind,path) "
                            "SELECT ?,kind,path FROM session_references WHERE session=?");
  refs.text(1, s.id);
  refs.text(2, parent.id);
  refs.run();
  reconcile(s);
  Statement q(impl_->db, "UPDATE sessions SET parent=? WHERE id=?");
  q.text(1, parent.id);
  q.text(2, s.id);
  q.run();
  return s;
}
void StateStore::update_metadata(const Session &s) {
  auto owner = acquire(s.id);
  Statement q(impl_->db,
              "INSERT INTO "
              "sessions(id,name,updated,messages,provider,model,reasoning_effort,message_format) "
              "VALUES(?,?,strftime('%s','now'),'[]',?,?,?,1) ON CONFLICT(id) DO UPDATE SET "
              "name=excluded.name,updated=excluded.updated,provider=excluded.provider,"
              "model=excluded.model,reasoning_effort=excluded.reasoning_effort");
  q.text(1, s.id);
  q.text(2, s.name);
  q.text(3, s.provider);
  q.text(4, s.model);
  if (s.effort)
    q.text(5, *s.effort);
  q.run();
}
void StateStore::save_provider_state(const Session &s) {
  const auto &messages = provider_messages(s.provider_state);
  for (const auto &part : messages)
    validate_provider_part(part);
  const auto next = provider_header(s.provider_state);
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  Json previous_base;
  size_t previous_count = 0, first = 0, bytes = 0;
  if (impl_->cached_session == s.id) {
    previous_base = provider_header(impl_->cached_provider);
    const auto &previous_messages = provider_messages(impl_->cached_provider);
    previous_count = previous_messages.size();
    while (first < messages.size() && first < previous_count &&
           messages[first] == previous_messages[first])
      ++first;
    bytes = impl_->cached_bytes;
    // Only the changed suffix is serialized; prefix bytes are known from the cached snapshot.
    for (size_t i = first; i < previous_count; ++i)
      bytes -= previous_messages[i].dump().size();
    if (!previous_base.is_null())
      bytes -= previous_base.dump().size();
  } else {
    Statement rows(impl_->db,
                   "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position");
    rows.text(1, s.id);
    int status;
    size_t position = 0;
    while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
      if (sqlite3_column_int64(rows.p, 0) != static_cast<sqlite3_int64>(position))
        corrupt_provider();
      const auto payload = column(rows.p, 1);
      const auto part = provider_json(payload);
      if (!position) {
        if (!part.is_null() && (!part.is_object() || part.contains("messages")))
          corrupt_provider();
        previous_base = provider_header(part);
      } else {
        validate_provider_part(part);
        if (!previous_base.is_object())
          corrupt_provider();
        if (first == previous_count && first < messages.size() && messages[first] == part) {
          ++first;
          bytes += payload.size();
        }
        ++previous_count;
      }
      ++position;
    }
    if (status != SQLITE_DONE)
      throw Error("Cannot compare provider conversation: " +
                  std::string(sqlite3_errmsg(impl_->db)));
  }
  const auto header = next.dump();
  if (next != previous_base) {
    Statement q(impl_->db, "INSERT INTO provider_parts VALUES(?,0,?) ON CONFLICT(session,position) "
                           "DO UPDATE SET payload=excluded.payload");
    q.text(1, s.id);
    q.text(2, header);
    q.run();
    impl_->serialized += header.size();
  }
  if (!next.is_null() || !previous_base.is_null())
    bytes += header.size();
  if (first < previous_count) {
    Statement q(impl_->db, "DELETE FROM provider_parts WHERE session=? AND position>?");
    q.text(1, s.id);
    sqlite3_bind_int64(q.p, 2, static_cast<sqlite3_int64>(first));
    q.run();
  }
  for (size_t i = first; i < messages.size(); ++i) {
    Statement q(impl_->db, "INSERT INTO provider_parts VALUES(?,?,?)");
    q.text(1, s.id);
    sqlite3_bind_int64(q.p, 2, static_cast<sqlite3_int64>(i) + 1);
    const auto text = messages[i].dump();
    bytes += text.size();
    impl_->serialized += text.size();
    q.text(3, text);
    q.run();
  }
  tx.commit();
  impl_->pending_provider_bytes = bytes;
  if (tx.owner)
    impl_->cache_provider(s, bytes);
}
size_t StateStore::provider_cache_bytes() const {
  return impl_->cached_bytes;
}
void StateStore::append_messages(const Session &s, size_t first, bool provider_state) {
  if (first > s.messages.size())
    throw Error("Invalid message append position");
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  update_metadata(s);
  Statement count(impl_->db, "SELECT count(*) FROM session_messages WHERE session=?");
  count.text(1, s.id);
  if (sqlite3_step(count.p) != SQLITE_ROW ||
      sqlite3_column_int64(count.p, 0) != static_cast<sqlite3_int64>(first))
    throw Error("Session changed before message append");
  for (size_t i = first; i < s.messages.size(); ++i) {
    Statement q(impl_->db, "INSERT INTO session_messages VALUES(?,?,?)");
    q.text(1, s.id);
    sqlite3_bind_int64(q.p, 2, static_cast<sqlite3_int64>(i));
    const auto text = safe_json(Json(s.messages[i])).dump();
    impl_->serialized += text.size();
    q.text(3, text);
    q.run();
    remember_references(s.id, {s.messages[i]});
  }
  if (provider_state)
    save_provider_state(s);
  tx.commit();
  if (provider_state && tx.owner)
    impl_->cache_provider(s, impl_->pending_provider_bytes);
}
void StateStore::revise_messages(const Session &s, const std::vector<size_t> &positions) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  for (const auto i : positions) {
    Statement q(impl_->db, "UPDATE session_messages SET payload=? WHERE session=? AND position=?");
    const auto text = safe_json(Json(s.messages.at(i))).dump();
    impl_->serialized += text.size();
    q.text(1, text);
    q.text(2, s.id);
    sqlite3_bind_int64(q.p, 3, static_cast<sqlite3_int64>(i));
    q.run();
    if (sqlite3_changes(impl_->db) != 1)
      throw Error("Session message missing during revision");
  }
  tx.commit();
}
void StateStore::replace_history(const Session &s) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  update_metadata(s);
  Statement clear(impl_->db, "DELETE FROM session_messages WHERE session=?");
  clear.text(1, s.id);
  clear.run();
  Statement mark(impl_->db, "UPDATE sessions SET message_format=1,messages='[]' WHERE id=?");
  mark.text(1, s.id);
  mark.run();
  append_messages(s, 0);
  save_provider_state(s);
  tx.commit();
  impl_->cache_provider(s, impl_->pending_provider_bytes);
}
void StateStore::save(const Session &s) {
  replace_history(s);
}
void StateStore::complete_tool(Session &s, const ToolCall &call, Message result) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  std::vector<Message> normalized{std::move(result)};
  portable_messages(normalized, s.id, call.name);
  ensure_message_ids(normalized);
  result = std::move(normalized.front());
  tool_finished(s.id, call, Json(result));
  const auto first = s.messages.size();
  s.messages.push_back(std::move(result));
  try {
    append_messages(s, first);
    tx.commit();
  } catch (...) {
    s.messages.resize(first);
    throw;
  }
}
void StateStore::remember_references(const std::string &id, const std::vector<Message> &messages) {
  auto &known = impl_->known_references[id];
  const auto paths = message_references(messages);
  if (std::all_of(paths.begin(), paths.end(), [&](const auto &path) { return known.count(path); }))
    return;
  Transaction transaction(impl_->db);
  for (const auto &path : paths) {
    if (known.count(path))
      continue;
    Statement q(impl_->db,
                "INSERT OR IGNORE INTO session_references(session,kind,path) VALUES(?,?,?)");
    q.text(1, id);
    q.text(2, result_name(path) ? "result" : "capture");
    q.text(3, path);
    q.run();
  }
  transaction.commit();
  if (transaction.owner)
    known.insert(paths.begin(), paths.end());
}
std::vector<std::string> StateStore::references(const std::string &id) {
  Statement q(impl_->db, "SELECT path FROM session_references WHERE session=? ORDER BY kind,path");
  q.text(1, id);
  std::vector<std::string> out;
  int status;
  while ((status = sqlite3_step(q.p)) == SQLITE_ROW)
    out.push_back(column(q.p, 0));
  if (status != SQLITE_DONE)
    throw Error("Cannot read session references");
  if (sqlite3_get_autocommit(impl_->db))
    impl_->known_references[id].insert(out.begin(), out.end());
  return out;
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
  if (status == "complete" && impl_->durability == "fast")
    sync();
}
void StateStore::tool_started(const std::string &id, const ToolCall &t) {
  auto owner = acquire(id);
  sql(impl_->db, "PRAGMA synchronous=FULL");
  struct Restore {
    Impl &impl;
    ~Restore() {
      if (impl.durability == "fast")
        (void)sqlite3_exec(impl.db, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
    }
  } restore{*impl_};
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
  Transaction transaction(impl_->db);
  Statement q(impl_->db, "UPDATE tools SET status='complete',result=? WHERE session=? AND "
                         "call_id=? AND status='running'");
  auto message = result.get<Message>();
  std::vector<Message> portable{message};
  portable_messages(portable, id, t.name);
  ensure_message_ids(portable);
  q.text(1, safe_json(Json(portable.front())).dump());
  q.text(2, id);
  q.text(3, t.id);
  q.run();
  if (sqlite3_changes(impl_->db) != 1)
    throw Error("Session modified by another process: tool outcome is no longer pending");
  remember_references(id, portable);
  transaction.commit();
}
} // namespace codet
