#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/support.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <thread>
#include <tuple>
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
struct InvalidSessionToken : RevisionIntegrityError {
  using RevisionIntegrityError::RevisionIntegrityError;
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
bool valid_revision_token(const std::string &token) {
  return token.size() == 32 && token.find_first_not_of("0123456789abcdef") == std::string::npos;
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
// Journal-only operations may use an internal savepoint without publishing a Session.
struct JournalTransaction {
  sqlite3 *db;
  bool owner, finished = false;
  explicit JournalTransaction(sqlite3 *connection)
      : db(connection), owner(sqlite3_get_autocommit(connection) != 0) {
    sql(db, owner ? "BEGIN IMMEDIATE" : "SAVEPOINT codet_transcript_write");
  }
  ~JournalTransaction() {
    if (!finished) {
      if (owner)
        (void)sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
      else {
        (void)sqlite3_exec(db, "ROLLBACK TO codet_transcript_write", nullptr, nullptr, nullptr);
        (void)sqlite3_exec(db, "RELEASE codet_transcript_write", nullptr, nullptr, nullptr);
      }
    }
  }
  void commit() {
    sql(db, owner ? "COMMIT" : "RELEASE codet_transcript_write");
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
// Session markers cover every persisted input used by preparation, including direct SQL writers.
const std::map<std::string, std::string> &session_revision_schema() {
  static const auto definitions = [] {
    std::map<std::string, std::string> out{
        {"session_revisions",
         "CREATE TABLE session_revisions(session TEXT PRIMARY KEY,revision TEXT NOT NULL,"
         "FOREIGN KEY(session) REFERENCES sessions(id) ON DELETE CASCADE)"}};
    const auto rotate = [](const std::string &id) {
      return "INSERT INTO session_revisions(session,revision) SELECT " + id +
             ",lower(hex(randomblob(16))) WHERE EXISTS(SELECT 1 FROM sessions WHERE id=" + id +
             ") ON CONFLICT(session) DO UPDATE SET revision=excluded.revision; ";
    };
    for (const auto *table : {"sessions", "session_messages", "tools", "session_references",
                              "provider_parts", "provider_revisions"}) {
      const bool parent = std::string(table) == "sessions";
      const std::string field = parent ? ".id" : ".session";
      for (const auto *event : {"INSERT", "UPDATE", "DELETE"}) {
        const std::string action(event),
            name = "session_revision_" + std::string(table) + "_" + action;
        std::string body;
        if (action != "INSERT")
          body += rotate("OLD" + field);
        if (action != "DELETE")
          body += rotate("NEW" + field);
        if (parent && action != "INSERT")
          body += "DELETE FROM session_revisions WHERE session=OLD.id AND NOT EXISTS"
                  "(SELECT 1 FROM sessions WHERE id=OLD.id); ";
        std::string definition = "CREATE TRIGGER ";
        definition += name;
        definition += " AFTER ";
        definition += action;
        definition += " ON ";
        definition += table;
        definition += " BEGIN ";
        definition += body;
        definition += "END";
        out.emplace(name, std::move(definition));
      }
    }
    return out;
  }();
  return definitions;
}
bool same_message(const Message &a, const Message &b) {
  if (std::tie(a.role, a.text, a.tool_call_id, a.replay, a.replay_provider, a.origin,
               a.artifact_index, a.message_id, a.result_file, a.images_archived, a.previewed,
               a.checkpoint_state) != std::tie(b.role, b.text, b.tool_call_id, b.replay,
                                               b.replay_provider, b.origin, b.artifact_index,
                                               b.message_id, b.result_file, b.images_archived,
                                               b.previewed, b.checkpoint_state) ||
      a.calls.size() != b.calls.size() || a.images.size() != b.images.size() ||
      a.assistant_items.size() != b.assistant_items.size())
    return false;
  for (size_t i = 0; i < a.calls.size(); ++i)
    if (std::tie(a.calls[i].id, a.calls[i].name, a.calls[i].arguments) !=
        std::tie(b.calls[i].id, b.calls[i].name, b.calls[i].arguments))
      return false;
  for (size_t i = 0; i < a.images.size(); ++i)
    if (std::tie(a.images[i].path, a.images[i].mime, a.images[i].sha256, a.images[i].detail,
                 a.images[i].width, a.images[i].height) !=
        std::tie(b.images[i].path, b.images[i].mime, b.images[i].sha256, b.images[i].detail,
                 b.images[i].width, b.images[i].height))
      return false;
  for (size_t i = 0; i < a.assistant_items.size(); ++i)
    if (std::tie(a.assistant_items[i].id, a.assistant_items[i].text, a.assistant_items[i].phase) !=
        std::tie(b.assistant_items[i].id, b.assistant_items[i].text, b.assistant_items[i].phase))
      return false;
  return true;
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
// nlohmann JSON destruction allocates a flattening stack for nonempty containers.
// Reserve that work before COMMIT, then empty retired trees from leaves upward.
struct JsonRetirement {
  std::vector<Json *> nodes;
  void prepare(const Json &value) {
    std::vector<const Json *> scan{&value};
    for (size_t i = 0; i < scan.size(); ++i)
      if (scan[i]->is_structured())
        for (const auto &child : *scan[i])
          scan.push_back(&child);
    nodes.resize(std::max(nodes.size(), scan.size()));
  }
  void retire(Json &value) noexcept {
    size_t used = 1;
    assert(!nodes.empty());
    nodes[0] = &value;
    for (size_t i = 0; i < used; ++i) {
      if (auto *object = nodes[i]->get_ptr<Json::object_t *>()) {
        for (auto &child : *object) {
          assert(used < nodes.size());
          nodes[used++] = &child.second;
        }
      } else if (auto *array = nodes[i]->get_ptr<Json::array_t *>()) {
        for (auto &child : *array) {
          assert(used < nodes.size());
          nodes[used++] = &child;
        }
      }
    }
    while (used) {
      auto *node = nodes[--used];
      if (auto *object = node->get_ptr<Json::object_t *>())
        object->clear();
      else if (auto *array = node->get_ptr<Json::array_t *>())
        array->clear();
    }
  }
  void prepare(const Message &message) {
    prepare(message.replay);
    prepare(message.checkpoint_state);
    for (auto &call : message.calls)
      prepare(call.arguments);
  }
  void retire(Message &message) noexcept {
    retire(message.replay);
    retire(message.checkpoint_state);
    for (auto &call : message.calls)
      retire(call.arguments);
  }
};
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
// Immutable snapshot provenance. Pending payloads are shared, never copied on acknowledgement.
struct SessionProjection {
  struct Mapping {
    std::string message_id, original_reference, portable_reference;
    size_t source_position = 0;
  };
  struct Recovery {
    struct Provisional {
      std::string message_id, status;
    };
    std::string source, journal;
    std::vector<std::string> baseline;
    std::set<std::string> calls;
    std::map<std::string, Provisional> provisional;
  };
  struct Pending {
    std::string source, normalized; // Stored source bytes and their read-only normalization.
    std::shared_ptr<const std::string> payload;
    std::set<std::string> references;
  };
  std::string session;
  std::map<size_t, Mapping> mappings;
  std::map<size_t, size_t> source_positions;
  std::map<size_t, std::shared_ptr<const Pending>> pending;
  std::string blob_fingerprint;
  std::shared_ptr<const Recovery> recovery;
};
struct StateStore::Impl {
  Impl() = default;
  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;
  struct SessionVersion {
    std::string token;
    int schema = 0;
    bool present = false;
    bool operator==(const SessionVersion &other) const {
      return token == other.token && schema == other.schema && present == other.present;
    }
  };
  sqlite3 *db = nullptr;
  fs::path home, artifact_root;
  mutable fs::path artifact_anchor;
  NativeFileSystem native_files;
  FileSystem *files = &native_files;
  DiagnosticSink diagnostics;
  std::shared_ptr<FileDiagnosticReceiver> file_diagnostics;
  void report_file(std::unique_ptr<FileNotice> notice) const noexcept {
    if (!notice)
      return;
    if (file_diagnostics) {
      file_diagnostics->submit(std::move(notice));
      return;
    }
    try {
      const auto text = notice->text();
      if (diagnostics)
        diagnostics(text);
      else
        report_diagnostic({}, text);
      return;
    } catch (...) {
      // The file and its held path survive a failed renderer.
      emergency_file_diagnostic(*notice);
    }
  }
  bool publication_confirmed(const fs::path &path, const std::string &bytes) const noexcept {
    try {
      return files->read(path, bytes.size()) == bytes;
    } catch (...) {
      return false;
    }
  }
  void require_session_token(const std::string &id) {
    (void)session_version(id);
  }
  using Backup = std::unique_ptr<FileNotice>;
  Json raw_backup(const std::string &id) const {
    Json backup = {{"session", id}, {"kind", "session_repair_backup"}};
    for (const auto *table : {"sessions", "session_messages", "provider_parts", "tools",
                              "session_references", "provider_revisions", "session_revisions"}) {
      const auto query = std::string("SELECT * FROM ") + table +
                         (std::string(table) == "sessions" ? " WHERE id=?" : " WHERE session=?");
      Statement rows(db, query.c_str());
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
        sql_read_error(db, "Cannot back up session repair rows", status);
    }
    return backup;
  }
  void write_backup(std::vector<Backup> &backups, const std::string &id, const Json &backup) {
    const auto bytes = backup.dump();
    backups.push_back(std::make_unique<FileNotice>(
        (repair_directory(home) / (id + "-backup-" + unique_id() + ".json")).string(),
        FileNoticeKind::repair_backup));
    auto &record = *backups.back();
    try {
      files->atomic_write(record.path, bytes, true);
      record.confirmed = true;
    } catch (...) {
      record.confirmed = publication_confirmed(record.path, bytes);
      throw;
    }
  }
  void report_backups(std::vector<Backup> &backups) const noexcept {
    for (auto &backup : backups)
      report_file(std::move(backup));
  }
  bool anchor() const {
    if (!artifact_anchor.empty())
      return true;
    try {
      artifact_anchor = files->canonical(artifact_root);
      return true;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      return false; // Unavailable artifacts do not prevent database-only operations.
    }
  }
  void portable(std::vector<Message> &messages, const std::string &id,
                const std::string &tool = "") const {
    portable_messages(messages, id, tool);
    ensure_message_ids(messages);
    std::map<std::string, std::string> references;
    for (auto &message : messages)
      project(message, references);
  }
  void project(Message &message, std::map<std::string, std::string> &references) const {
    const auto *ref = absolute_checkpoint_reference(message);
    if (!ref)
      return;
    const auto original = *ref;
    auto found = references.find(original);
    if (found == references.end()) {
      Message normalized;
      normalized.checkpoint_state = message.checkpoint_state;
      const bool valid = anchor() && normalize_checkpoint_reference(normalized, *files,
                                                                    artifact_root, artifact_anchor);
      found =
          references
              .emplace(original,
                       valid ? normalized.checkpoint_state.at("recovery_record").get<std::string>()
                             : original)
              .first;
    }
    message.checkpoint_state["recovery_record"] = found->second;
  }
  static void known_projection(Message &message, size_t position,
                               const std::shared_ptr<const SessionProjection> &projection,
                               const std::string &id) {
    if (!projection || projection->session != id)
      return;
    const auto *reference = absolute_checkpoint_reference(message);
    if (!reference)
      return;
    const auto apply = [&](size_t coordinate) {
      const auto row = projection->mappings.find(coordinate);
      if (row == projection->mappings.end() || row->second.message_id != message.message_id ||
          *reference != row->second.original_reference)
        return false;
      message.checkpoint_state["recovery_record"] = row->second.portable_reference;
      return true;
    };
    if (apply(position))
      return;
    const auto source = projection->source_positions.find(position);
    if (source != projection->source_positions.end())
      (void)apply(source->second);
  }
  static void known_projections(Session &session,
                                const std::shared_ptr<const SessionProjection> &projection) {
    if (!projection || projection->session != session.id)
      return;
    for (size_t i = 0; i < session.messages.size(); ++i)
      known_projection(session.messages[i], i, projection, session.id);
  }
  static Message lexical(Message message, const std::string &id, size_t position,
                         const std::string &tool) {
    portable_message(message, id, tool);
    for (auto &call : message.calls)
      call.arguments = safe_json(std::move(call.arguments));
    ensure_message_id(message, position);
    return message;
  }
  void require_owned_write() const {
    if (!sqlite3_get_autocommit(db))
      throw Error("Transcript writes require their own transaction");
  }
  bool session_exists(const std::string &id) const {
    Statement exists(db, "SELECT 1 FROM sessions WHERE id=?");
    exists.text(1, id);
    const auto status = sqlite3_step(exists.p);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
      sql_read_error(db, "Cannot inspect session existence", status);
    return status == SQLITE_ROW;
  }
  SessionVersion session_version(const std::string &id, bool allow_invalid = false) {
    verify_revisions();
    Statement q(db, "SELECT revision FROM session_revisions WHERE session=?");
    q.text(1, id);
    const auto status = sqlite3_step(q.p);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
      sql_read_error(db, "Cannot inspect session revision", status);
    const auto token = status == SQLITE_ROW ? column(q.p, 0) : std::string{};
    if (status == SQLITE_DONE && !allow_invalid && !session_exists(id))
      return {token, verified_schema, false};
    if (!allow_invalid && (status != SQLITE_ROW || !valid_revision_token(token)))
      throw InvalidSessionToken("Invalid session revision token; explicit session repair required");
    return {token, verified_schema, status == SQLITE_ROW};
  }
  bool session_current(const std::string &id, const SessionVersion &version,
                       bool allow_invalid = false) {
    return session_version(id, allow_invalid) == version;
  }
  enum class WriteKind : std::uint8_t { append, revise, replace };
  struct PreparedRow {
    Message message;
    std::shared_ptr<const std::string> payload;
    std::set<std::string> references;
  };
  enum class MemoryPublication : std::uint8_t { publish, discard };
  enum class CanonicalInput : std::uint8_t { normalize, ready };
  enum class RecoveryInput : std::uint8_t { prepare, verified };
  struct PrepareOptions {
    WriteKind kind = WriteKind::append;
    size_t first = 0;
    std::string tool;
    MemoryPublication memory = MemoryPublication::publish;
    CanonicalInput input = CanonicalInput::normalize;
    RecoveryInput recovery = RecoveryInput::prepare;
    const Message *suffix = nullptr; // Prepared append overlay; caller history stays untouched.
  };
  struct PreparedJournal {
    std::string id, name, args, status;
    std::shared_ptr<const std::string> payload;
    std::string expected_status = {};
  };
  struct CommitReceipt {
    std::string session;
    std::weak_ptr<const SessionProjection> projection;
    SessionVersion version;
    std::vector<std::string> input, canonical;
  };
  std::shared_ptr<CommitReceipt> receipt;
  mutable uint64_t transcript_rows = 0;
  struct PreparedTranscript {
    WriteKind kind = WriteKind::append;
    size_t first = 0;
    bool blob = false;
    std::string blob_fingerprint;
    SessionVersion version;
    bool legacy_rows = false;
    std::map<std::string, PreparedJournal> journal;
    std::map<size_t, PreparedRow> rows;
    std::map<size_t, std::set<std::string>> expected;
    std::shared_ptr<const SessionProjection> committed;
    JsonRetirement retirement;
    std::shared_ptr<CommitReceipt> receipt;
    std::vector<std::string> validated_input;
    std::vector<std::string> validated_canonical;
  };
  bool receipt_matches(const Session &s, const SessionVersion &version) const {
    return receipt && receipt->session == s.id &&
           receipt->projection.lock() == s.checkpoint_projection && receipt->version == version;
  }
  static bool receipt_fits(const Session &s) {
    constexpr size_t receipt_limit = size_t{8} * 1024 * 1024;
    const auto overhead = sizeof(CommitReceipt) + s.id.size() + 32 + 2 * sizeof(void *);
    return s.checkpoint_projection && s.checkpoint_projection->recovery &&
           overhead <= receipt_limit &&
           s.messages.size() <=
               (receipt_limit - overhead) / (size_t{2} * 64 + size_t{2} * sizeof(std::string));
  }
  void prepare_receipt(const Session &s, PreparedTranscript &plan) const {
    if (!receipt_fits(s))
      return;
    try {
      auto next = std::make_shared<CommitReceipt>();
      next->session = s.id;
      next->projection = s.checkpoint_projection;
      next->input = std::move(plan.validated_input);
      next->input.reserve(s.messages.size());
      for (size_t i = next->input.size(); i < s.messages.size(); ++i)
        next->input.push_back(message_fingerprint(s.messages[i]));
      next->canonical.reserve(s.messages.size());
      const bool reuse = receipt_matches(s, plan.version);
      std::map<std::string, std::string> names;
      for (size_t i = 0; i < s.messages.size(); ++i) {
        for (const auto &call : s.messages[i].calls)
          names[call.id] = call.name;
        const auto row = plan.rows.find(i);
        if (row != plan.rows.end())
          next->canonical.push_back(digest(*row->second.payload));
        else if (i < plan.validated_canonical.size())
          next->canonical.push_back(plan.validated_canonical[i]);
        else if (reuse && i < receipt->canonical.size())
          next->canonical.push_back(receipt->canonical[i]);
        else {
          auto message = lexical(s.messages[i], s.id, i, names[s.messages[i].tool_call_id]);
          known_projection(message, i, s.checkpoint_projection, s.id);
          next->canonical.push_back(digest(safe_json(Json(message)).dump()));
        }
      }
      plan.receipt = std::move(next);
    } catch (const std::bad_alloc &) {
      // Optional knowledge cannot turn a valid write into an allocation failure.
      return;
    }
  }
  void capture_receipt(const std::string &id, PreparedTranscript &plan) {
    if (plan.receipt)
      plan.receipt->version = session_version(id);
  }
  void publish_receipt(PreparedTranscript &plan) noexcept {
    receipt = std::move(plan.receipt);
  }
  template <class Prepare, class Write, class Publish>
  void publish_retry(const std::string &id, Prepare prepare_plan, Write write, Publish publish,
                     const char *exhausted) {
    static_assert(noexcept(publish(std::declval<PreparedTranscript &>())),
                  "Commit acknowledgement must not throw");
    for (int attempt = 0; attempt < 2; ++attempt) {
      auto plan = prepare_plan();
      {
        Transaction transaction(db);
        if (!prepared_current(id, plan))
          continue;
        write(plan);
        capture_receipt(id, plan);
        transaction.commit();
      }
      publish_receipt(plan);
      publish(plan);
      return;
    }
    throw Error(exhausted);
  }
  std::optional<std::string> stored_payload(const std::string &id, size_t position,
                                            Statement &row) const {
    sqlite3_reset(row.p);
    row.text(1, id);
    sqlite3_bind_int64(row.p, 2, static_cast<sqlite3_int64>(position));
    const auto status = sqlite3_step(row.p);
    if (status == SQLITE_DONE)
      return std::nullopt;
    if (status != SQLITE_ROW)
      sql_read_error(db, "Cannot inspect checkpoint normalization", status);
    auto payload = column(row.p, 0);
    sqlite3_reset(row.p);
    return payload;
  }
  struct TranscriptRows {
    std::vector<Message> messages;
    std::map<size_t, std::string> sources;
    std::string signature;
  };
  TranscriptRows read_rows(const std::string &id, bool sources, bool signature) const {
    TranscriptRows out;
    std::string fingerprints;
    Statement rows(
        db, "SELECT position,payload FROM session_messages WHERE session=? ORDER BY position");
    rows.text(1, id);
    int status;
    while ((status = sqlite3_step(rows.p)) == SQLITE_ROW) {
      ++transcript_rows;
      if (sqlite3_column_int64(rows.p, 0) != static_cast<sqlite3_int64>(out.messages.size()))
        throw StoredDataCorruption("Corrupt session messages: noncontiguous positions");
      auto bytes = column(rows.p, 1);
      auto message = session_json<Message>(bytes);
      const auto fingerprint = signature || (sources && absolute_checkpoint_reference(message))
                                   ? digest(bytes)
                                   : std::string{};
      if (signature)
        fingerprints += fingerprint;
      if (sources && absolute_checkpoint_reference(message))
        out.sources.emplace(out.messages.size(), fingerprint);
      out.messages.push_back(std::move(message));
    }
    if (status != SQLITE_DONE)
      sql_read_error(db, "Cannot load session messages", status);
    if (signature)
      out.signature = "rows:" + digest(fingerprints);
    return out;
  }
  PreparedTranscript prepare(const Session &s, std::vector<size_t> positions,
                             const PrepareOptions &options) {
    const auto message_count = s.messages.size() + (options.suffix ? 1u : 0u);
    const auto input_at = [&](size_t position) -> const Message & {
      return options.suffix && position == s.messages.size() ? *options.suffix
                                                             : s.messages.at(position);
    };
    const auto kind = options.kind;
    const auto first = options.first;
    const auto &tool = options.tool;
    const bool publish_memory = options.memory == MemoryPublication::publish;
    const bool canonical = options.input == CanonicalInput::ready;
    if (options.recovery == RecoveryInput::prepare && s.checkpoint_projection &&
        s.checkpoint_projection->session == s.id && s.checkpoint_projection->recovery &&
        kind != WriteKind::replace)
      return prepare_recovered(s, std::move(positions), options);
    if (kind == WriteKind::revise) {
      std::sort(positions.begin(), positions.end());
      positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    }
    PreparedTranscript plan;
    plan.kind = kind;
    plan.first = first;
    const bool legacy_snapshot = options.recovery != RecoveryInput::verified &&
                                 s.checkpoint_projection &&
                                 s.checkpoint_projection->session == s.id &&
                                 !s.checkpoint_projection->blob_fingerprint.empty();
    Transaction snapshot(db, false);
    plan.version = session_version(s.id);
    bool legacy_rows = false;
    std::map<size_t, Message> materialized;
    {
      Statement format(db, "SELECT message_format,messages FROM sessions WHERE id=?");
      format.text(1, s.id);
      const auto status = sqlite3_step(format.p);
      if (status != SQLITE_ROW && status != SQLITE_DONE)
        sql_read_error(db, "Cannot inspect transcript storage", status);
      plan.blob = status == SQLITE_ROW && sqlite3_column_int(format.p, 0) == 0;
      legacy_rows = legacy_snapshot && !plan.blob;
      std::vector<Message> stored;
      if (plan.blob) {
        const auto bytes = column(format.p, 1);
        plan.blob_fingerprint = digest(bytes);
        if (legacy_snapshot && s.checkpoint_projection->blob_fingerprint != plan.blob_fingerprint)
          throw Error("Session modified by another process: legacy transcript changed");
        stored = session_json<std::vector<Message>>(bytes);
      } else if (legacy_rows && kind != WriteKind::replace) {
        plan.legacy_rows = true;
        stored = read_rows(s.id, false, false).messages;
      }
      if (plan.blob || plan.legacy_rows) {
        portable_messages(stored, s.id);
        ensure_message_ids(stored);
        for (size_t i = 0; i < stored.size(); ++i)
          known_projection(stored[i], i, s.checkpoint_projection, s.id);
        const std::set<size_t> requested(positions.begin(), positions.end());
        if ((kind == WriteKind::append && stored.size() != first) ||
            (kind == WriteKind::revise && stored.size() != message_count))
          throw Error("Session changed before legacy transcript write");
        std::map<std::string, std::string> context;
        for (size_t i = 0; i < message_count; ++i) {
          const auto &input = input_at(i);
          for (const auto &call : input.calls)
            context[call.id] = call.name;
          const bool equal = i < stored.size() && same_message(input, stored[i]);
          if (equal && !plan.blob && !requested.count(i))
            continue;
          auto message = equal ? input : lexical(input, s.id, i, context[input.tool_call_id]);
          known_projection(message, i, s.checkpoint_projection, s.id);
          if (kind != WriteKind::replace && (kind == WriteKind::append || !requested.count(i)) &&
              i < stored.size() && !same_message(message, stored[i]))
            throw Error("Unrequested legacy transcript message changed");
          if (plan.blob || requested.count(i) ||
              (publish_memory && !same_message(input, message))) {
            if (!plan.blob && !requested.count(i))
              positions.push_back(i);
            materialized.emplace(i, std::move(message));
          }
        }
        if (plan.blob) {
          positions.clear();
          for (size_t i = 0; i < message_count; ++i)
            positions.push_back(i);
        }
      }
    }
    const auto previous = s.checkpoint_projection;
    bool provenance_changed = kind == WriteKind::replace || plan.blob || legacy_rows ||
                              (previous && (!previous->pending.empty() || previous->recovery));
    for (const auto position : positions)
      provenance_changed = provenance_changed ||
                           absolute_checkpoint_reference(input_at(position)) ||
                           (previous && previous->mappings.count(position));
    auto compact = std::make_shared<SessionProjection>();
    compact->session = s.id;
    const std::set<size_t> requested(positions.begin(), positions.end());
    if (provenance_changed && previous && previous->session == s.id) {
      compact->mappings = previous->mappings;
      if (!plan.blob)
        for (const auto &row : previous->pending) {
          plan.expected[row.first] = {row.second->source, row.second->normalized};
          if (kind != WriteKind::replace && !requested.count(row.first)) {
            if (row.first >= message_count)
              throw Error("Pending checkpoint message missing from snapshot");
            auto message = session_json<Message>(*row.second->payload);
            auto input = input_at(row.first);
            known_projection(input, row.first, previous, s.id);
            if (!same_message(input, message))
              throw Error("Unrequested checkpoint message changed");
            plan.rows.emplace(row.first, PreparedRow{std::move(message), row.second->payload,
                                                     row.second->references});
          }
        }
    }
    std::map<std::string, std::string> names, references;
    std::map<std::string, const ToolCall *> calls;
    if (!plan.blob && !plan.legacy_rows && !canonical) {
      std::set<std::string> needed;
      for (const auto position : positions) {
        const auto &message = input_at(position);
        if (message.role == "tool" && (tool.empty() || position != first))
          needed.insert(message.tool_call_id);
      }
      for (auto message = s.messages.rbegin(); message != s.messages.rend() && !needed.empty();
           ++message) {
        for (const auto &call : message->calls)
          if (needed.erase(call.id)) {
            names.emplace(call.id, call.name);
            calls.emplace(call.id, &call);
          }
      }
    }
    Statement source(db, "SELECT payload FROM session_messages WHERE session=? AND position=?");
    const auto expect_source = [&](size_t position) {
      if (!plan.blob && !plan.expected.count(position)) {
        const auto stored = stored_payload(s.id, position, source);
        plan.expected[position].insert(stored ? digest(*stored) : std::string{});
      }
    };
    if (kind == WriteKind::replace)
      for (const auto &mapping : compact->mappings)
        expect_source(mapping.first);
    for (const auto position : positions) {
      if (compact->mappings.count(position) || plan.legacy_rows)
        expect_source(position);
      const auto &input = input_at(position);
      auto message =
          (plan.blob || plan.legacy_rows) ? std::move(materialized.at(position))
          : canonical
              ? input
              : lexical(input, s.id, position,
                        !tool.empty() && position == first ? tool : names[input.tool_call_id]);
      known_projection(message, position, previous, s.id);
      const auto *absolute = absolute_checkpoint_reference(message);
      const auto original = absolute ? *absolute : std::string{};
      project(message, references);
      if (!original.empty() && !absolute_checkpoint_reference(message)) {
        compact->mappings[position] = {
            message.message_id, original,
            message.checkpoint_state.at("recovery_record").get<std::string>(), position};
        expect_source(position);
      }
      auto payload = std::make_shared<const std::string>(safe_json(Json(message)).dump());
      auto paths = message_references(message);
      plan.rows.insert_or_assign(
          position, PreparedRow{std::move(message), std::move(payload), std::move(paths)});
    }
    for (auto row = compact->mappings.begin(); row != compact->mappings.end();) {
      const auto position = row->first;
      const auto prepared = plan.rows.find(position);
      const Message *message = prepared != plan.rows.end() ? &prepared->second.message
                               : position < message_count  ? &input_at(position)
                                                           : nullptr;
      if (!message || !message->checkpoint_state.is_object() ||
          message->message_id != row->second.message_id ||
          message->checkpoint_state.value("recovery_record", std::string{}) !=
              row->second.portable_reference)
        row = compact->mappings.erase(row);
      else
        ++row;
    }
    // Const snapshots may repeat an already committed identical normalization or explicit write.
    for (auto &entry : plan.expected) {
      const auto target = plan.rows.find(entry.first);
      if (target != plan.rows.end())
        entry.second.insert(digest(*target->second.payload));
    }
    for (const auto &entry : plan.rows)
      plan.retirement.prepare(publish_memory ? input_at(entry.first) : entry.second.message);
    for (auto &mapping : compact->mappings)
      mapping.second.source_position = mapping.first;
    plan.committed = provenance_changed ? std::move(compact) : previous;
    prepare_provisional_journal(plan, s, calls);
    snapshot.commit();
    return plan;
  }
  bool prepared_current(const std::string &id, const PreparedTranscript &plan) {
    return session_current(id, plan.version);
  }
  void apply_prepared_transcript(const Session &s, const PreparedTranscript &plan) {
    apply_prepared_journal(s.id, plan.journal);
    if (plan.blob) {
      Statement blob(db, "SELECT message_format,messages FROM sessions WHERE id=?");
      blob.text(1, s.id);
      const auto status = sqlite3_step(blob.p);
      if (status != SQLITE_ROW || sqlite3_column_int(blob.p, 0) != 0 ||
          digest(column(blob.p, 1)) != plan.blob_fingerprint)
        throw Error("Session modified by another process: legacy transcript changed");
    }
    Statement current(db, "SELECT payload FROM session_messages WHERE session=? AND position=?");
    std::map<size_t, std::optional<std::string>> observed;
    for (const auto &entry : plan.expected) {
      auto payload = stored_payload(s.id, entry.first, current);
      const bool already_removed =
          plan.kind == WriteKind::replace && entry.first >= s.messages.size() && !payload;
      if (!already_removed && !entry.second.count(payload ? digest(*payload) : std::string{}))
        throw Error("Session modified by another process: checkpoint row changed");
      observed.emplace(entry.first, std::move(payload));
    }
    const bool replace = plan.blob || plan.kind == WriteKind::replace;
    if (replace)
      reset_message_rows(db, s.id);
    else if (plan.kind == WriteKind::append) {
      Statement count(db, "SELECT count(*) FROM session_messages WHERE session=?");
      count.text(1, s.id);
      if (sqlite3_step(count.p) != SQLITE_ROW ||
          sqlite3_column_int64(count.p, 0) != static_cast<sqlite3_int64>(plan.first))
        throw Error("Session changed before message append");
    }
    Statement insert(db, "INSERT INTO session_messages(session,position,payload) VALUES(?,?,?)");
    Statement update(db, "UPDATE session_messages SET payload=? WHERE session=? AND position=?");
    std::set<std::string> paths;
    for (const auto &entry : plan.rows) {
      const auto &row = entry.second;
      if (replace || (plan.kind == WriteKind::append && entry.first >= plan.first)) {
        insert_message_row(insert, s.id, entry.first, *row.payload);
        serialized += row.payload->size();
      } else if (!observed.count(entry.first) || !observed.at(entry.first) ||
                 *observed.at(entry.first) != *row.payload) {
        sqlite3_reset(update.p);
        update.text(1, *row.payload);
        update.text(2, s.id);
        sqlite3_bind_int64(update.p, 3, static_cast<sqlite3_int64>(entry.first));
        update.run();
        if (sqlite3_changes(db) != 1)
          throw Error("Session message missing during revision");
        serialized += row.payload->size();
      }
      paths.insert(row.references.begin(), row.references.end());
    }
    remember_paths(s.id, std::move(paths));
  }
  static void publish_transcript(Session &s, PreparedTranscript &plan) noexcept {
    static_assert(std::is_nothrow_swappable<Message>::value, "Message publication must not throw");
    for (auto &row : plan.rows)
      if (row.first < s.messages.size()) {
        std::swap(s.messages[row.first], row.second.message);
        plan.retirement.retire(row.second.message);
      }
    s.checkpoint_projection = std::move(plan.committed);
  }
  static void discard_prepared(PreparedTranscript &plan) noexcept {
    for (auto &row : plan.rows)
      plan.retirement.retire(row.second.message);
  }
  struct JournalEntry {
    std::string result, status, name, args;
  };
  static std::optional<JournalEntry> read_journal_entry(Statement &row, const std::string &id,
                                                        const std::string &call_id) {
    sqlite3_reset(row.p);
    row.text(1, id);
    row.text(2, call_id);
    const auto status = sqlite3_step(row.p);
    if (status == SQLITE_ROW)
      return JournalEntry{column(row.p, 0), column(row.p, 1), column(row.p, 2), column(row.p, 3)};
    if (status != SQLITE_DONE)
      sql_read_error(sqlite3_db_handle(row.p), "Cannot read tool journal", status);
    return std::nullopt;
  }
  static void validate_journal_call(const JournalEntry &entry, const ToolCall &call) {
    try {
      if (entry.name != call.name || stored_json(entry.args) != safe_json(call.arguments))
        throw Error("Ambiguous journaled tool call id");
    } catch (const Json::exception &) {
      throw Error("Corrupt journaled tool arguments");
    }
  }
  void prepare_provisional_journal(PreparedTranscript &plan, const Session &s,
                                   const std::map<std::string, const ToolCall *> &known) {
    if (std::none_of(plan.rows.begin(), plan.rows.end(),
                     [](const auto &row) { return row.second.message.role == "tool"; }))
      return;
    std::map<std::string, JournalEntry> pending;
    Statement row(db, "SELECT status,name,args FROM tools WHERE session=? AND call_id=? "
                      "AND status IN ('uncertain','not_started')");
    for (const auto &entry : plan.rows) {
      const auto &message = entry.second.message;
      if (message.role != "tool")
        continue;
      sqlite3_reset(row.p);
      row.text(1, s.id);
      row.text(2, message.tool_call_id);
      const auto status = sqlite3_step(row.p);
      if (status == SQLITE_ROW) {
        JournalEntry record{{}, column(row.p, 0), column(row.p, 1), column(row.p, 2)};
        if (!pending.emplace(message.tool_call_id, std::move(record)).second)
          throw Error("Ambiguous duplicate tool results in session");
      } else if (status != SQLITE_DONE)
        sql_read_error(db, "Cannot inspect provisional tool journal", status);
    }
    if (pending.empty())
      return;
    auto calls = known;
    std::set<std::string> needed;
    for (const auto &entry : pending)
      if (!calls.count(entry.first))
        needed.insert(entry.first);
    for (auto message = s.messages.rbegin(); message != s.messages.rend() && !needed.empty();
         ++message)
      for (const auto &call : message->calls)
        if (needed.erase(call.id))
          calls.emplace(call.id, &call);
    if (!needed.empty())
      throw Error("Orphan provisional tool result in session");
    for (const auto &entry : plan.rows) {
      const auto &message = entry.second.message;
      const auto record = pending.find(message.tool_call_id);
      if (message.role != "tool" || record == pending.end())
        continue;
      validate_journal_call(record->second, *calls.at(record->first));
      plan.journal.insert_or_assign(record->first,
                                    PreparedJournal{record->first, record->second.name,
                                                    record->second.args, record->second.status,
                                                    entry.second.payload, record->second.status});
    }
  }
  using Journal = std::map<std::string, JournalEntry>;
  Journal read_journal(const std::string &id, const std::vector<Message> &messages,
                       const std::vector<Message> *extra = nullptr) const {
    Journal journal;
    std::set<std::string> queried;
    Statement row(db, "SELECT result,status,name,args FROM tools WHERE session=? AND call_id=?");
    for (const auto *inputs : {&messages, extra}) {
      if (!inputs)
        continue;
      for (const auto &message : *inputs)
        for (const auto &call : message.calls) {
          if (!queried.insert(call.id).second)
            continue;
          auto entry = read_journal_entry(row, id, call.id);
          if (entry)
            journal.emplace(call.id, std::move(*entry));
        }
    }
    return journal;
  }
  struct Recovered {
    std::vector<Message> messages;
    std::map<std::string, std::pair<size_t, std::string>> writes;
    std::vector<size_t> origins;
  };
  static Recovered recover(const std::vector<Message> &messages, const Journal &journal,
                           const std::string &id,
                           const std::set<std::string> *historical = nullptr) {
    std::map<std::string, const ToolCall *> calls;
    std::map<std::string, const Message *> answers;
    for (const auto &message : messages) {
      if (message.role == "tool" &&
          (message.tool_call_id.empty() || !answers.emplace(message.tool_call_id, &message).second))
        throw Error("Ambiguous duplicate tool results in session");
      for (const auto &call : message.calls)
        if (message.role != "assistant" || call.id.empty() || !calls.emplace(call.id, &call).second)
          throw Error("Ambiguous duplicate tool call ids in session");
    }
    for (const auto &answer : answers)
      if (!calls.count(answer.first))
        throw Error("Orphan tool result in session");
    const auto synthetic = [](const Message &message) {
      const auto value = Json::parse(message.text, nullptr, false);
      if (!value.is_object() || !value.contains("metadata") || !value["metadata"].is_object())
        return false;
      const auto status = value["metadata"].value("recovery", Json());
      return status == "uncertain" || status == "not_started";
    };
    Recovered recovered;
    for (const auto &message : messages) {
      if (message.role == "tool")
        continue;
      recovered.origins.push_back(static_cast<size_t>(&message - messages.data()));
      recovered.messages.push_back(message);
      for (const auto &call : message.calls) {
        if (historical && !historical->count(call.id)) {
          // Calls added after this recovery snapshot are live, not interrupted history.
          if (answers.count(call.id)) {
            recovered.messages.push_back(*answers.at(call.id));
            recovered.origins.push_back(static_cast<size_t>(answers.at(call.id) - messages.data()));
          }
          continue;
        }
        const auto record = journal.find(call.id);
        const bool recorded = record != journal.end();
        const auto status = recorded ? record->second.status : "not_started";
        if (recorded)
          validate_journal_call(record->second, call);
        Message result;
        std::string outcome;
        if (recorded &&
            (status == "complete" || status == "uncertain" || status == "not_started")) {
          try {
            result = stored_json(record->second.result).get<Message>();
          } catch (const Json::exception &) {
            throw Error(status == "complete" ? "Corrupt completed tool record"
                                             : "Corrupt recovered tool record");
          }
          if (result.role != "tool" || result.tool_call_id != call.id)
            throw Error("Mismatched tool record");
        } else if (answers.count(call.id) && !synthetic(*answers.at(call.id))) {
          result = *answers.at(call.id);
          if (!recorded)
            outcome = "complete";
        } else if (!recorded && answers.count(call.id)) {
          result = *answers.at(call.id);
          outcome = Json::parse(result.text).at("metadata").at("recovery").get<std::string>();
        } else {
          outcome = recorded ? "uncertain" : "not_started";
          result.role = "tool";
          result.tool_call_id = call.id;
          result.text =
              Json({{"content",
                     recorded ? "Interrupted: completion unknown. Inspect actual state before "
                                "another action; this tool was not replayed."
                              : "Interrupted: this tool was not started and was not replayed."},
                    {"is_error", true},
                    {"metadata", {{"recovery", outcome}}}})
                  .dump();
        }
        if (answers.count(call.id)) {
          const auto &answer = *answers.at(call.id);
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
        portable_message(result, id, call.name);
        ensure_message_id(result, recovered.messages.size());
        if (!outcome.empty())
          recovered.writes.emplace(call.id,
                                   std::make_pair(recovered.messages.size(), std::move(outcome)));
        recovered.origins.push_back(answers.count(call.id)
                                        ? static_cast<size_t>(answers.at(call.id) - messages.data())
                                        : std::numeric_limits<size_t>::max());
        recovered.messages.push_back(std::move(result));
      }
    }
    return recovered;
  }
  struct MessageComparison {
    bool exact = true, non_tools = true;
  };
  static MessageComparison compare_messages(const std::vector<Message> &left,
                                            const std::vector<Message> &right) {
    MessageComparison out;
    out.exact = left.size() == right.size();
    bool aligned = left.size() == right.size();
    for (size_t i = 0; i < std::min(left.size(), right.size()); ++i) {
      const bool equal = same_message(left[i], right[i]);
      out.exact = out.exact && equal;
      if (left[i].role != right[i].role)
        aligned = false;
      if (left[i].role != "tool" && right[i].role != "tool" && !equal)
        out.non_tools = false;
    }
    if (aligned)
      return out;
    size_t a = 0, b = 0;
    out.non_tools = true;
    while (true) {
      while (a < left.size() && left[a].role == "tool")
        ++a;
      while (b < right.size() && right[b].role == "tool")
        ++b;
      if (a == left.size() || b == right.size()) {
        out.non_tools = a == left.size() && b == right.size();
        return out;
      }
      if (!same_message(left[a++], right[b++])) {
        out.non_tools = false;
        return out;
      }
    }
  }
  static bool same_messages(const std::vector<Message> &left, const std::vector<Message> &right) {
    if (left.size() != right.size())
      return false;
    for (size_t i = 0; i < left.size(); ++i)
      if (!same_message(left[i], right[i]))
        return false;
    return true;
  }
  static std::string journal_signature(const Journal &journal) {
    std::string out;
    for (const auto &entry : journal)
      for (const auto *part : {&entry.first, &entry.second.result, &entry.second.status,
                               &entry.second.name, &entry.second.args})
        out += std::to_string(part->size()) + ":" + digest(*part);
    return digest(out);
  }
  static std::string message_fingerprint(const Message &message) {
    return digest(safe_json(Json(message)).dump());
  }
  static std::vector<std::string> message_fingerprints(const std::vector<Message> &messages) {
    std::vector<std::string> out;
    out.reserve(messages.size());
    for (const auto &message : messages)
      out.push_back(message_fingerprint(message));
    return out;
  }
  static void prepare_journal(PreparedTranscript &plan, const std::vector<Message> &messages,
                              const std::map<std::string, std::pair<size_t, std::string>> &writes) {
    for (const auto &message : messages)
      for (const auto &call : message.calls) {
        const auto entry = writes.find(call.id);
        if (entry != writes.end())
          plan.journal.insert_or_assign(
              call.id,
              PreparedJournal{call.id, call.name, safe_json(call.arguments).dump(),
                              entry->second.second, plan.rows.at(entry->second.first).payload});
      }
  }
  static std::shared_ptr<const SessionProjection> recovered_projection(
      const std::string &id, const std::shared_ptr<const SessionProjection> &previous,
      const std::vector<Message> &before, const Recovered &recovered, std::string source,
      const Journal &journal, const Recovered *persisted = nullptr) {
    if (same_messages(before, recovered.messages))
      return previous;
    auto projection = std::make_shared<SessionProjection>();
    projection->session = id;
    if (previous && previous->session == id) {
      projection->blob_fingerprint = previous->blob_fingerprint;
      for (size_t i = 0; i < recovered.origins.size(); ++i) {
        const auto found = previous->mappings.find(recovered.origins[i]);
        if (found != previous->mappings.end() &&
            found->second.message_id == recovered.messages[i].message_id) {
          projection->mappings.emplace(i, found->second);
          if (found->second.source_position != i)
            projection->source_positions.emplace(found->second.source_position, i);
        }
      }
    }
    auto recovery = std::make_shared<SessionProjection::Recovery>();
    recovery->source = std::move(source);
    recovery->journal = journal_signature(journal);
    recovery->baseline = message_fingerprints(persisted ? persisted->messages : recovered.messages);
    for (const auto &message : recovered.messages)
      for (const auto &call : message.calls)
        recovery->calls.insert(call.id);
    for (const auto &entry : recovered.writes)
      recovery->provisional.emplace(
          entry.first,
          SessionProjection::Recovery::Provisional{
              recovered.messages.at(entry.second.first).message_id, entry.second.second});
    projection->recovery = std::move(recovery);
    return projection;
  }
  PreparedTranscript prepare_recovered(const Session &s, std::vector<size_t> positions,
                                       const PrepareOptions &options) {
    Transaction snapshot(db, false);
    const auto version = session_version(s.id);
    std::vector<std::string> input_fingerprints;
    if (receipt_matches(s, version)) {
      const auto &acknowledged = *receipt;
      const std::set<size_t> requested(positions.begin(), positions.end());
      if ((options.kind == WriteKind::append && options.first != acknowledged.canonical.size()) ||
          (options.kind == WriteKind::revise && s.messages.size() != acknowledged.canonical.size()))
        throw Error("Session changed before recovered transcript write");
      auto verified_positions = positions;
      bool retain = options.memory == MemoryPublication::discard && receipt_fits(s);
      if (retain)
        try {
          input_fingerprints.reserve(s.messages.size());
        } catch (const std::bad_alloc &) {
          retain = false;
        }
      bool matches = true;
      for (size_t i = 0; i < acknowledged.canonical.size(); ++i) {
        auto fingerprint = message_fingerprint(s.messages.at(i));
        const bool mismatch = (options.kind == WriteKind::append || !requested.count(i)) &&
                              fingerprint != acknowledged.input[i] &&
                              fingerprint != acknowledged.canonical[i];
        if (options.memory == MemoryPublication::publish && !requested.count(i) &&
            fingerprint != acknowledged.canonical[i])
          verified_positions.push_back(i);
        if (retain)
          input_fingerprints.push_back(std::move(fingerprint));
        if (mismatch) {
          matches = false;
          break; // Normalization may still establish equivalence on the strict path.
        }
      }
      if (matches) {
        auto verified = options;
        verified.recovery = RecoveryInput::verified;
        auto plan = prepare(s, std::move(verified_positions), verified);
        plan.validated_input = std::move(input_fingerprints);
        snapshot.commit();
        return plan;
      }
    }
    std::vector<Message> stored;
    bool blob = false;
    std::string signature;
    {
      Statement format(db, "SELECT message_format,messages FROM sessions WHERE id=?");
      format.text(1, s.id);
      if (sqlite3_step(format.p) != SQLITE_ROW)
        throw Error("Session not found before recovery publication");
      blob = sqlite3_column_int(format.p, 0) == 0;
      if (blob) {
        const auto bytes = column(format.p, 1);
        signature = "blob:" + digest(bytes);
        stored = session_json<std::vector<Message>>(bytes);
      } else {
        auto rows = read_rows(s.id, false, true);
        signature = std::move(rows.signature);
        stored = std::move(rows.messages);
      }
    }
    portable_messages(stored, s.id);
    ensure_message_ids(stored);
    Session view{s.id, s.name, std::move(stored)};
    known_projections(view, s.checkpoint_projection);
    const auto journal = read_journal(s.id, view.messages);
    auto recovered =
        recover(view.messages, journal, s.id, &s.checkpoint_projection->recovery->calls);
    const bool already_recovered =
        !blob && recovered.writes.empty() && same_messages(view.messages, recovered.messages);
    view.messages = std::move(recovered.messages);
    known_projections(view, s.checkpoint_projection);
    const auto &baseline = *s.checkpoint_projection->recovery;
    const bool original_inputs =
        signature == baseline.source && journal_signature(journal) == baseline.journal;
    std::map<std::string, std::string> names;
    for (const auto &message : s.messages)
      for (const auto &call : message.calls)
        names[call.id] = call.name;
    std::map<size_t, Message> normalized;
    const auto input_at = [&](size_t position) -> const Message & {
      const auto &input = s.messages.at(position);
      if (position < view.messages.size() && same_message(view.messages[position], input))
        return input;
      const auto cached = normalized.find(position);
      if (cached != normalized.end())
        return cached->second;
      auto message = lexical(input, s.id, position, names[input.tool_call_id]);
      known_projection(message, position, s.checkpoint_projection, s.id);
      return normalized.emplace(position, std::move(message)).first->second;
    };
    auto fingerprints = message_fingerprints(view.messages);
    std::string canonical_rows;
    for (const auto &fingerprint : fingerprints)
      canonical_rows += fingerprint;
    if (!original_inputs && (!already_recovered || signature != "rows:" + digest(canonical_rows)))
      throw Error("Session modified by another process: recovery source bytes changed");
    if (fingerprints.size() < baseline.baseline.size())
      throw Error("Session modified by another process: recovered history was shortened");
    for (size_t i = 0; i < baseline.baseline.size(); ++i)
      if (fingerprints[i] != baseline.baseline[i] &&
          (i >= s.messages.size() || !same_message(view.messages[i], input_at(i))))
        throw Error(original_inputs
                        ? "Recovered snapshot no longer matches its original inputs"
                        : "Session modified by another process: recovered inputs changed");
    const std::set<size_t> requested(positions.begin(), positions.end());
    if ((options.kind == WriteKind::append && options.first != view.messages.size()) ||
        (options.kind == WriteKind::revise && s.messages.size() != view.messages.size()))
      throw Error("Session changed before recovered transcript write");
    for (size_t i = 0; i < view.messages.size(); ++i)
      if ((options.kind == WriteKind::append || !requested.count(i)) &&
          !same_message(view.messages[i], input_at(i)))
        throw Error("Unrequested recovered transcript message changed");
    PrepareOptions publication = options;
    publication.recovery = RecoveryInput::verified;
    PreparedTranscript plan;
    if (already_recovered) {
      for (const auto &row : normalized)
        if (!requested.count(row.first))
          positions.push_back(row.first);
      plan = prepare(s, std::move(positions), publication);
      Statement row(db, "SELECT payload FROM session_messages WHERE session=? AND position=?");
      for (const auto &entry : normalized) {
        const auto source = stored_payload(s.id, entry.first, row);
        plan.expected[entry.first].insert(source ? digest(*source) : std::string{});
      }
    } else {
      Session working{s.id, s.name, s.messages};
      for (auto &entry : normalized)
        working.messages[entry.first] = std::move(entry.second);
      auto projection = std::make_shared<SessionProjection>();
      projection->session = s.id;
      projection->mappings = s.checkpoint_projection->mappings;
      working.checkpoint_projection = std::move(projection);
      publication.kind = WriteKind::replace;
      std::vector<size_t> all;
      for (size_t i = 0; i < working.messages.size(); ++i)
        all.push_back(i);
      plan = prepare(working, std::move(all), publication);
    }
    plan.version = version;
    prepare_journal(plan, s.messages, recovered.writes);
    plan.validated_input = std::move(input_fingerprints);
    if (options.memory == MemoryPublication::discard && receipt_fits(s))
      plan.validated_canonical = std::move(fingerprints);
    snapshot.commit();
    return plan;
  }
  void apply_prepared_journal(const std::string &id,
                              const std::map<std::string, PreparedJournal> &writes) {
    if (writes.empty())
      return;
    Statement write(
        db, "INSERT INTO tools(session,call_id,name,args,result,status) VALUES(?,?,?,?,?,?) "
            "ON CONFLICT(session,call_id) DO UPDATE SET "
            "result=excluded.result,status=excluded.status WHERE tools.status!='complete'");
    Statement refresh(db, "UPDATE tools SET result=? WHERE session=? AND call_id=? AND status=? "
                          "AND name=? AND args=?");
    for (const auto &item : writes) {
      const auto &entry = item.second;
      if (!entry.expected_status.empty()) {
        sqlite3_reset(refresh.p);
        refresh.text(1, *entry.payload);
        refresh.text(2, id);
        refresh.text(3, entry.id);
        refresh.text(4, entry.expected_status);
        refresh.text(5, entry.name);
        refresh.text(6, entry.args);
        refresh.run();
        if (sqlite3_changes(db) != 1)
          throw Error("Session modified by another process: provisional tool outcome changed");
        continue;
      }
      sqlite3_reset(write.p);
      write.text(1, id);
      write.text(2, entry.id);
      write.text(3, entry.name);
      write.text(4, entry.args);
      write.text(5, *entry.payload);
      write.text(6, entry.status);
      write.run();
    }
  }
  void apply_tool_result(const std::string &id, const ToolCall &call,
                         const std::string &payload) const {
    Statement q(db, "UPDATE tools SET status='complete',result=? WHERE session=? AND "
                    "call_id=? AND status='running'");
    q.text(1, payload);
    q.text(2, id);
    q.text(3, call.id);
    q.run();
    if (sqlite3_changes(db) != 1)
      throw Error("Session modified by another process: tool outcome is no longer pending");
  }
  std::set<std::string> message_paths(const std::vector<Message> &messages) const {
    auto paths = message_references(messages);
    std::map<std::string, std::string> references;
    for (const auto &message : messages)
      if (absolute_checkpoint_reference(message)) {
        Message checkpoint;
        checkpoint.checkpoint_state = message.checkpoint_state;
        project(checkpoint, references);
        if (!absolute_checkpoint_reference(checkpoint))
          paths.insert(checkpoint.checkpoint_state.at("recovery_record").get<std::string>());
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
    require_session_token(id);
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
    const auto schema_status = sqlite3_step(version.p);
    if (schema_status != SQLITE_ROW)
      sql_read_error(db, "Cannot inspect revision schema version", schema_status);
    const auto current = sqlite3_column_int(version.p, 0);
    if (current == verified_schema)
      return;
    clear_provider_cache();
    auto definitions = revision_schema();
    definitions.insert(session_revision_schema().begin(), session_revision_schema().end());
    for (const auto &definition : definitions) {
      Statement q(db, "SELECT sql FROM sqlite_master WHERE name=?");
      q.text(1, definition.first);
      const auto status = sqlite3_step(q.p);
      if (status != SQLITE_ROW && status != SQLITE_DONE) {
        const auto context = "Cannot inspect revision infrastructure " + definition.first;
        sql_read_error(db, context.c_str(), status);
      }
      if (status == SQLITE_DONE || column(q.p, 0) != definition.second)
        throw RevisionIntegrityError("Invalid revision infrastructure: " + definition.first);
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
    if (!valid_revision_token(token))
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
  std::optional<std::string> load_provider_token(const std::string &id) {
    try {
      return provider_token(id);
    } catch (const RevisionIntegrityError &) {
      clear_provider_cache();
      throw;
    } catch (...) {
      clear_provider_cache();
      return std::nullopt;
    }
  }
  void populate_provider(const Session &session, size_t bytes,
                         const std::optional<std::string> &token, bool publish) noexcept {
    if (token && publish)
      publish_provider(session, bytes, *token);
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
    : StateStore(home, artifact_root, durability, nullptr, {}) {}
StateStore::StateStore(FileSystem &files, const fs::path &home, const fs::path &artifact_root,
                       const std::string &durability)
    : StateStore(home, artifact_root, durability, &files, {}) {}
StateStore::StateStore(FileSystem &files, const fs::path &home, const fs::path &artifact_root,
                       const std::string &durability, const StartupOptions &startup)
    : StateStore(home, artifact_root, durability, &files, startup) {}
StateStore::StateStore(const fs::path &home, const fs::path &artifact_root,
                       const std::string &durability, FileSystem *files,
                       const StartupOptions &startup)
    : impl_(std::make_unique<Impl>()) {
  if (files)
    impl_->files = files;
  impl_->home = home;
  impl_->durability = durability;
  if (durability != "durable" && durability != "fast")
    throw Error("Invalid durability mode", 2);
  impl_->artifact_root =
      fs::absolute(artifact_root.empty() ? home / "artifacts" : artifact_root).lexically_normal();
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
    if (value > 8)
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
    struct stat migration_stat {};
    if (migration_fd < 0)
      throw Error("Cannot open database migration lock");
    migration_locks.descriptors.push_back(migration_fd);
    if (::fstat(migration_fd, &migration_stat) || !S_ISREG(migration_stat.st_mode) ||
        migration_stat.st_uid != ::geteuid() || ::fchmod(migration_fd, 0600) ||
        ::fcntl(migration_fd, F_SETFD, FD_CLOEXEC) < 0)
      throw Error("Database migration lock must be an owned regular file");
    bool notified = false;
    const auto notify_waiting = [&]() noexcept {
      try {
        report_diagnostic(startup.diagnostics, "Waiting for database migration");
      } catch (...) {
        return;
      }
    };
    for (;;) {
      if (startup.cancellation && startup.cancellation->requested.load())
        throw Error("Interrupted waiting for database migration", 130);
      if (::flock(migration_fd, LOCK_EX | LOCK_NB) == 0)
        break;
      if (errno == EINTR)
        throw Error("Interrupted waiting for database migration", 130);
      if (errno != EWOULDBLOCK && errno != EAGAIN)
        throw Error("Cannot acquire database migration lock");
      if (!notified && startup.diagnostics) {
        notified = true;
        notify_waiting();
      }
      // Existing constructors retain their interruptible blocking behavior.
      if (!startup.cancellation) {
        if (::flock(migration_fd, LOCK_EX) == 0)
          break;
        if (errno == EINTR)
          throw Error("Interrupted waiting for database migration", 130);
        throw Error("Cannot acquire database migration lock");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  };
  if (version() < 8) {
    acquire_migration();
    const auto old_version = version();
    if (old_version > 0 && old_version < 8) {
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
      const auto target = home / ("state.sqlite3.pre-v8-" + unique_id());
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
    if (version() < 8) {
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
            insert_reference_rows(impl_->db, column(sessions.p, 0), impl_->message_paths(parsed));
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
            insert_reference_rows(impl_->db, column(journal.p, 0), impl_->message_paths({parsed}));
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
      if (n < 8) {
        // Do not parse historical transcripts during this upgrade.
        for (const auto &definition : session_revision_schema()) {
          Statement existing(impl_->db, "SELECT sql FROM sqlite_master WHERE name=?");
          existing.text(1, definition.first);
          const auto status = sqlite3_step(existing.p);
          if (status == SQLITE_DONE)
            sql(impl_->db, definition.second.c_str());
          else if (status != SQLITE_ROW)
            sql_read_error(impl_->db, "Cannot inspect session revision infrastructure", status);
          else if (column(existing.p, 0) != definition.second)
            throw RevisionIntegrityError("Invalid session revision infrastructure: " +
                                         definition.first);
        }
        sql(impl_->db, "INSERT OR IGNORE INTO session_revisions(session,revision) "
                       "SELECT id,lower(hex(randomblob(16))) FROM sessions;PRAGMA user_version=8");
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
void StateStore::set_diagnostics(DiagnosticSink sink) {
  impl_->diagnostics = std::move(sink);
}
void StateStore::set_file_diagnostics(std::shared_ptr<FileDiagnosticReceiver> receiver) noexcept {
  impl_->file_diagnostics = std::move(receiver);
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
    Lease() = default;
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;
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
  update_metadata(s);
  return s;
}
Session StateStore::load(const std::string &selector, bool repair) {
  const auto id = select_session(impl_->db, selector, false);
  return load_id(id, repair);
}
Session StateStore::load_id(const std::string &id, bool repair) {
  ReadOptions options;
  options.repair = repair;
  return read_session(id, options);
}
Session StateStore::read_session(const std::string &id, const ReadOptions &options) {
  const bool repair = options.repair, provider_state = options.provider, project = options.project;
  const bool validate_revision = options.validate_revision;
  auto *source = options.source;
  Transaction snapshot(impl_->db, false);
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
  std::map<size_t, std::string> source_rows;
  std::string source_signature;
  auto projection = std::make_shared<SessionProjection>();
  projection->session = s.id;
  if (sqlite3_column_int(q.p, 7) == 0) {
    const auto blob = column(q.p, 2);
    s.messages = session_json<std::vector<Message>>(blob);
    if (repair || source)
      source_signature = "blob:" + digest(blob);
    if (project) {
      projection->blob_fingerprint = digest(blob);
      for (size_t i = 0; i < s.messages.size(); ++i)
        if (absolute_checkpoint_reference(s.messages[i]))
          source_rows.emplace(i, std::string{});
    }
  } else {
    auto rows = impl_->read_rows(s.id, project, repair || source);
    s.messages = std::move(rows.messages);
    source_rows = std::move(rows.sources);
    source_signature = std::move(rows.signature);
  }
  size_t provider_bytes = 0;
  bool normalized_provider = false;
  std::optional<std::string> provider_token;
  if (provider_state) {
    Statement parts(
        impl_->db, "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position");
    parts.text(1, s.id);
    impl_->clear_provider_cache();
    int status;
    size_t position = 0;
    while ((status = sqlite3_step(parts.p)) == SQLITE_ROW) {
      if (sqlite3_column_int64(parts.p, 0) != static_cast<sqlite3_int64>(position))
        corrupt_provider();
      ++impl_->rows_read;
      const auto payload = column(parts.p, 1);
      provider_bytes += payload.size();
      auto part = provider_json(payload);
      if (position == 0) {
        if (!part.is_null() && (!part.is_object() || part.contains("messages")))
          corrupt_provider();
        s.provider_state = provider_header(part);
      } else {
        normalized_provider = normalize_legacy_part(part, s.provider_state) || normalized_provider;
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
  }
  const auto journal = repair ? impl_->read_journal(s.id, s.messages) : Impl::Journal{};
  if (validate_revision)
    (void)impl_->session_version(s.id, true);
  // Capture every required input before optional population, which may lose the read transaction.
  if (provider_state && snapshot.owner)
    provider_token = impl_->load_provider_token(s.id);
  sqlite3_reset(q.p);
  if (snapshot.owner && sqlite3_get_autocommit(impl_->db)) {
    snapshot.finished = true; // An optional token failure ended the already captured read snapshot.
    provider_token.reset();
    impl_->clear_provider_cache();
  } else
    snapshot.commit();
  if (provider_state && snapshot.owner)
    impl_->populate_provider(s, provider_bytes, provider_token, !normalized_provider);
  portable_messages(s.messages, s.id);
  ensure_message_ids(s.messages);
  if (project) {
    std::map<std::string, std::string> references;
    for (const auto &entry : source_rows) {
      auto &message = s.messages.at(entry.first);
      const auto original = *absolute_checkpoint_reference(message);
      impl_->project(message, references);
      if (!absolute_checkpoint_reference(message)) {
        projection->mappings.emplace(
            entry.first, SessionProjection::Mapping{
                             message.message_id, original,
                             message.checkpoint_state.at("recovery_record").get<std::string>()});
        projection->mappings.at(entry.first).source_position = entry.first;
        if (projection->blob_fingerprint.empty()) {
          auto pending = std::make_shared<SessionProjection::Pending>();
          pending->payload = std::make_shared<const std::string>(safe_json(Json(message)).dump());
          pending->references = message_references(message);
          pending->source = entry.second; // Empty for blobs: the original row does not exist yet.
          pending->normalized = digest(*pending->payload);
          projection->pending.emplace(entry.first, std::move(pending));
        }
      }
    }
  }
  if (source)
    *source = source_signature;
  if (project && (!projection->mappings.empty() || !projection->blob_fingerprint.empty()))
    s.checkpoint_projection = projection;
  if (repair) {
    auto recovered = impl_->recover(s.messages, journal, s.id);
    if (project)
      s.checkpoint_projection =
          Impl::recovered_projection(s.id, s.checkpoint_projection, s.messages, recovered,
                                     std::move(source_signature), journal);
    s.messages = std::move(recovered.messages);
  }
  return s;
}
void StateStore::reconcile(Session &destination, bool persist) {
  if (persist)
    impl_->require_owned_write();
  auto owner = persist ? acquire(destination.id) : std::shared_ptr<void>{};
  for (int attempt = 0; attempt < 2; ++attempt) {
    Impl::SessionVersion version;
    // Provider history is borrowed from destination; only the transcript needs a working view.
    Session session{destination.id, destination.name, destination.messages};
    session.provider = destination.provider;
    session.model = destination.model;
    session.effort = destination.effort;
    session.checkpoint_projection = destination.checkpoint_projection;
    portable_messages(session.messages, session.id);
    ensure_message_ids(session.messages);
    impl_->known_projections(session, session.checkpoint_projection);
    Impl::Journal journal;
    bool stored_equal = true;
    std::string source_signature;
    Impl::Recovered persisted;
    {
      Transaction snapshot(impl_->db, false);
      version = impl_->session_version(session.id, !persist);
      ReadOptions read_options;
      read_options.provider = false;
      read_options.project = false;
      read_options.source = persist ? nullptr : &source_signature;
      Session stored{session.id, session.name, {}};
      if (persist || impl_->session_exists(session.id))
        stored = read_session(session.id, read_options);
      else
        source_signature = "rows:" + digest("");
      impl_->known_projections(stored, session.checkpoint_projection);
      if (persist) {
        const auto comparison = Impl::compare_messages(stored.messages, session.messages);
        if (!comparison.non_tools)
          throw Error("Session modified by another process: reload before recovery");
        stored_equal = comparison.exact;
      }
      journal =
          impl_->read_journal(session.id, session.messages, persist ? nullptr : &stored.messages);
      if (!persist)
        persisted = impl_->recover(stored.messages, journal, session.id);
      snapshot.commit();
    }
    auto recovered = impl_->recover(session.messages, journal, session.id);
    const bool unchanged = impl_->same_messages(recovered.messages, session.messages) &&
                           recovered.writes.empty() && stored_equal;
    if (!persist) {
      auto projection =
          Impl::recovered_projection(session.id, session.checkpoint_projection, session.messages,
                                     recovered, std::move(source_signature), journal, &persisted);
      destination.messages.swap(recovered.messages);
      destination.checkpoint_projection = std::move(projection);
      return;
    }
    session.messages = std::move(recovered.messages);
    std::vector<size_t> positions;
    if (!unchanged)
      for (size_t i = 0; i < session.messages.size(); ++i)
        positions.push_back(i);
    Impl::PrepareOptions options;
    options.kind = unchanged ? Impl::WriteKind::revise : Impl::WriteKind::replace;
    options.input = Impl::CanonicalInput::ready;
    auto prepared = impl_->prepare(session, std::move(positions), options);
    Impl::prepare_journal(prepared, session.messages, recovered.writes);
    JsonRetirement retirement;
    for (const auto &message : destination.messages)
      retirement.prepare(message);
    journal.clear();
    recovered.writes.clear();
    bool stale;
    {
      Transaction publication(impl_->db);
      stale = !impl_->session_current(session.id, version);
      if (!stale) {
        stale = !impl_->prepared_current(session.id, prepared);
      }
      if (!stale) {
        if (!unchanged)
          update_metadata(session);
        impl_->apply_prepared_transcript(session, prepared);
        if (!unchanged)
          save_provider_state(destination);
        publication.commit();
      }
    }
    if (stale) {
      if (attempt == 1)
        throw Error("Reconciliation preparation changed twice; no changes applied; retry recovery");
      continue;
    }
    impl_->publish_transcript(session, prepared);
    if (!unchanged)
      impl_->publish_provider(destination, impl_->pending_provider_bytes,
                              impl_->pending_provider_token);
    destination.messages.swap(session.messages);
    destination.checkpoint_projection.swap(session.checkpoint_projection);
    for (auto &message : session.messages)
      retirement.retire(message);
    return;
  }
}
Session StateStore::last(bool repair) {
  const auto id = select_session(impl_->db, "", true);
  return load_id(id, repair);
}
Session StateStore::fork(const Session &parent) {
  auto recovered = parent;
  reconcile(recovered, false);
  std::map<std::string, std::string> fork_references;
  for (auto &message : recovered.messages)
    impl_->project(message, fork_references);
  Session s{unique_id(), parent.name.empty() ? "" : parent.name + " (fork)", recovered.messages};
  s.provider = parent.provider;
  s.model = parent.model;
  s.effort = parent.effort;
  s.provider_state = parent.provider_state;
  auto owner = acquire(s.id);
  if (std::any_of(s.messages.begin(), s.messages.end(),
                  [](const auto &m) { return !m.artifact_index.empty(); }) &&
      !fs::exists(artifact_index_path(impl_->artifact_root, parent.id)))
    throw Error("Historical image index missing; restore the active artifact root");
  copy_image_history(impl_->artifact_root, *impl_->files, parent.id, s.id);
  for (auto &message : s.messages)
    if (!message.artifact_index.empty())
      message.artifact_index = artifact_index_reference(s.id);
  save(s);
  remember_references(s.id, s.messages);
  {
    JournalTransaction publication(impl_->db);
    impl_->require_session_token(s.id);
    Statement refs(impl_->db, "INSERT OR IGNORE INTO session_references(session,kind,path) "
                              "SELECT ?,kind,path FROM session_references WHERE session=?");
    refs.text(1, s.id);
    refs.text(2, parent.id);
    refs.run();
    publication.commit();
  }
  reconcile(s);
  {
    JournalTransaction publication(impl_->db);
    impl_->require_session_token(s.id);
    Statement q(impl_->db, "UPDATE sessions SET parent=? WHERE id=?");
    q.text(1, parent.id);
    q.text(2, s.id);
    q.run();
    publication.commit();
  }
  return s;
}
void StateStore::update_metadata(const Session &s) {
  auto owner = acquire(s.id);
  JournalTransaction transaction(impl_->db);
  impl_->require_session_token(s.id);
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
  transaction.commit();
}
void StateStore::save_provider_state(const Session &s,
                                     const std::optional<ProviderStateUpdate> &hint) {
  auto owner = acquire(s.id);
  Transaction tx(impl_->db);
  impl_->require_session_token(s.id);
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
uint64_t StateStore::transcript_rows_read() const {
  return impl_->transcript_rows;
}
size_t StateStore::provider_cache_bytes() const {
  return impl_->cached_bytes;
}
void StateStore::append_transcript(const Session &source, Session *destination, size_t first,
                                   bool provider_state,
                                   const std::optional<ProviderStateUpdate> &hint) {
  impl_->require_owned_write();
  if (first > source.messages.size())
    throw Error("Invalid message append position");
  auto owner = acquire(source.id);
  Impl::PrepareOptions options;
  options.first = first;
  options.memory =
      destination ? Impl::MemoryPublication::publish : Impl::MemoryPublication::discard;
  impl_->publish_retry(
      source.id,
      [&] {
        std::vector<size_t> positions;
        for (size_t i = first; i < source.messages.size(); ++i)
          positions.push_back(i);
        auto prepared = impl_->prepare(source, std::move(positions), options);
        if (!destination)
          impl_->prepare_receipt(source, prepared);
        return prepared;
      },
      [&](Impl::PreparedTranscript &prepared) {
        update_metadata(source);
        impl_->apply_prepared_transcript(source, prepared);
        if (provider_state)
          save_provider_state(source, hint);
      },
      [&](Impl::PreparedTranscript &prepared) noexcept {
        if (destination)
          impl_->publish_transcript(*destination, prepared);
        else
          impl_->discard_prepared(prepared);
        if (provider_state)
          impl_->publish_provider(source, impl_->pending_provider_bytes,
                                  impl_->pending_provider_token);
      },
      "Transcript preparation changed twice; no changes applied; retry write");
}
void StateStore::append_messages(Session &s, size_t first, bool provider_state,
                                 const std::optional<ProviderStateUpdate> &hint) {
  append_transcript(s, &s, first, provider_state, hint);
}
void StateStore::append_messages(const Session &s, size_t first, bool provider_state,
                                 const std::optional<ProviderStateUpdate> &hint) {
  append_transcript(s, nullptr, first, provider_state, hint);
}
void StateStore::revise_transcript(const Session &source, Session *destination,
                                   const std::vector<size_t> &positions) {
  impl_->require_owned_write();
  auto owner = acquire(source.id);
  Impl::PrepareOptions options;
  options.kind = Impl::WriteKind::revise;
  options.memory =
      destination ? Impl::MemoryPublication::publish : Impl::MemoryPublication::discard;
  impl_->publish_retry(
      source.id,
      [&] {
        auto prepared = impl_->prepare(source, positions, options);
        if (!destination)
          impl_->prepare_receipt(source, prepared);
        return prepared;
      },
      [&](Impl::PreparedTranscript &prepared) {
        impl_->apply_prepared_transcript(source, prepared);
      },
      [&](Impl::PreparedTranscript &prepared) noexcept {
        if (destination)
          impl_->publish_transcript(*destination, prepared);
        else
          impl_->discard_prepared(prepared);
      },
      "Transcript preparation changed twice; no changes applied; retry write");
}
void StateStore::revise_messages(Session &s, const std::vector<size_t> &positions) {
  revise_transcript(s, &s, positions);
}
void StateStore::revise_messages(const Session &s, const std::vector<size_t> &positions) {
  revise_transcript(s, nullptr, positions);
}
void StateStore::replace_transcript(const Session &source, Session *destination) {
  impl_->require_owned_write();
  auto owner = acquire(source.id);
  Impl::PrepareOptions options;
  options.kind = Impl::WriteKind::replace;
  options.memory =
      destination ? Impl::MemoryPublication::publish : Impl::MemoryPublication::discard;
  impl_->publish_retry(
      source.id,
      [&] {
        std::vector<size_t> positions;
        for (size_t i = 0; i < source.messages.size(); ++i)
          positions.push_back(i);
        return impl_->prepare(source, std::move(positions), options);
      },
      [&](Impl::PreparedTranscript &prepared) {
        update_metadata(source);
        impl_->apply_prepared_transcript(source, prepared);
        save_provider_state(source);
      },
      [&](Impl::PreparedTranscript &prepared) noexcept {
        if (destination)
          impl_->publish_transcript(*destination, prepared);
        else
          impl_->discard_prepared(prepared);
        impl_->publish_provider(source, impl_->pending_provider_bytes,
                                impl_->pending_provider_token);
      },
      "Transcript preparation changed twice; no changes applied; retry write");
}
void StateStore::replace_history(Session &s) {
  replace_transcript(s, &s);
}
void StateStore::replace_history(const Session &s) {
  replace_transcript(s, nullptr);
}
void StateStore::save(Session &s) {
  replace_transcript(s, &s);
}
void StateStore::save(const Session &s) {
  replace_transcript(s, nullptr);
}
void StateStore::complete_tool(Session &s, const ToolCall &call, Message result) {
  impl_->require_owned_write();
  auto owner = acquire(s.id);
  try {
    if (result.role != "tool" || result.tool_call_id != call.id)
      throw Error("Mismatched tool completion result");
    Session working;
    size_t position = 0;
    bool already_complete = false;
    JsonRetirement retirement;
    const bool has_result =
        std::any_of(s.messages.begin(), s.messages.end(), [&](const Message &m) {
          return m.role == "tool" && m.tool_call_id == call.id;
        });
    const bool overlay =
        !has_result && (!s.checkpoint_projection || !s.checkpoint_projection->recovery);
    impl_->publish_retry(
        s.id,
        [&] {
          if (!overlay)
            working = Session{s.id, s.name, s.messages};
          working.provider = s.provider;
          working.model = s.model;
          working.effort = s.effort;
          working.checkpoint_projection = s.checkpoint_projection;
          position = overlay ? s.messages.size() : working.messages.size();
          for (size_t i = 0; i < working.messages.size(); ++i)
            if (working.messages[i].role == "tool" && working.messages[i].tool_call_id == call.id) {
              if (position != working.messages.size())
                throw Error("Ambiguous duplicate tool results in session");
              position = i;
            }
          if (!overlay) {
            auto completed = result;
            if (position < working.messages.size()) {
              completed.message_id = working.messages[position].message_id;
              working.messages[position] = std::move(completed);
            } else
              working.messages.push_back(std::move(completed));
          }
          Impl::PreparedTranscript prepared;
          already_complete = false;
          {
            Transaction snapshot(impl_->db, false);
            impl_->require_session_token(s.id);
            Statement entry(
                impl_->db,
                "SELECT result,status,name,args FROM tools WHERE session=? AND call_id=?");
            const auto record = Impl::read_journal_entry(entry, s.id, call.id);
            if (!record || (record->status != "running" && record->status != "complete"))
              throw Error("Session modified by another process: tool outcome is no longer pending");
            Impl::validate_journal_call(*record, call);
            already_complete = record->status == "complete";
            if (has_result && !already_complete) {
              const auto projection = s.checkpoint_projection;
              if (!projection || !projection->recovery ||
                  !projection->recovery->provisional.count(call.id) ||
                  projection->recovery->provisional.at(call.id).message_id !=
                      s.messages[position].message_id)
                throw Error("Existing tool result is not a provisional recovery outcome");
            }
            Impl::PrepareOptions options;
            options.kind = position < s.messages.size() || already_complete
                               ? Impl::WriteKind::revise
                               : Impl::WriteKind::append;
            options.first = position;
            options.tool = call.name;
            if (overlay)
              options.suffix = &result;
            prepared = impl_->prepare(overlay ? s : working, {position}, options);
            if (already_complete) {
              Statement row(impl_->db,
                            "SELECT payload FROM session_messages WHERE session=? AND position=?");
              const auto payload = impl_->stored_payload(s.id, position, row);
              if (!payload || *payload != *prepared.rows.at(position).payload ||
                  record->result != *payload)
                throw Error("Session modified by another process: completed tool result differs");
            }
            snapshot.commit();
          }
          retirement.prepare(result);
          if (overlay)
            s.messages.reserve(s.messages.size() + 1);
          else
            for (const auto &message : s.messages)
              retirement.prepare(message);
          return prepared;
        },
        [&](Impl::PreparedTranscript &prepared) {
          if (!already_complete)
            impl_->apply_tool_result(s.id, call, *prepared.rows.at(position).payload);
          update_metadata(overlay ? s : working);
          impl_->apply_prepared_transcript(overlay ? s : working, prepared);
        },
        [&](Impl::PreparedTranscript &prepared) noexcept {
          if (overlay) {
            static_assert(std::is_nothrow_move_constructible<Message>::value,
                          "Tool publication must not throw");
            impl_->publish_transcript(s, prepared);
            s.messages.push_back(std::move(prepared.rows.find(position)->second.message));
          } else {
            impl_->publish_transcript(working, prepared);
            s.messages.swap(working.messages);
            s.checkpoint_projection.swap(working.checkpoint_projection);
            for (auto &message : working.messages)
              retirement.retire(message);
          }
          retirement.retire(result);
        },
        "Tool result preparation changed twice; no changes applied; retry publication");
  } catch (...) {
    const auto failure = std::current_exception();
    try {
      const auto path =
          repair_directory(impl_->home) / (s.id + "-tool-result-" + unique_id() + ".json");
      const auto bytes = safe_json(Json({{"kind", "uncommitted_tool_result"},
                                         {"session", s.id},
                                         {"call", call},
                                         {"message", result},
                                         {"dispatch_allowed", false}}))
                             .dump();
      bool confirmed = false;
      auto notice = std::make_unique<FileNotice>(path.string(), FileNoticeKind::tool_result);
      try {
        impl_->files->atomic_write(path, bytes, true);
        confirmed = true;
      } catch (...) {
        confirmed = impl_->publication_confirmed(path, bytes);
      }
      notice->confirmed = confirmed;
      impl_->report_file(std::move(notice));
    } catch (...) {
      // Preserve the original failure, including allocation and non-standard exceptions.
      std::rethrow_exception(failure);
    }
    std::rethrow_exception(failure);
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
  impl_->files->atomic_write(path,
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
Json StateStore::repair_session_token(const std::string &selector, bool last) {
  impl_->require_owned_write();
  const auto id = select_session(impl_->db, selector, last);
  auto owner = acquire(id);
  std::vector<Impl::Backup> backups;
  backups.reserve(2);
  try {
    for (int attempt = 0; attempt < 2; ++attempt) {
      Impl::SessionVersion version;
      Json backup;
      {
        Transaction snapshot(impl_->db, false);
        version = impl_->session_version(id, true);
        if (version.present && valid_revision_token(version.token)) {
          Json report = {{"session", id},
                         {"changed", false},
                         {"session_token_regenerated", false},
                         {"provider_cache_reset", false}};
          if (!backups.empty()) {
            report["backups"] = Json::array();
            for (const auto &record : backups)
              report["backups"].push_back(record->path);
          }
          return report;
        }
        backup = impl_->raw_backup(id);
        snapshot.commit();
      }
      impl_->write_backup(backups, id, backup);
      backup = nullptr;
      Json report = {{"session", id},
                     {"changed", true},
                     {"session_token_regenerated", true},
                     {"provider_cache_reset", false},
                     {"backup", backups.back()->path}};
      if (backups.size() > 1) {
        report["backups"] = Json::array();
        for (const auto &record : backups)
          report["backups"].push_back(record->path);
      }
      {
        Transaction publication(impl_->db);
        if (!impl_->session_current(id, version, true))
          continue;
        Statement marker(impl_->db, "INSERT INTO session_revisions(session,revision) "
                                    "VALUES(?,lower(hex(randomblob(16)))) ON CONFLICT(session) "
                                    "DO UPDATE SET revision=excluded.revision");
        marker.text(1, id);
        marker.run();
        publication.commit();
      }
      return report;
    }
    throw Error(
        "Session token repair preparation changed twice; no repair changes applied; retry repair");
  } catch (...) {
    impl_->report_backups(backups);
    throw;
  }
}
Json StateStore::repair(const std::string &selector, bool last, bool provider_cache, bool recovery,
                        Tools &tools) {
  if (!sqlite3_get_autocommit(impl_->db))
    throw Error("Repair requires its own transaction");
  const auto id = select_session(impl_->db, selector, last);
  auto owner = acquire(id);
  const bool mutate = provider_cache || recovery;
  std::vector<Impl::Backup> backups;
  backups.reserve(2);
  // One retry only. Preparation never holds the database's writer lock.
  for (int attempt = 0; attempt < 2; ++attempt) {
    std::vector<RecoveryCandidate> candidates;
    const auto cleanup = [&] {
      impl_->clear_provider_cache();
      cleanup_recovery_candidates(*this, tools, candidates);
    };
    try {
      Impl::SessionVersion version;
      Session session;
      Json journal, backup,
          report = {{"session", id}, {"changed", false}, {"issues", Json::array()}};
      {
        Transaction snapshot(impl_->db, false);
        version = impl_->session_version(id, true);
        if (!valid_revision_token(version.token))
          report["issues"].push_back(
              "Invalid session revision token; explicit session repair required");
        ReadOptions read_options;
        read_options.provider = false;
        read_options.validate_revision = false;
        session = read_session(id, read_options);
        try {
          read_options.provider = true;
          (void)read_session(id, read_options);
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
        if (mutate)
          backup = impl_->raw_backup(id);
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
      impl_->write_backup(backups, id, backup);
      backup = nullptr;
      report["backup"] = backups.back()->path;
      if (backups.size() > 1) {
        report["backups"] = Json::array();
        for (const auto &backup : backups)
          report["backups"].push_back(backup->path);
      }
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
        stale = !impl_->session_current(id, version, true);
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
          if (sqlite3_changes(impl_->db) == 0) {
            Statement rotate(impl_->db, "INSERT INTO provider_revisions(session,revision) "
                                        "VALUES(?,lower(hex(randomblob(16)))) ON CONFLICT(session) "
                                        "DO UPDATE SET revision=excluded.revision");
            rotate.text(1, id);
            rotate.run();
          }
          if (recovery) {
            update_metadata(session);
            reset_message_rows(impl_->db, id);
            insert_message_rows(impl_->db, id, payloads);
            for (const auto &payload : payloads)
              impl_->serialized += payload.size();
            insert_reference_rows(impl_->db, id, retained);
          }
          Statement marker(impl_->db,
                           "INSERT INTO session_revisions(session,revision) "
                           "VALUES(?,lower(hex(randomblob(16)))) "
                           "ON CONFLICT(session) DO UPDATE SET revision=excluded.revision");
          marker.text(1, id);
          marker.run();
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
      impl_->report_backups(backups);
      std::rethrow_exception(failure);
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
  JournalTransaction transaction(impl_->db);
  impl_->require_session_token(id);
  Statement q(impl_->db, "UPDATE sessions SET status='running' WHERE id=?");
  q.text(1, id);
  q.run();
  transaction.commit();
}
void StateStore::finish_turn(const std::string &id, const std::string &status) {
  auto owner = acquire(id);
  JournalTransaction transaction(impl_->db);
  impl_->require_session_token(id);
  Statement q(impl_->db, "UPDATE sessions SET status=? WHERE id=?");
  q.text(1, status);
  q.text(2, id);
  q.run();
  transaction.commit();
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
  JournalTransaction transaction(impl_->db);
  impl_->require_session_token(id);
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
  transaction.commit();
}
void StateStore::tool_finished(const std::string &id, const ToolCall &t, const Json &result) {
  auto owner = acquire(id);
  std::vector<Message> messages{result.get<Message>()};
  impl_->portable(messages, id, t.name);
  const auto payload = safe_json(Json(messages.front())).dump();
  auto paths = message_references(messages);
  JournalTransaction transaction(impl_->db);
  impl_->require_session_token(id);
  impl_->apply_tool_result(id, t, payload);
  impl_->remember_paths(id, std::move(paths));
  transaction.commit();
}
} // namespace codet
