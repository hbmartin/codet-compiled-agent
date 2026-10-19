#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/support.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace codet {
namespace {
struct StoredDataCorruption : Error {
  using Error::Error;
};
Json stored_json(const std::string &bytes) {
  // The JSON lexer treats NUL as EOF even when the string has trailing bytes.
  if (bytes.find('\0') != std::string::npos)
    throw StoredDataCorruption("Stored JSON contains an embedded NUL");
  return Json::parse(bytes);
}
struct ProviderCacheCorruption : Error {
  using Error::Error;
};
struct RevisionIntegrityError : Error {
  using Error::Error;
};
struct InvalidRevisionToken : RevisionIntegrityError {
  using RevisionIntegrityError::RevisionIntegrityError;
};
template <class T> T session_json(const std::string &bytes) {
  try {
    return stored_json(bytes).get<T>();
  } catch (const Json::exception &) {
    throw StoredDataCorruption("Corrupt session messages");
  } catch (const StoredDataCorruption &) {
    throw StoredDataCorruption("Corrupt session messages");
  }
}
[[noreturn]] void sql_read_error(sqlite3 *db, const char *operation, int status) {
  throw Error(std::string(operation) + " (SQLite " + std::to_string(status) +
              "): " + sqlite3_errmsg(db));
}
constexpr size_t provider_cache_limit = size_t{8} * 1024 * 1024;
[[noreturn]] void corrupt_provider() {
  throw ProviderCacheCorruption(
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
// A previous release emitted this precise empty Anthropic wire message. Keep
// the source part so signed-epoch/transcript alignment remains intact.
bool normalize_legacy_part(Json &part, const Json &header) {
  bool changed = false;
  if (!header.is_object() || header.value("provider", "") != "anthropic" || !part.is_object() ||
      !part.contains("wire") || !part["wire"].is_array())
    return false;
  auto &wire = part["wire"];
  for (auto it = wire.begin(); it != wire.end();) {
    if (it->is_object() && it->size() == 2 && it->value("role", Json()) == "assistant" &&
        it->contains("content") && (*it)["content"].is_array() && (*it)["content"].empty()) {
      it = wire.erase(it);
      changed = true;
    } else
      ++it;
  }
  return changed;
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
                          "system", "latest_system", "execution_digest"})
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
  explicit Transaction(sqlite3 *connection, bool write = true)
      : db(connection), owner(sqlite3_get_autocommit(connection) != 0) {
    if (owner)
      sql(db, write ? "BEGIN IMMEDIATE" : "BEGIN");
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
void reset_message_rows(sqlite3 *db, const std::string &id) {
  Statement clear(db, "DELETE FROM session_messages WHERE session=?");
  clear.text(1, id);
  clear.run();
  Statement mark(db, "UPDATE sessions SET message_format=1,messages='[]' WHERE id=?");
  mark.text(1, id);
  mark.run();
}
void insert_message_row(Statement &row, const std::string &id, size_t position,
                        const std::string &payload) {
  if (position > static_cast<size_t>(std::numeric_limits<sqlite3_int64>::max()))
    throw Error("Session message position exceeds SQLite range");
  sqlite3_reset(row.p);
  row.text(1, id);
  sqlite3_bind_int64(row.p, 2, static_cast<sqlite3_int64>(position));
  row.text(3, payload);
  row.run();
}
void insert_message_rows(sqlite3 *db, const std::string &id,
                         const std::vector<std::string> &payloads) {
  Statement row(db, "INSERT INTO session_messages(session,position,payload) VALUES(?,?,?)");
  for (size_t i = 0; i < payloads.size(); ++i)
    insert_message_row(row, id, i, payloads[i]);
}
void insert_reference_rows(sqlite3 *db, const std::string &id, const std::set<std::string> &paths) {
  Statement ref(db, "INSERT OR IGNORE INTO session_references(session,kind,path) VALUES(?,?,?)");
  for (const auto &path : paths) {
    sqlite3_reset(ref.p);
    ref.text(1, id);
    ref.text(2, result_name(path) ? "result" : "capture");
    ref.text(3, path);
    ref.run();
  }
}
// The stored SQL is checked after schema changes before trusting revision tokens.
const std::map<std::string, std::string> &revision_schema() {
  static const std::map<std::string, std::string> definitions = {
      {"provider_revisions",
       "CREATE TABLE provider_revisions(session TEXT PRIMARY KEY,revision "
       "TEXT NOT NULL,FOREIGN KEY(session) REFERENCES sessions(id) ON DELETE CASCADE)"},
      {"provider_revision_insert",
       "CREATE TRIGGER provider_revision_insert AFTER INSERT ON provider_parts BEGIN "
       "INSERT INTO provider_revisions VALUES(NEW.session,lower(hex(randomblob(16)))) "
       "ON CONFLICT(session) DO UPDATE SET revision=excluded.revision; END"},
      {"provider_revision_delete",
       "CREATE TRIGGER provider_revision_delete AFTER DELETE ON provider_parts BEGIN "
       "INSERT INTO provider_revisions VALUES(OLD.session,lower(hex(randomblob(16)))) "
       "ON CONFLICT(session) DO UPDATE SET revision=excluded.revision; END"},
      {"provider_revision_update",
       "CREATE TRIGGER provider_revision_update AFTER UPDATE ON provider_parts BEGIN "
       "INSERT INTO provider_revisions VALUES(OLD.session,lower(hex(randomblob(16)))) "
       "ON CONFLICT(session) DO UPDATE SET revision=excluded.revision; "
       "INSERT INTO provider_revisions VALUES(NEW.session,lower(hex(randomblob(16)))) "
       "ON CONFLICT(session) DO UPDATE SET revision=excluded.revision; END"}};
  return definitions;
}
std::string select_session(sqlite3 *db, const std::string &selector, bool last) {
  Statement q(db, last ? "SELECT id FROM sessions ORDER BY updated DESC,rowid DESC LIMIT 1"
                       : "SELECT id FROM sessions WHERE id=? OR name=? "
                         "ORDER BY (id=?) DESC,updated DESC,rowid DESC LIMIT 1");
  if (!last) {
    q.text(1, selector);
    q.text(2, selector);
    q.text(3, selector);
  }
  const auto status = sqlite3_step(q.p);
  if (status == SQLITE_DONE)
    throw Error(last ? "No saved sessions" : "Session not found: " + selector, 2);
  if (status != SQLITE_ROW)
    sql_read_error(db, "Cannot select session", status);
  return column(q.p, 0);
}
fs::path repair_directory(const fs::path &home) {
  const auto dir = home / "repairs";
  if (fs::is_symlink(dir))
    throw Error("Invalid repair directory");
  fs::create_directories(dir);
  if (::chmod(dir.c_str(), 0700))
    throw Error("Cannot protect repair directory");
  return dir;
}
} // namespace
struct StateStore::Impl {
  sqlite3 *db = nullptr;
  fs::path home, artifact_root, artifact_anchor;
  NativeFileSystem files;
  void portable(std::vector<Message> &messages, const std::string &id,
                const std::string &tool = "") const {
    for (auto &message : messages)
      normalize_checkpoint_reference(message, files, artifact_root, artifact_anchor);
    portable_messages(messages, id, tool);
  }
  const Message &portable_message(const Message &message, std::optional<Message> &storage) const {
    const auto &state = message.checkpoint_state;
    if (state.is_object()) {
      const auto ref = state.find("recovery_record");
      if (ref != state.end() && ref->is_string() &&
          fs::path(ref->get_ref<const std::string &>()).is_absolute()) {
        auto &normalized = storage.emplace(message);
        normalize_checkpoint_reference(normalized, files, artifact_root, artifact_anchor);
        return normalized;
      }
    }
    return message;
  }
  std::set<std::string> message_paths(const std::vector<Message> &messages) const {
    auto paths = message_references(messages);
    for (const auto &message : messages) {
      const auto &state = message.checkpoint_state;
      if (!state.is_object())
        continue;
      const auto ref = state.find("recovery_record");
      if (ref == state.end() || !ref->is_string() ||
          !fs::path(ref->get_ref<const std::string &>()).is_absolute())
        continue;
      Message checkpoint;
      checkpoint.checkpoint_state = state;
      if (normalize_checkpoint_reference(checkpoint, files, artifact_root, artifact_anchor)) {
        auto normalized = message_references({checkpoint});
        paths.insert(normalized.begin(), normalized.end());
      }
    }
    return paths;
  }
  std::map<std::string, std::weak_ptr<void>> owners;
  std::map<std::string, std::set<std::string>> known_references;
  void remember_paths(const std::string &id, std::set<std::string> paths) {
    auto &known = known_references[id];
    for (auto it = paths.begin(); it != paths.end();)
      if (known.count(*it))
        it = paths.erase(it);
      else
        ++it;
    if (paths.empty())
      return;
    Transaction transaction(db);
    insert_reference_rows(db, id, paths);
    transaction.commit();
    if (transaction.owner) {
      try {
        known.insert(paths.begin(), paths.end());
      } catch (...) {
        known.clear(); // Optional knowledge cannot fail a durable reference publication.
      }
    }
  }
  std::string cached_session;
  Json cached_provider = nullptr;
  size_t cached_bytes = 0, pending_provider_bytes = 0;
  void clear_provider_cache() noexcept {
    cached_session.clear();
    cached_provider = nullptr;
    cached_bytes = 0;
    cursor_session.clear();
    cursor_token.clear();
    cursor_count = cursor_bytes = cursor_header_bytes = 0;
    revision = 0;
  }
  void cache_provider(const Session &session, size_t bytes) {
    clear_provider_cache();
    if (bytes <= provider_cache_limit) {
      cached_provider = session.provider_state;
      cached_session = session.id;
      cached_bytes = bytes;
    }
  }
  std::string durability;
  uint64_t serialized = 0, rows_read = 0, generation = 0, revision = 0;
  std::string cursor_session;
  size_t cursor_count = 0, cursor_bytes = 0, cursor_header_bytes = 0;
  std::string cursor_token, pending_provider_token;
  int verified_schema = -1;
  void verify_revisions() {
    Statement version(db, "PRAGMA schema_version");
    if (sqlite3_step(version.p) != SQLITE_ROW)
      throw Error("Cannot inspect provider revision schema");
    const auto current = sqlite3_column_int(version.p, 0);
    if (current == verified_schema)
      return;
    clear_provider_cache();
    for (const auto &definition : revision_schema()) {
      Statement q(db, "SELECT sql FROM sqlite_master WHERE name=?");
      q.text(1, definition.first);
      const auto status = sqlite3_step(q.p);
      if (status != SQLITE_ROW && status != SQLITE_DONE)
        throw Error("Cannot inspect provider revision infrastructure: " +
                    std::string(sqlite3_errmsg(db)));
      if (status == SQLITE_DONE || column(q.p, 0) != definition.second)
        throw RevisionIntegrityError("Invalid provider revision infrastructure: " +
                                     definition.first);
    }
    verified_schema = current;
  }
  std::string provider_token(const std::string &id) {
    verify_revisions();
    Statement q(db, "SELECT revision FROM provider_revisions WHERE session=?");
    q.text(1, id);
    const auto status = sqlite3_step(q.p);
    if (status == SQLITE_DONE)
      return {};
    if (status != SQLITE_ROW)
      throw Error("Cannot inspect provider revision");
    const auto token = column(q.p, 0);
    if (token.size() != 32 || token.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw InvalidRevisionToken(
          "Invalid provider revision token; explicit provider-cache repair required");
    return token;
  }
  void check_provider_tail(const std::string &id, size_t count) const {
    Statement tail(db, "SELECT 1 FROM provider_parts WHERE session=? AND position>? LIMIT 1");
    tail.text(1, id);
    sqlite3_bind_int64(tail.p, 2, static_cast<sqlite3_int64>(count));
    const auto status = sqlite3_step(tail.p);
    if (status != SQLITE_DONE)
      throw Error(status == SQLITE_ROW ? "Unexpected provider rows beyond committed prefix"
                                       : "Cannot inspect provider prefix boundary");
  }
  void populate_provider(const Session &session, size_t bytes, bool publish = true) {
    try {
      const auto token = provider_token(session.id);
      if (publish)
        publish_provider(session, bytes, token);
    } catch (const RevisionIntegrityError &) {
      clear_provider_cache();
      throw;
    } catch (...) {
      clear_provider_cache();
    }
  }
  // Cache publication is optional: a successful COMMIT must remain successful.
  void publish_provider(const Session &session, size_t bytes, const std::string &token) noexcept {
    try {
      cache_provider(session, bytes);
      cursor_session = session.id;
      cursor_count = provider_messages(session.provider_state).size();
      cursor_bytes = bytes;
      cursor_header_bytes = bytes ? provider_header(session.provider_state).dump().size() : 0;
      cursor_token = token;
      revision = ++generation;
    } catch (...) {
      clear_provider_cache();
    }
  }
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
  impl_->artifact_root =
      fs::absolute(artifact_root.empty() ? home / "artifacts" : artifact_root).lexically_normal();
  impl_->artifact_anchor = impl_->files.canonical(impl_->artifact_root);
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
    if (value > 7)
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
  if (version() < 7) {
    acquire_migration();
    const auto old_version = version();
    if (old_version > 0 && old_version < 7) {
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
      const auto target = home / ("state.sqlite3.pre-v7-" + unique_id());
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
    if (version() < 7) {
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
      if (n < 7) {
        // Older-layout fixtures may retain the revision table while rebuilding provider_parts.
        for (const auto &definition : revision_schema()) {
          Statement exists(impl_->db, "SELECT sql FROM sqlite_master WHERE name=?");
          exists.text(1, definition.first);
          const auto status = sqlite3_step(exists.p);
          if (status == SQLITE_DONE)
            sql(impl_->db, definition.second.c_str());
          else if (status != SQLITE_ROW)
            sql_read_error(impl_->db, "Cannot inspect provider revision infrastructure", status);
          else if (column(exists.p, 0) != definition.second)
            throw RevisionIntegrityError("Invalid provider revision infrastructure: " +
                                         definition.first);
        }
        sql(impl_->db, "INSERT OR IGNORE INTO provider_revisions "
                       "SELECT session,lower(hex(randomblob(16))) FROM (SELECT DISTINCT session "
                       "FROM provider_parts);"
                       "PRAGMA user_version=7");
      }
      migration.commit();
    }
  }
  impl_->verify_revisions();
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
  const auto id = select_session(impl_->db, selector, false);
  return load_id(id, repair);
}
Session StateStore::load_id(const std::string &id, bool repair) {
  return read_session(id, repair, true);
}
Session StateStore::read_session(const std::string &id, bool repair, bool provider_state) {
  Statement q(impl_->db,
              "SELECT id,name,messages,status,provider,model,reasoning_effort,message_format FROM "
              "sessions WHERE id=?");
  q.text(1, id);
  const auto selected = sqlite3_step(q.p);
  if (selected == SQLITE_DONE)
    throw Error("Session not found: " + id, 2);
  if (selected != SQLITE_ROW)
    sql_read_error(impl_->db, "Cannot load session", selected);
  Session s{column(q.p, 0), column(q.p, 1), {}};
  s.provider = column(q.p, 4);
  s.model = column(q.p, 5);
  if (sqlite3_column_type(q.p, 6) != SQLITE_NULL && !column(q.p, 6).empty())
    s.effort = column(q.p, 6);
  if (sqlite3_column_int(q.p, 7) == 0)
    s.messages = session_json<std::vector<Message>>(column(q.p, 2));
  else {
    Statement rows(
        impl_->db,
        "SELECT position,payload FROM session_messages WHERE session=? ORDER BY position");
    rows.text(1, s.id);
    int status;
    while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
      if (sqlite3_column_int64(rows.p, 0) != static_cast<sqlite3_int64>(s.messages.size()))
        throw StoredDataCorruption("Corrupt session messages: noncontiguous positions");
      s.messages.push_back(session_json<Message>(column(rows.p, 1)));
    }
    if (status != SQLITE_DONE)
      sql_read_error(impl_->db, "Cannot load session messages", status);
  }
  if (provider_state) {
    Statement parts(
        impl_->db, "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position");
    parts.text(1, s.id);
    impl_->clear_provider_cache();
    int status;
    size_t position = 0, bytes = 0;
    bool normalized = false;
    while ((status = sqlite3_step(parts.p)) == SQLITE_ROW) {
      if (sqlite3_column_int64(parts.p, 0) != static_cast<sqlite3_int64>(position))
        corrupt_provider();
      ++impl_->rows_read;
      const auto payload = column(parts.p, 1);
      bytes += payload.size();
      auto part = provider_json(payload);
      if (position == 0) {
        if (!part.is_null() && (!part.is_object() || part.contains("messages")))
          corrupt_provider();
        s.provider_state = provider_header(part);
      } else {
        normalized = normalize_legacy_part(part, s.provider_state) || normalized;
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
    if (sqlite3_get_autocommit(impl_->db))
      impl_->populate_provider(s, bytes, !normalized);
  }
  impl_->portable(s.messages, s.id);
  ensure_message_ids(s.messages);
  if (repair)
    reconcile(s, false);
  return s;
}
void StateStore::reconcile(Session &session, bool persist) {
  impl_->portable(session.messages, session.id);
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
      auto stored = read_session(session.id, false, false).messages;
      impl_->portable(stored, session.id);
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
    impl_->portable(repaired, session.id);
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
  const auto id = select_session(impl_->db, "", true);
  return load_id(id, repair);
}
Session StateStore::fork(const Session &parent) {
  auto recovered = parent;
  impl_->portable(recovered.messages, parent.id);
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
void StateStore::save_provider_state(const Session &s,
                                     const std::optional<ProviderStateUpdate> &hint) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  const auto token = impl_->provider_token(s.id);
  const auto &messages = provider_messages(s.provider_state);
  const bool incremental = s.provider_state.is_object() && hint && hint->revision &&
                           hint->revision == impl_->revision && impl_->cursor_session == s.id &&
                           hint->unchanged == impl_->cursor_count &&
                           messages.size() >= hint->unchanged && token == impl_->cursor_token;
  for (size_t i = incremental ? hint->unchanged : 0; i < messages.size(); ++i)
    validate_provider_part(messages[i]);
  const auto next = provider_header(s.provider_state);
  Json previous_base;
  size_t previous_count = 0, first = 0, bytes = 0;
  if (incremental) {
    impl_->check_provider_tail(s.id, impl_->cursor_count);
    previous_count = first = impl_->cursor_count;
    bytes = impl_->cursor_bytes - impl_->cursor_header_bytes;
    // Rewriting the small header avoids retaining another copy of it outside the cap.
    previous_base = nullptr;
  } else if (impl_->cached_session == s.id && token == impl_->cursor_token) {
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
      ++impl_->rows_read;
      const auto payload = column(rows.p, 1);
      if (!position) {
        const auto part = provider_json(payload);
        if (!part.is_null() && (!part.is_object() || part.contains("messages")))
          corrupt_provider();
        previous_base = provider_header(part);
      } else {
        if (!previous_base.is_object())
          corrupt_provider();
        // Canonical unchanged rows are already validated through the incoming state. Avoid
        // reparsing large image/string blocks on every uncached append.
        bool same =
            first == previous_count && first < messages.size() && payload == messages[first].dump();
        if (!same) {
          auto part = provider_json(payload);
          const bool normalized = normalize_legacy_part(part, previous_base);
          validate_provider_part(part);
          same = !normalized && first == previous_count && first < messages.size() &&
                 messages[first] == part;
        }
        if (same) {
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
  // Compare the cached prefix first, but never leave it describing rows changed by
  // an enclosing transaction whose eventual COMMIT or ROLLBACK we do not own.
  impl_->clear_provider_cache();
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
  impl_->check_provider_tail(s.id, messages.size());
  auto committed_token = impl_->provider_token(s.id);
  tx.commit();
  impl_->pending_provider_bytes = bytes;
  impl_->pending_provider_token = std::move(committed_token);
  if (tx.owner)
    impl_->publish_provider(s, bytes, impl_->pending_provider_token);
}
uint64_t StateStore::provider_revision(const std::string &id) const {
  const auto token = impl_->provider_token(id);
  return impl_->cursor_session == id && token == impl_->cursor_token ? impl_->revision : 0;
}
uint64_t StateStore::provider_rows_read() const {
  return impl_->rows_read;
}
size_t StateStore::provider_cache_bytes() const {
  return impl_->cached_bytes;
}
void StateStore::append_messages(const Session &s, size_t first, bool provider_state,
                                 const std::optional<ProviderStateUpdate> &hint) {
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
  Statement row(impl_->db, "INSERT INTO session_messages(session,position,payload) VALUES(?,?,?)");
  for (size_t i = first; i < s.messages.size(); ++i) {
    std::optional<Message> normalized;
    const auto &message = impl_->portable_message(s.messages[i], normalized);
    const auto payload = safe_json(Json(message)).dump();
    impl_->serialized += payload.size();
    insert_message_row(row, s.id, i, payload);
    impl_->remember_paths(s.id, message_references({message}));
  }
  if (provider_state)
    save_provider_state(s, hint);
  tx.commit();
  if (provider_state && tx.owner)
    impl_->publish_provider(s, impl_->pending_provider_bytes, impl_->pending_provider_token);
  else if (provider_state)
    impl_->clear_provider_cache();
}
void StateStore::revise_messages(const Session &s, const std::vector<size_t> &positions) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  for (const auto i : positions) {
    Statement q(impl_->db, "UPDATE session_messages SET payload=? WHERE session=? AND position=?");
    std::optional<Message> normalized;
    const auto &message = impl_->portable_message(s.messages.at(i), normalized);
    const auto text = safe_json(Json(message)).dump();
    impl_->serialized += text.size();
    q.text(1, text);
    q.text(2, s.id);
    sqlite3_bind_int64(q.p, 3, static_cast<sqlite3_int64>(i));
    q.run();
    if (sqlite3_changes(impl_->db) != 1)
      throw Error("Session message missing during revision");
    impl_->remember_paths(s.id, message_references({message}));
  }
  tx.commit();
}
void StateStore::replace_history(const Session &s) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  update_metadata(s);
  reset_message_rows(impl_->db, s.id);
  append_messages(s, 0);
  save_provider_state(s);
  tx.commit();
  if (tx.owner)
    impl_->publish_provider(s, impl_->pending_provider_bytes, impl_->pending_provider_token);
  else
    impl_->clear_provider_cache();
}
void StateStore::save(const Session &s) {
  replace_history(s);
}
void StateStore::complete_tool(Session &s, const ToolCall &call, Message result) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  std::vector<Message> normalized{std::move(result)};
  impl_->portable(normalized, s.id, call.name);
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
  impl_->remember_paths(id, impl_->message_paths(messages));
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
Json StateStore::recovery_facts(const std::string &id) const {
  Json facts = Json::array();
  Statement rows(impl_->db,
                 "SELECT call_id,status,result FROM tools WHERE session=? ORDER BY rowid");
  rows.text(1, id);
  int status;
  while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
    const auto state = column(rows.p, 1);
    if (state == "complete")
      continue;
    Json fact = {{"call_id", column(rows.p, 0)},
                 {"recovery", state == "not_started" ? "not_started" : "uncertain"},
                 {"action_completed", false}};
    const auto record = Json::parse(column(rows.p, 2), nullptr, false);
    if (record.is_object() && record.value("text", Json()).is_string()) {
      const auto envelope = Json::parse(record.at("text").get<std::string>(), nullptr, false);
      if (envelope.is_object() && envelope.value("metadata", Json()).is_object())
        fact["action_completed"] = envelope["metadata"].value("action_completed", Json()) == true;
    }
    facts.push_back(std::move(fact));
  }
  if (status != SQLITE_DONE)
    throw Error("Cannot reconstruct checkpoint journal facts");
  return facts;
}
std::string StateStore::preserve_response(const Session &session, const Message &message) {
  const auto dir = repair_directory(impl_->home);
  const auto path = dir / (session.id + "-response-" + unique_id() + ".json");
  NativeFileSystem files;
  files.atomic_write(path,
                     safe_json(Json({{"kind", "uncommitted_provider_response"},
                                     {"session", session.id},
                                     {"message", message},
                                     {"dispatch_allowed", false}}))
                         .dump(),
                     true);
  return path.string();
}
void StateStore::discard_recovery_record(const RecoveryCandidate &candidate, Tools &tools) {
  const auto &ref = candidate.file.identifier;
  if (!result_name(ref))
    throw Error("Invalid recovery candidate reference");
  Statement used(impl_->db, "SELECT 1 FROM session_references WHERE path=? LIMIT 1");
  used.text(1, ref);
  const auto status = sqlite3_step(used.p);
  if (status == SQLITE_ROW)
    return;
  if (status != SQLITE_DONE)
    throw Error("Cannot inspect recovery candidate references");
  tools.discard_recovery_candidate(candidate);
}
Json StateStore::repair(const std::string &selector, bool last, bool provider_cache, bool recovery,
                        Tools &tools) {
  if (!sqlite3_get_autocommit(impl_->db))
    throw Error("Repair requires its own transaction");
  const auto id = select_session(impl_->db, selector, last);
  auto owner = acquire(id);
  const bool mutate = provider_cache || recovery;
  std::vector<std::string> backups;
  backups.reserve(2);
  const auto database_version = [&] {
    Statement q(impl_->db, "PRAGMA data_version");
    if (sqlite3_step(q.p) != SQLITE_ROW)
      throw Error("Cannot inspect repair snapshot version");
    return sqlite3_column_int64(q.p, 0);
  };
  // One retry only. Preparation never holds the database's writer lock.
  for (int attempt = 0; attempt < 2; ++attempt) {
    std::vector<RecoveryCandidate> candidates;
    const auto cleanup = [&] {
      impl_->clear_provider_cache();
      cleanup_recovery_candidates(*this, tools, candidates);
    };
    try {
      const auto version =
          database_version(); // Before BEGIN: later external commits invalidate it.
      Session session;
      Json journal, backup,
          report = {{"session", id}, {"changed", false}, {"issues", Json::array()}};
      {
        Transaction snapshot(impl_->db, false);
        session = read_session(id, false, false);
        try {
          (void)load_id(id, false);
        } catch (const ProviderCacheCorruption &error) {
          if (sqlite3_get_autocommit(impl_->db))
            throw;
          report["issues"].push_back(error.what());
        }
        if (sqlite3_get_autocommit(impl_->db))
          throw Error("Repair read snapshot ended unexpectedly");
        try {
          (void)impl_->provider_token(id);
        } catch (const InvalidRevisionToken &error) {
          if (sqlite3_get_autocommit(impl_->db))
            throw;
          report["issues"].push_back(error.what());
        }
        if (sqlite3_get_autocommit(impl_->db))
          throw Error("Repair read snapshot ended unexpectedly");
        journal = recovery_facts(id);
        if (mutate) {
          backup = {{"session", id}, {"kind", "session_repair_backup"}};
          for (const auto *table : {"sessions", "session_messages", "provider_parts", "tools",
                                    "session_references", "provider_revisions"}) {
            const auto query =
                std::string("SELECT * FROM ") + table +
                (std::string(table) == "sessions" ? " WHERE id=?" : " WHERE session=?");
            Statement rows(impl_->db, query.c_str());
            rows.text(1, id);
            backup[table] = Json::array();
            int status;
            while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
              Json row = Json::object();
              for (int i = 0; i < sqlite3_column_count(rows.p); ++i) {
                const auto value = column(rows.p, i);
                const auto type = sqlite3_column_type(rows.p, i);
                Json stored = type == SQLITE_NULL ? Json() : Json(value);
                if (type != SQLITE_NULL && safe_text(value) != value)
                  stored = {{"encoding", "base64"}, {"bytes", base64_encode(value)}};
                row[sqlite3_column_name(rows.p, i)] = std::move(stored);
              }
              backup[table].push_back(std::move(row));
            }
            if (status != SQLITE_DONE)
              throw Error("Cannot back up session repair rows");
          }
        }
        snapshot.commit();
      }
      auto execution = tools.observe_execution(session.messages, [&] { return journal; });
      auto &facts = execution.facts;
      for (const auto &fact : facts)
        if (fact.value("kind", Json()) == "recovery_unavailable")
          report["issues"].push_back(fact);
      using ObservationKey = std::pair<ArtifactPolicy, std::string>;
      std::map<ObservationKey, ReferenceObservation> observed;
      for (auto &entry : execution.archives)
        observed.emplace(ObservationKey{entry.second.policy, entry.first}, std::move(entry.second));
      struct ResultObservation {
        ReferenceObservation *reference = nullptr;
        Json missing;
      };
      std::vector<ResultObservation> results;
      results.reserve(session.messages.size());
      for (const auto &message : session.messages) {
        results.push_back({nullptr, nullptr});
        if (message.result_file.empty())
          continue;
        // Archive and result policies differ; keep separate records when both use a path.
        const ObservationKey key{ArtifactPolicy::owned_regular, message.result_file};
        auto entry = observed.find(key);
        if (entry == observed.end())
          entry = observed.emplace(key, tools.observe_result_reference(message.result_file)).first;
        auto &result = results.back();
        result.reference = &entry->second;
        if (!result.reference->error.empty()) {
          result.missing = {{"kind", "recovery_unavailable"},
                            {"recovery", "unknown"},
                            {"original_reference", message.result_file},
                            {"message_id", message.message_id},
                            {"reason", result.reference->error},
                            {"instruction", "Only a partial surviving observation is available; "
                                            "inspect effects before repeating actions."}};
          report["issues"].push_back(result.missing);
        }
      }
      if (!mutate)
        return report;
      auto backup_path =
          (repair_directory(impl_->home) / (id + "-backup-" + unique_id() + ".json")).string();
      backups.push_back(std::move(backup_path));
      try {
        tools.write_repair_backup(backups.back(), backup.dump());
      } catch (...) {
        std::error_code status;
        if (!fs::is_regular_file(backups.back(), status))
          backups.pop_back();
        throw;
      }
      report["backup"] = backups.back();
      if (backups.size() > 1)
        report["backups"] = backups;
      std::vector<std::string> payloads;
      std::set<std::string> retained;
      if (recovery) {
        tools.use_session(id);
        bool unavailable = false;
        for (size_t i = 0; i < session.messages.size(); ++i) {
          auto &message = session.messages[i];
          if (message.result_file.empty())
            continue;
          if (results[i].reference->error.empty())
            message.result_file = results[i].reference->identifier;
          else {
            facts.push_back(results[i].missing);
            unavailable = true;
            Message partial = message;
            partial.message_id = unique_id();
            partial.result_file.clear();
            partial.checkpoint_state = Json::array({results[i].missing});
            partial.text = "Partial surviving observation from explicit repair; original result "
                           "unavailable.\n" +
                           message.text;
            const auto previous_candidates = candidates.size();
            tools.retain_recovery_result(partial, candidates);
            message.result_file = partial.result_file;
            observed[{ArtifactPolicy::private_owned_regular, partial.result_file}] =
                candidates.size() > previous_candidates
                    ? candidates.back().file
                    : tools.observe_result_reference(partial.result_file, true);
          }
        }
        for (auto &message : session.messages)
          if (!message.checkpoint_state.is_null())
            message.checkpoint_state = facts;
        if (unavailable) {
          Message marker;
          marker.role = "system";
          marker.origin = "recovery";
          marker.message_id = unique_id();
          marker.text = "Explicit repair retained incomplete historical results. Use current "
                        "recovery facts; never replay historical effects automatically.";
          marker.checkpoint_state = facts;
          session.messages.push_back(std::move(marker));
        }
        impl_->portable(session.messages, id);
        ensure_message_ids(session.messages);
        payloads.reserve(session.messages.size());
        for (const auto &message : session.messages)
          payloads.push_back(safe_json(Json(message)).dump());
        retained = message_references(session.messages);
      }
      // Allocate report fields before the durable commit, just like prepared payloads.
      report["changed"] = true;
      report["provider_cache_reset"] = true;
      report["recovery_facts"] = recovery ? facts : Json();
      bool stale = false;
      {
        Transaction publication(impl_->db);
        impl_->verify_revisions();
        stale = database_version() != version;
        if (recovery)
          for (const auto &entry : observed) {
            if (stale)
              break;
            stale = !tools.reference_current(entry.second);
          }
        if (!stale) {
          impl_->clear_provider_cache();
          impl_->known_references.erase(id);
          Statement clear(impl_->db, "DELETE FROM provider_parts WHERE session=?");
          clear.text(1, id);
          clear.run();
          Statement rotate(impl_->db, "INSERT INTO provider_revisions(session,revision) "
                                      "VALUES(?,lower(hex(randomblob(16)))) ON CONFLICT(session) "
                                      "DO UPDATE SET revision=excluded.revision");
          rotate.text(1, id);
          rotate.run();
          if (recovery) {
            update_metadata(session);
            reset_message_rows(impl_->db, id);
            insert_message_rows(impl_->db, id, payloads);
            for (const auto &payload : payloads)
              impl_->serialized += payload.size();
            insert_reference_rows(impl_->db, id, retained);
          }
          publication.commit();
        }
      }
      if (!stale)
        return report;
      cleanup();
      candidates.clear();
      if (attempt == 1)
        throw Error("Repair preparation changed twice; no repair changes applied; retry repair");
    } catch (...) {
      const auto failure = std::current_exception();
      cleanup();
      if (backups.empty())
        std::rethrow_exception(failure);
      std::string suffix;
      for (const auto &path : backups)
        suffix += "; backup: " + path;
      try {
        std::rethrow_exception(failure);
      } catch (const Error &error) {
        throw Error(std::string(error.what()) + suffix, error.code);
      } catch (const std::exception &error) {
        throw Error(std::string(error.what()) + suffix);
      }
    }
  }
  throw Error("Repair preparation exhausted"); // Both attempts return or throw above.
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
  impl_->portable(portable, id, t.name);
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
