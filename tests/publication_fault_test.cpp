// A separate executable confines allocator/SQLite interception to deterministic fault tests.
#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/prompt.hpp"
#include "temp.hpp"
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <new>
#include <sqlite3.h>
using namespace codet;
namespace fault {
bool after_commit = false, after_token = false, fired = false;
bool provider_read_error = false, rollback_read = false, token_error = false;
bool infrastructure_error = false;
bool read_failed = false;
sqlite3 *observed_connection = nullptr;
size_t steps_after_failure = 0, checkpoint_reads = 0;
int countdown = -1, skip = 0;
std::function<void()> provider_read_done, journal_read_done, before_write;
bool measure_allocations = false;
size_t largest_allocation = 0, journal_reads = 0, transcript_rows_read = 0;
const char *failed_query = nullptr;
int failed_status = SQLITE_IOERR;
bool failed_allocation = false, failed_rollback = false;
void arm() noexcept {
  countdown = skip;
}
void disable() noexcept {
  after_commit = after_token = false;
  countdown = -1;
  provider_read_error = rollback_read = token_error = read_failed = false;
  infrastructure_error = false;
  provider_read_done = {};
  journal_read_done = {};
  before_write = {};
  measure_allocations = false;
  failed_query = nullptr;
  failed_allocation = failed_rollback = false;
  failed_status = SQLITE_IOERR;
  skip = 0;
}
} // namespace fault
void *operator new(std::size_t n) {
  if (fault::measure_allocations)
    fault::largest_allocation = std::max(fault::largest_allocation, n);
  if (fault::countdown == 0) {
    fault::countdown = -1;
    fault::fired = true;
    throw std::bad_alloc();
  }
  if (fault::countdown > 0)
    --fault::countdown;
  if (void *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](std::size_t n) {
  return ::operator new(n);
}
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
  try {
    return ::operator new(n);
  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}
void *operator new[](std::size_t n, const std::nothrow_t &tag) noexcept {
  return ::operator new(n, tag);
}
void operator delete(void *p) noexcept {
  std::free(p);
}
void operator delete[](void *p) noexcept {
  ::operator delete(p);
}
void operator delete(void *p, std::size_t) noexcept {
  ::operator delete(p);
}
void operator delete[](void *p, std::size_t) noexcept {
  ::operator delete(p);
}
void operator delete(void *p, const std::nothrow_t &) noexcept {
  ::operator delete(p);
}
void operator delete[](void *p, const std::nothrow_t &) noexcept {
  ::operator delete(p);
}
extern "C" int sqlite3_exec(sqlite3 *db, const char *sql,
                            int (*callback)(void *, int, char **, char **), void *context,
                            char **error) {
  using Execute =
      int (*)(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **);
  static const auto execute = reinterpret_cast<Execute>(dlsym(RTLD_NEXT, "sqlite3_exec"));
  if (std::strcmp(sql, "BEGIN IMMEDIATE") == 0 && fault::before_write) {
    auto callback = std::move(fault::before_write);
    fault::before_write = {};
    callback();
  }
  const bool write_commit =
      std::strcmp(sql, "COMMIT") == 0 && sqlite3_txn_state(db, "main") == SQLITE_TXN_WRITE;
  const int result = execute(db, sql, callback, context, error);
  if (result == SQLITE_OK && fault::after_commit && write_commit) {
    fault::after_commit = false;
    fault::arm();
  }
  return result;
}
extern "C" int sqlite3_step(sqlite3_stmt *statement) {
  fault::observed_connection = sqlite3_db_handle(statement);
  if (std::strcmp(
          sqlite3_sql(statement),
          "SELECT position,payload FROM session_messages WHERE session=? ORDER BY position") == 0)
    ++fault::transcript_rows_read;
  if (std::strcmp(sqlite3_sql(statement),
                  "SELECT payload FROM session_messages WHERE session=? AND position=?") == 0)
    ++fault::checkpoint_reads;
  using Step = int (*)(sqlite3_stmt *);
  static const auto step = reinterpret_cast<Step>(dlsym(RTLD_NEXT, "sqlite3_step"));
  if (fault::read_failed)
    ++fault::steps_after_failure;
  if (fault::failed_query && std::strcmp(sqlite3_sql(statement), fault::failed_query) == 0) {
    fault::failed_query = nullptr;
    if (fault::failed_allocation)
      throw std::bad_alloc();
    if (fault::failed_rollback)
      sqlite3_exec(sqlite3_db_handle(statement), "ROLLBACK", nullptr, nullptr, nullptr);
    fault::read_failed = true;
    return fault::failed_status;
  }
  if (fault::provider_read_error &&
      std::strcmp(
          sqlite3_sql(statement),
          "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position") == 0) {
    fault::provider_read_error = false;
    if (fault::rollback_read)
      sqlite3_exec(sqlite3_db_handle(statement), "ROLLBACK", nullptr, nullptr, nullptr);
    fault::read_failed = true;
    return SQLITE_IOERR;
  }
  if (fault::token_error &&
      std::strcmp(sqlite3_sql(statement),
                  "SELECT revision FROM provider_revisions WHERE session=?") == 0) {
    fault::token_error = false;
    return SQLITE_IOERR;
  }
  if (fault::infrastructure_error &&
      std::strcmp(sqlite3_sql(statement), "SELECT sql FROM sqlite_master WHERE name=?") == 0) {
    fault::infrastructure_error = false;
    return SQLITE_IOERR;
  }
  const int result = step(statement);
  if (result == SQLITE_DONE && fault::provider_read_done &&
      std::strcmp(
          sqlite3_sql(statement),
          "SELECT position,payload FROM provider_parts WHERE session=? ORDER BY position") == 0) {
    auto callback = std::move(fault::provider_read_done);
    fault::provider_read_done = {};
    callback();
  }
  if (result == SQLITE_DONE &&
      std::strcmp(sqlite3_sql(statement),
                  "SELECT result,status,name,args FROM tools WHERE session=? AND call_id=?") == 0) {
    ++fault::journal_reads;
    if (fault::journal_read_done) {
      auto callback = std::move(fault::journal_read_done);
      fault::journal_read_done = {};
      callback();
    }
  }
  if (result == SQLITE_ROW && fault::after_token &&
      std::strcmp(sqlite3_sql(statement),
                  "SELECT revision FROM provider_revisions WHERE session=?") == 0) {
    fault::after_token = false;
    fault::arm();
  }
  return result;
}
namespace {
TEST(FaultAllocator, NothrowAllocationsUseTheSameReplacementAndFailureInjection) {
  fault::disable();
  void *scalar = ::operator new(16, std::nothrow);
  ASSERT_NE(scalar, nullptr);
  ::operator delete(scalar);
  void *array = ::operator new[](16, std::nothrow);
  ASSERT_NE(array, nullptr);
  ::operator delete[](array);
  fault::fired = false;
  fault::countdown = 0;
  void *failed = ::operator new(16, std::nothrow);
  const bool injected = fault::fired;
  fault::disable();
  EXPECT_EQ(failed, nullptr);
  EXPECT_TRUE(injected);
}
Json snapshot(const std::string &text) {
  return {{"provider", "anthropic"},
          {"messages",
           Json::array({{{"source", "source"},
                         {"wire", Json::array({{{"role", "user"}, {"content", text}}})}}})}};
}
struct NoHttp : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected network request");
  }
};
struct Received : Provider {
  Json state;
  void restore_conversation(const Json &value) override {
    state = value;
  }
  Json conversation_state() const override {
    return state;
  }
  ProviderResult complete(const Config &, const std::string &, const std::vector<Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink &) override {
    state = snapshot("new");
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = "received";
    fault::after_commit = true;
    return result;
  }
};
} // namespace
struct PublicationFault : testing::Test {
  void SetUp() override {
    fault::disable();
    fault::fired = false;
    fault::steps_after_failure = fault::checkpoint_reads = 0;
    fault::observed_connection = nullptr;
    fault::journal_reads = fault::largest_allocation = fault::transcript_rows_read = 0;
  }
  void TearDown() override {
    fault::disable();
  }
};
TEST_F(PublicationFault, OwningWritesRemainSuccessfulAndTheNextSaveWorksWithoutReload) {
  for (const auto *mode : {"provider", "append", "replace"})
    for (const int skip : {0, 2, 6, 12, 20}) {
      SCOPED_TRACE(std::string(mode) + ":" + std::to_string(skip));
      TempDirectory temp;
      StateStore store(temp.root / "state");
      auto s = store.create();
      s.provider_state = snapshot(std::string(4096, 'a'));
      store.save(s);
      auto lease = store.acquire(s.id);
      s.provider_state = snapshot(std::string(4096, 'b'));
      Message message;
      message.role = "assistant";
      message.text = "received";
      fault::fired = false;
      fault::skip = skip;
      bool failed = false;
      try {
        ResponseAppend guard(s);
        // ResponseAppend owns the old snapshot; install the newly received one inside its scope.
        s.provider_state = snapshot(std::string(4096, 'b'));
        s.messages.push_back(message);
        fault::after_commit = true;
        if (std::strcmp(mode, "provider") == 0)
          store.save_provider_state(s);
        else if (std::strcmp(mode, "append") == 0)
          store.append_messages(s, 0, true);
        else
          store.replace_history(s);
        fault::disable();
        guard.commit();
      } catch (...) {
        fault::disable();
        failed = true;
      }
      EXPECT_FALSE(failed);
      EXPECT_TRUE(fault::fired);
      EXPECT_EQ(s.messages.size(), 1u);
      EXPECT_EQ(s.provider_state, snapshot(std::string(4096, 'b')));
      EXPECT_EQ(store.provider_cache_bytes(), 0u);
      EXPECT_EQ(store.provider_revision(s.id), 0u);
      // Verify the next owning save before a reload can hide divergence.
      EXPECT_NO_THROW(store.save(s));
      EXPECT_EQ(store.load_id(s.id, false).provider_state, s.provider_state);
    }
}
TEST_F(PublicationFault, LoadCacheFailureDoesNotFailAValidLoad) {
  TempDirectory temp;
  StateStore store(temp.root / "state");
  auto s = store.create();
  s.provider_state = snapshot(std::string(4096, 'a'));
  store.save(s);
  // The token's string allocation precedes optional cache publication.
  fault::skip = 1;
  fault::fired = false;
  fault::after_token = true;
  bool failed = false;
  Session loaded;
  try {
    loaded = store.load_id(s.id, false);
  } catch (...) {
    failed = true;
  }
  fault::disable();
  EXPECT_FALSE(failed);
  EXPECT_TRUE(fault::fired);
  EXPECT_EQ(loaded.provider_state, s.provider_state);
  EXPECT_EQ(store.provider_cache_bytes(), 0u);
  EXPECT_NO_THROW(store.save(loaded));
}
TEST_F(PublicationFault, AgentKeepsCommittedResponseAndProviderAdapterWithoutAReceipt) {
  TempDirectory temp;
  Config config;
  config.home = temp.root / "home";
  config.cwd = temp.root / "work";
  config.state_root = temp.root / "state";
  config.roots = {config.cwd};
  config.approval = "never";
  fs::create_directories(config.cwd);
  fs::create_directories(config.home);
  NativeFileSystem files;
  NativeProcesses processes;
  NoHttp http;
  SystemClock clock;
  Cancellation cancel;
  StateStore store(config.state_root);
  auto s = store.create();
  s.provider_state = snapshot("old");
  store.save(s);
  Received provider;
  Tools tools(config, files, processes, http, clock);
  Agent agent(config, provider, tools, store, {});
  fault::skip = 0;
  fault::fired = false;
  bool failed = false;
  try {
    agent.run(s, "continue", cancel);
  } catch (...) {
    failed = true;
  }
  fault::disable();
  EXPECT_FALSE(failed);
  EXPECT_TRUE(fault::fired);
  ASSERT_EQ(s.messages.size(), 2u);
  EXPECT_EQ(s.messages.back().text, "received");
  EXPECT_EQ(s.provider_state, provider.state);
  EXPECT_FALSE(fs::exists(config.state_root / "repairs"));
  EXPECT_NO_THROW(store.save(s));
  EXPECT_NO_THROW(store.reconcile(s));
}

TEST_F(PublicationFault, ActualLoadKeepsOldProviderRowsUnderTheOldRevisionToken) {
  TempDirectory temp;
  StateStore store(temp.root / "state");
  auto s = store.create();
  s.provider_state = snapshot("old");
  store.save(s);
  sqlite3 *writer = nullptr;
  ASSERT_EQ(sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &writer), SQLITE_OK);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(writer, sqlite3_close);
  struct ClearReadCallback {
    ~ClearReadCallback() {
      fault::provider_read_done = {};
    }
  } clear_callback;
  fault::provider_read_done = [&] {
    ASSERT_EQ(
        sqlite3_exec(
            writer,
            "UPDATE provider_parts SET payload=replace(payload,'old','external') WHERE position=1",
            nullptr, nullptr, nullptr),
        SQLITE_OK);
  };
  auto loaded = store.load_id(s.id, false);
  EXPECT_FALSE(fault::provider_read_done);
  EXPECT_EQ(loaded.provider_state, snapshot("old"));
  EXPECT_EQ(store.provider_revision(s.id), 0u);
  const auto reads = store.provider_rows_read();
  EXPECT_NO_THROW(store.save(loaded));
  EXPECT_GT(store.provider_rows_read(), reads);
  EXPECT_EQ(store.load_id(s.id, false).provider_state, loaded.provider_state);
}

TEST_F(PublicationFault, TokenAllocationAndQueryFailuresLeaveValidLoadsSuccessful) {
  TempDirectory temp;
  StateStore store(temp.root / "state");
  auto s = store.create();
  s.provider_state = snapshot(std::string(4096, 'a'));
  store.save(s);
  for (const int mode : {0, 1, 2}) {
    SCOPED_TRACE(mode);
    if (mode == 2) {
      sqlite3 *raw = nullptr;
      ASSERT_EQ(sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &raw), SQLITE_OK);
      std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(raw, sqlite3_close);
      ASSERT_EQ(sqlite3_exec(raw, "CREATE TABLE unrelated(id INTEGER)", nullptr, nullptr, nullptr),
                SQLITE_OK);
    }
    fault::skip = 0;
    fault::fired = false;
    fault::after_token = mode == 0;
    fault::token_error = mode == 1;
    fault::infrastructure_error = mode == 2;
    Session loaded;
    bool failed = false;
    try {
      loaded = store.load_id(s.id, false);
    } catch (...) {
      failed = true;
    }
    fault::disable();
    if (mode == 2) {
      EXPECT_TRUE(failed);
      EXPECT_NO_THROW(loaded = store.load_id(s.id, false));
      EXPECT_EQ(loaded.provider_state, s.provider_state);
      continue;
    }
    EXPECT_FALSE(failed);
    if (mode == 0) {
      EXPECT_TRUE(fault::fired);
    }
    EXPECT_EQ(loaded.provider_state, s.provider_state);
    EXPECT_EQ(store.provider_cache_bytes(), 0u);
    EXPECT_EQ(store.provider_revision(s.id), 0u);
    EXPECT_NO_THROW(store.save(loaded));
  }
}
TEST_F(PublicationFault, RepairStopsImmediatelyOnOperationalReadFailureWithOrWithoutRollback) {
  TempDirectory temp;
  Config config;
  config.home = temp.root / "home";
  config.state_root = temp.root / "state";
  config.cwd = temp.root / "work";
  fs::create_directories(config.home);
  fs::create_directories(config.cwd);
  NativeFileSystem files;
  NativeProcesses processes;
  NoHttp http;
  SystemClock clock;
  StateStore store(config.state_root);
  auto s = store.create();
  s.provider_state = snapshot("original");
  store.save(s);
  Tools tools(config, files, processes, http, clock);
  for (const bool rollback : {false, true}) {
    SCOPED_TRACE(rollback);
    fault::provider_read_error = true;
    fault::rollback_read = rollback;
    fault::steps_after_failure = fault::checkpoint_reads = 0;
    std::string failure;
    try {
      (void)store.repair(s.id, false, true, false, tools);
    } catch (const Error &e) {
      failure = e.what();
    }
    const auto steps = fault::steps_after_failure;
    fault::disable();
    EXPECT_NE(failure.find("Cannot load provider conversation"), std::string::npos) << failure;
    EXPECT_EQ(steps, 0u);
    EXPECT_FALSE(fs::exists(config.state_root / "repairs"));
    EXPECT_EQ(store.load_id(s.id, false).provider_state, s.provider_state);
    EXPECT_NO_THROW(store.save(s));
  }
}
TEST_F(PublicationFault, InvalidRevisionTokensRemainMandatoryLoadErrors) {
  TempDirectory temp;
  StateStore store(temp.root / "state");
  auto s = store.create();
  s.provider_state = snapshot("original");
  store.save(s);
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &raw), SQLITE_OK);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(raw, sqlite3_close);
  ASSERT_EQ(sqlite3_exec(raw, "UPDATE provider_revisions SET revision='invalid'", nullptr, nullptr,
                         nullptr),
            SQLITE_OK);
  EXPECT_THROW(store.load_id(s.id, false), Error);
  EXPECT_EQ(store.provider_cache_bytes(), 0u);
}

TEST_F(PublicationFault, SessionLoadAndRepairPreserveOperationalAndAllocationFailures) {
  TempDirectory temp;
  Config config;
  config.home = temp.root / "home";
  config.state_root = temp.root / "state";
  config.cwd = temp.root / "work";
  fs::create_directories(config.home);
  fs::create_directories(config.cwd);
  NativeFileSystem files;
  NativeProcesses processes;
  NoHttp http;
  SystemClock clock;
  StateStore store(config.state_root);
  auto s = store.create();
  s.messages = {Message{"user", "saved"}};
  store.save(s);
  Tools tools(config, files, processes, http, clock);
  const auto arm = [](const char *query, int status, bool rollback = false) {
    fault::failed_query = query;
    fault::failed_status = status;
    fault::failed_rollback = rollback;
    fault::steps_after_failure = fault::checkpoint_reads = 0;
  };
  for (const int status : {SQLITE_IOERR, SQLITE_BUSY, SQLITE_CORRUPT}) {
    SCOPED_TRACE(status);
    for (const bool rollback : {false, true}) {
      SCOPED_TRACE(rollback);
      arm("SELECT position,payload FROM session_messages WHERE session=? ORDER BY position", status,
          rollback);
      std::string failure;
      try {
        store.repair(s.id, false, true, false, tools);
      } catch (const Error &e) {
        failure = e.what();
      }
      const auto steps = fault::steps_after_failure;
      fault::disable();
      EXPECT_NE(failure.find("Cannot load session messages"), std::string::npos) << failure;
      EXPECT_NE(failure.find(std::to_string(status)), std::string::npos) << failure;
      EXPECT_EQ(steps, 0u);
      EXPECT_FALSE(fs::exists(config.state_root / "repairs"));
    }
    arm("SELECT id FROM sessions WHERE id=? OR name=? ORDER BY (id=?) DESC,updated DESC,rowid DESC "
        "LIMIT 1",
        status);
    std::string failure;
    try {
      store.load(s.id, false);
    } catch (const Error &e) {
      failure = e.what();
    }
    fault::disable();
    EXPECT_NE(failure.find("Cannot select session"), std::string::npos) << failure;
    EXPECT_EQ(failure.find("not found"), std::string::npos);
  }
  arm("SELECT position,payload FROM session_messages WHERE session=? ORDER BY position",
      SQLITE_NOMEM);
  fault::failed_allocation = true;
  EXPECT_THROW(store.load_id(s.id, false), std::bad_alloc);
  fault::disable();
  EXPECT_NO_THROW(store.save(s));
}
TEST_F(PublicationFault, MigrationInspectionReportsSqlFailuresWithoutClaimingInvalidSchema) {
  for (const int status : {SQLITE_IOERR, SQLITE_BUSY, SQLITE_CORRUPT}) {
    SCOPED_TRACE(status);
    TempDirectory temp;
    const auto root = temp.root / "state";
    {
      StateStore store(root);
      store.create();
    }
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open((root / "state.sqlite3").c_str(), &db), SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(db, sqlite3_close);
    ASSERT_EQ(sqlite3_exec(db, "PRAGMA user_version=6", nullptr, nullptr, nullptr), SQLITE_OK);
    fault::failed_query = "SELECT sql FROM sqlite_master WHERE name=?";
    fault::failed_status = status;
    std::string failure;
    try {
      StateStore migrating(root);
    } catch (const Error &e) {
      failure = e.what();
    }
    fault::disable();
    EXPECT_NE(failure.find("Cannot inspect provider revision infrastructure"), std::string::npos)
        << failure;
    EXPECT_EQ(failure.find("Invalid provider revision infrastructure"), std::string::npos);
    sqlite3_stmt *version = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &version, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(version), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(version, 0), 6);
    sqlite3_finalize(version);
    EXPECT_NO_THROW(StateStore retry(root));
  }
}

TEST_F(PublicationFault, OptionalReferenceKnowledgeCannotFailCommittedRows) {
  TempDirectory temp;
  StateStore store(temp.root / "state");
  auto s = store.create();
  Message first;
  first.result_file = "results/owner/first.json";
  fault::after_commit = true;
  fault::skip = 0;
  fault::fired = false;
  EXPECT_NO_THROW(store.remember_references(s.id, {first}));
  const bool fired = fault::fired;
  fault::disable();
  EXPECT_TRUE(fired);
  Message second;
  second.result_file = "results/owner/second.json";
  EXPECT_NO_THROW(store.remember_references(s.id, {first, second}));
  EXPECT_EQ(store.references(s.id).size(), 2u);
  EXPECT_NO_THROW(store.save(s));
}

TEST_F(PublicationFault, OwningCheckpointAcknowledgementDoesNotAllocateAfterCommit) {
  TempDirectory temp;
  NativeFileSystem files;
  const auto root = temp.root / "artifacts";
  const auto path = root / "results/owner/archive.json";
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "{}", true);
  StateStore store(files, temp.root / "state", root);
  auto s = store.create();
  Message cp;
  cp.role = "system";
  cp.text = "summary";
  cp.message_id = "checkpoint";
  cp.checkpoint_state = {{"recovery_record", path.string()}};
  s.messages = {cp};
  store.save(s);
  // Owning full replacement must keep its acknowledgement allocation-free too.
  fault::after_commit = true;
  fault::skip = 0;
  bool failed = false;
  try {
    store.replace_history(s);
  } catch (...) {
    failed = true;
  }
  // Optional provider-cache population may consume the failure, without failing the commit.
  fault::disable();
  EXPECT_FALSE(failed);
  s.messages.push_back(Message{"user", "next"});
  fault::after_commit = true;
  fault::fired = false;
  try {
    store.append_messages(s, 1);
  } catch (...) {
    failed = true;
  }
  const bool allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(failed);
  EXPECT_FALSE(allocated);
  EXPECT_NO_THROW(store.reconcile(s));
}

namespace {
struct ProjectionFixture {
  TempDirectory temp;
  NativeFileSystem files;
  fs::path root = temp.root / "artifacts", archive = root / "results/owner/archive.json";
  StateStore store{files, temp.root / "state", root};
  Session session;
  sqlite3 *db = nullptr;
  explicit ProjectionFixture(bool blob = false) {
    fs::create_directories(archive.parent_path());
    files.atomic_write(archive, "{}", true);
    session = store.create();
    Message cp;
    cp.role = "system";
    cp.message_id = "checkpoint";
    cp.text = "summary";
    cp.checkpoint_state = {{"recovery_record", archive.string()}};
    session.messages = {cp};
    store.save(session);
    db = fault::observed_connection;
    const auto payload = Json(cp).dump(2);
    const auto query = blob ? "UPDATE sessions SET message_format=0,messages=?"
                            : "UPDATE session_messages SET payload=?";
    sqlite3_stmt *q = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &q, nullptr) != SQLITE_OK)
      throw Error("Cannot prepare fixture");
    const auto bytes = blob ? Json(std::vector<Message>{cp}).dump(2) : payload;
    sqlite3_bind_text(q, 1, bytes.c_str(), -1, SQLITE_TRANSIENT);
    const auto status = sqlite3_step(q);
    sqlite3_finalize(q);
    if (status != SQLITE_DONE)
      throw Error("Cannot store fixture");
    if (blob)
      exec("DELETE FROM session_messages");
    session = store.load_id(session.id, false);
  }
  void exec(const char *query) const {
    if (sqlite3_exec(db, query, nullptr, nullptr, nullptr) != SQLITE_OK)
      throw Error(sqlite3_errmsg(db));
  }
  std::string value(const char *query) const {
    sqlite3_stmt *q = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &q, nullptr) != SQLITE_OK)
      throw Error("Cannot inspect fixture");
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(q, sqlite3_finalize);
    if (sqlite3_step(q) != SQLITE_ROW)
      throw Error("Missing fixture row");
    const auto *bytes = sqlite3_column_text(q, 0);
    return bytes ? reinterpret_cast<const char *>(bytes) : "";
  }
};
} // namespace
TEST_F(PublicationFault, PreparationAllocationFailuresLeaveSnapshotAndNextWriteUsable) {
  for (const auto *mode : {"append", "revise", "replace"})
    for (int skip : {0, 2, 5, 12, 30}) {
      SCOPED_TRACE(std::string(mode) + ":" + std::to_string(skip));
      ProjectionFixture f;
      auto &s = f.session;
      const auto provenance = s.checkpoint_projection;
      const auto before = f.value("SELECT payload FROM session_messages WHERE position=0");
      if (std::strcmp(mode, "append") == 0)
        s.messages.push_back(Message{"user", "new suffix"});
      else
        s.messages[0].text = "revised";
      const auto incoming = Json(s.messages);
      const auto write = [&] {
        if (std::strcmp(mode, "append") == 0)
          f.store.append_messages(s, 1);
        else if (std::strcmp(mode, "revise") == 0)
          f.store.revise_messages(s, {0});
        else
          f.store.replace_history(s);
      };
      fault::countdown = skip;
      bool failed = false;
      try {
        write();
      } catch (const std::bad_alloc &) {
        failed = true;
      }
      fault::disable();
      ASSERT_TRUE(failed);
      EXPECT_EQ(s.checkpoint_projection, provenance);
      EXPECT_EQ(Json(s.messages), incoming);
      EXPECT_EQ(f.value("SELECT payload FROM session_messages WHERE position=0"), before);
      EXPECT_NO_THROW(write());
      EXPECT_NO_THROW(f.store.reconcile(s));
    }
}
TEST_F(PublicationFault, BlobConversionDeferredFailureRetainsItsOriginalStorage) {
  ProjectionFixture f(true);
  const auto provenance = f.session.checkpoint_projection;
  f.exec("CREATE TABLE parent(id PRIMARY KEY);CREATE TABLE poison(id REFERENCES parent(id) "
         "DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER fail_blob AFTER INSERT ON session_messages "
         "BEGIN INSERT INTO poison VALUES(1); END");
  f.session.messages.push_back(Message{"user", "next"});
  EXPECT_THROW(f.store.append_messages(f.session, 1), Error);
  EXPECT_EQ(f.value("SELECT message_format FROM sessions"), "0");
  EXPECT_EQ(f.session.checkpoint_projection, provenance);
  f.exec("DROP TRIGGER fail_blob");
  EXPECT_NO_THROW(f.store.append_messages(f.session, 1));
}
TEST_F(PublicationFault, AcknowledgedMappingsNeedNoRowReadsOrPayloadCopiesDuringOrdinaryAppends) {
  ProjectionFixture f;
  f.store.revise_messages(f.session, {});
  const auto compact = f.session.checkpoint_projection;
  fault::checkpoint_reads = 0;
  const auto bytes = f.store.serialized_bytes();
  for (int i = 0; i < 10; ++i) {
    f.session.messages.push_back(Message{"user", "next"});
    f.store.append_messages(f.session, f.session.messages.size() - 1);
    EXPECT_EQ(f.session.checkpoint_projection, compact);
  }
  EXPECT_EQ(fault::checkpoint_reads, 0u);
  EXPECT_LT(f.store.serialized_bytes() - bytes, 5000u);
}
TEST_F(PublicationFault, ReconciliationRetiresCopiedJsonWithoutAllocatingAfterCommit) {
  ProjectionFixture f;
  fault::fired = false;
  fault::after_commit = true;
  bool failed = false;
  try {
    f.store.reconcile(f.session);
  } catch (...) {
    failed = true;
  }
  const auto allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(failed);
  EXPECT_FALSE(allocated);
  EXPECT_NO_THROW(f.store.reconcile(f.session));
}

TEST_F(PublicationFault, PublicTranscriptMutationsRejectUnownedTransactionsWithoutPublication) {
  for (bool blob : {false, true})
    for (bool rollback : {false, true})
      for (const auto *mode :
           {"save", "const-save", "append", "const-append", "revise", "const-revise", "replace",
            "const-replace", "complete", "reconcile"}) {
        SCOPED_TRACE(std::string(mode) + (blob ? ":blob" : ":rows") +
                     (rollback ? ":rollback" : ":commit"));
        ProjectionFixture f(blob);
        auto &s = f.session;
        if (!blob) {
          s.messages.push_back(Message{"user", "ordinary original"});
          f.store.append_messages(s, 1);
        }
        const auto first = s.messages.size();
        const auto clean = s;
        if (std::strstr(mode, "append"))
          s.messages.push_back(Message{"user", "new suffix"});
        else if (std::strcmp(mode, "complete") && std::strcmp(mode, "reconcile")) {
          s.messages[0].text = "uncommitted checkpoint change";
          if (!blob)
            s.messages[1].text = "uncommitted ordinary change";
        }
        const auto incoming = Json(s.messages);
        const auto provenance = s.checkpoint_projection;
        const auto stored = f.value(blob ? "SELECT messages FROM sessions"
                                         : "SELECT payload FROM session_messages WHERE position=0");
        const auto bytes = f.store.serialized_bytes();
        const Session &immutable = s;
        f.exec("BEGIN IMMEDIATE");
        const auto changes = sqlite3_total_changes(f.db);
        std::string failure;
        try {
          if (std::strcmp(mode, "save") == 0)
            f.store.save(s);
          else if (std::strcmp(mode, "const-save") == 0)
            f.store.save(immutable);
          else if (std::strcmp(mode, "append") == 0)
            f.store.append_messages(s, first);
          else if (std::strcmp(mode, "const-append") == 0)
            f.store.append_messages(immutable, first);
          else if (std::strcmp(mode, "revise") == 0)
            f.store.revise_messages(s, blob ? std::vector<size_t>{0} : std::vector<size_t>{0, 1});
          else if (std::strcmp(mode, "const-revise") == 0)
            f.store.revise_messages(immutable,
                                    blob ? std::vector<size_t>{0} : std::vector<size_t>{0, 1});
          else if (std::strcmp(mode, "replace") == 0)
            f.store.replace_history(s);
          else if (std::strcmp(mode, "const-replace") == 0)
            f.store.replace_history(immutable);
          else if (std::strcmp(mode, "complete") == 0)
            f.store.complete_tool(s, ToolCall{"call", "exec_command", Json::object()},
                                  s.messages[0]);
          else
            f.store.reconcile(s);
        } catch (const Error &error) {
          failure = error.what();
        }
        EXPECT_NE(failure.find("require their own transaction"), std::string::npos) << failure;
        EXPECT_EQ(sqlite3_get_autocommit(f.db), 0);
        EXPECT_EQ(sqlite3_total_changes(f.db), changes);
        EXPECT_EQ(Json(s.messages), incoming);
        EXPECT_EQ(s.checkpoint_projection, provenance);
        EXPECT_EQ(f.store.serialized_bytes(), bytes);
        f.exec(rollback ? "ROLLBACK" : "COMMIT");
        EXPECT_EQ(f.value(blob ? "SELECT messages FROM sessions"
                               : "SELECT payload FROM session_messages WHERE position=0"),
                  stored);
        s = clean; // Caller edits are not undone by rejection; retry from the retained clean
                   // snapshot.
        s.messages.push_back(Message{"user", "ordinary next write"});
        ASSERT_NO_THROW(f.store.append_messages(s, first));
        EXPECT_EQ(
            Json::parse(f.value("SELECT payload FROM session_messages WHERE position=0"))["text"],
            "summary");
      }
}
TEST_F(PublicationFault, ConstAdaptersAndReconciliationBorrowLargeProviderState) {
  ProjectionFixture f;
  auto &s = f.session;
  s.provider_state = snapshot(std::string(9 * 1024 * 1024, 'x'));
  f.exec("INSERT INTO tools(session,call_id,name,args,result,status) SELECT "
         "id,'historical','exec_command','{}',printf('%.*c',9437184,'x'),'complete' FROM sessions");
  for (const auto *mode : {"append", "revise", "read-reconcile", "write-reconcile"}) {
    SCOPED_TRACE(mode);
    if (std::strcmp(mode, "append") == 0)
      s.messages.push_back(Message{"user", "tiny suffix"});
    const Session &immutable = s;
    fault::largest_allocation = 0;
    fault::measure_allocations = true;
    if (std::strcmp(mode, "append") == 0)
      f.store.append_messages(immutable, 1);
    else if (std::strcmp(mode, "revise") == 0)
      f.store.revise_messages(immutable, {});
    else
      f.store.reconcile(s, std::strcmp(mode, "write-reconcile") == 0);
    const auto largest = fault::largest_allocation;
    fault::measure_allocations = false;
    EXPECT_LT(largest, 128u * 1024u);
    EXPECT_EQ(
        s.provider_state["messages"][0]["wire"][0]["content"].get_ref<const std::string &>().size(),
        9u * 1024u * 1024u);
  }
}
TEST_F(PublicationFault, ConstPreparedJsonCleanupDoesNotAllocateAfterCommit) {
  for (bool immutable : {false, true}) {
    ProjectionFixture f;
    f.session.messages[0].replay =
        Json::array({{{"deep", Json::array({1, Json::object(), Json::array({2, 3})})}}});
    const Session &view = f.session;
    fault::fired = false;
    fault::after_commit = true;
    bool failed = false;
    try {
      if (immutable)
        f.store.revise_messages(view, {0});
      else
        f.store.revise_messages(f.session, {0});
    } catch (...) {
      failed = true;
    }
    const auto allocated = fault::fired;
    fault::disable();
    EXPECT_FALSE(failed);
    EXPECT_FALSE(allocated);
  }
}
TEST_F(PublicationFault, ReconciliationDetectsSameConnectionDmlAndDdlAndRetriesOnlyOnce) {
  for (bool ddl : {false, true})
    for (int invalidations : {1, 2}) {
      SCOPED_TRACE(std::to_string(invalidations) + (ddl ? ":ddl" : ":dml"));
      ProjectionFixture f;
      Message assistant;
      assistant.role = "assistant";
      assistant.calls = {{"unrecorded", "exec_command", Json::object()}};
      f.session.messages.push_back(assistant);
      f.store.append_messages(f.session, 1);
      const auto stored = f.value("SELECT payload FROM session_messages WHERE position=0");
      const auto before = Json(f.session.messages);
      const auto projection = f.session.checkpoint_projection;
      int fired = 0;
      std::function<void()> callback;
      callback = [&] {
        ++fired;
        if (ddl)
          f.exec(("CREATE TABLE concurrent_" + std::to_string(fired) + "(value)").c_str());
        else
          f.exec(("UPDATE sessions SET status='other-" + std::to_string(fired) + "'").c_str());
        if (fired < invalidations)
          fault::journal_read_done = callback;
      };
      fault::journal_read_done = callback;
      std::string failure;
      try {
        f.store.reconcile(f.session);
      } catch (const Error &error) {
        failure = error.what();
      }
      EXPECT_FALSE(fault::journal_read_done);
      EXPECT_EQ(fired, invalidations);
      if (invalidations == 1) {
        EXPECT_TRUE(failure.empty()) << failure;
      } else {
        EXPECT_NE(failure.find("preparation changed twice"), std::string::npos) << failure;
        EXPECT_EQ(Json(f.session.messages), before);
        EXPECT_EQ(f.session.checkpoint_projection, projection);
        EXPECT_EQ(f.value("SELECT payload FROM session_messages WHERE position=0"), stored);
      }
    }
}

TEST_F(PublicationFault, OptionalTokenSnapshotLossUsesAlreadyCapturedLoadInputs) {
  ProjectionFixture f;
  Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"pending", "exec_command", Json::object()}};
  f.session.messages.push_back(assistant);
  f.store.append_messages(f.session, 1);
  fault::failed_query = "SELECT revision FROM provider_revisions WHERE session=?";
  fault::failed_status = SQLITE_IOERR;
  fault::failed_rollback = true;
  Session loaded;
  EXPECT_NO_THROW(loaded = f.store.load_id(f.session.id));
  const auto consumed = fault::failed_query == nullptr;
  const auto steps = fault::steps_after_failure;
  fault::disable();
  EXPECT_TRUE(consumed);
  EXPECT_EQ(steps, 0u);
  ASSERT_EQ(loaded.messages.size(), 3u);
  EXPECT_NE(loaded.messages.back().text.find("not_started"), std::string::npos);
  EXPECT_EQ(f.store.provider_cache_bytes(), 0u);
  EXPECT_NO_THROW(f.store.reconcile(loaded));
}

TEST_F(PublicationFault, LegacySnapshotHandoffChecksItsVersionThenRestoresSuffixOnlyWrites) {
  ProjectionFixture f(true);
  auto copied = f.session;
  f.store.revise_messages(f.session, {});
  copied.messages.push_back(Message{"user", "first suffix"});
  const auto incoming = Json(copied.messages);
  const auto provenance = copied.checkpoint_projection;
  bool fired = false;
  fault::before_write = [&] {
    fired = true;
    f.exec("UPDATE sessions SET status='concurrent metadata'");
  };
  EXPECT_NO_THROW(f.store.append_messages(copied, 1));
  EXPECT_TRUE(fired);
  EXPECT_FALSE(fault::before_write);
  EXPECT_EQ(copied.messages[0].message_id, incoming[0]["message_id"]);
  EXPECT_NE(copied.checkpoint_projection, provenance);
  EXPECT_EQ(f.value("SELECT count(*) FROM session_messages"), "2");
  fault::transcript_rows_read = fault::checkpoint_reads = 0;
  copied.messages.push_back(Message{"user", "ordinary suffix"});
  EXPECT_NO_THROW(f.store.append_messages(copied, 2));
  EXPECT_EQ(fault::transcript_rows_read, 0u);
  EXPECT_EQ(fault::checkpoint_reads, 0u);
}

TEST_F(PublicationFault, SessionRevisionRetriesOnlySelectedChangesAndKeepsToolOutput) {
  for (bool unrelated : {false, true})
    for (int invalidations : {1, 2}) {
      SCOPED_TRACE(std::to_string(unrelated) + ":" + std::to_string(invalidations));
      ProjectionFixture f(true);
      auto copied = f.session;
      f.store.revise_messages(f.session, {});
      auto other = f.store.create();
      Message assistant;
      assistant.role = "assistant";
      ToolCall call{"completed-once", "exec_command", Json::object()};
      assistant.calls = {call};
      copied.messages.push_back(assistant);
      f.store.append_messages(copied, 1);
      f.store.tool_started(copied.id, call);
      int executed = 0, commits = 0;
      Message result;
      result.role = "tool";
      result.tool_call_id = call.id;
      result.text = "returned output " + std::to_string(++executed);
      std::function<void()> commit;
      commit = [&] {
        ++commits;
        f.exec(("UPDATE sessions SET status='changed-" + std::to_string(commits) + "' WHERE id='" +
                (unrelated ? other.id : copied.id) + "'")
                   .c_str());
        if (commits < invalidations)
          fault::before_write = commit;
      };
      fault::before_write = commit;
      std::string failure;
      try {
        f.store.complete_tool(copied, call, result);
      } catch (const Error &error) {
        failure = error.what();
      }
      fault::before_write = {};
      EXPECT_EQ(executed, 1);
      if (unrelated || invalidations == 1) {
        EXPECT_TRUE(failure.empty()) << failure;
        EXPECT_EQ(copied.messages.back().text, result.text);
        EXPECT_EQ(f.value("SELECT result FROM tools WHERE call_id='completed-once'"),
                  Json(copied.messages.back()).dump());
        EXPECT_EQ(commits, unrelated ? 1 : invalidations);
      } else {
        EXPECT_NE(failure.find("changed twice"), std::string::npos) << failure;
        EXPECT_NE(failure.find("tool result preserved at"), std::string::npos) << failure;
        EXPECT_EQ(f.value("SELECT status FROM tools WHERE call_id='completed-once'"), "running");
        EXPECT_EQ(copied.messages.size(), 2u);
      }
    }
}
TEST_F(PublicationFault, RepairSelectedSameConnectionChangesRetryAndUnrelatedChangesDoNot) {
  for (bool unrelated : {false, true})
    for (int invalidations : {1, 2}) {
      ProjectionFixture f;
      Config config;
      config.home = f.temp.root / "home";
      config.cwd = f.temp.root;
      config.state_root = f.temp.root / "state";
      config.artifact_root = f.root;
      config.roots = {f.temp.root};
      NativeProcesses processes;
      NoHttp http;
      SystemClock clock;
      Tools tools(config, f.files, processes, http, clock);
      auto other = f.store.create();
      int commits = 0;
      std::function<void()> commit;
      commit = [&] {
        ++commits;
        f.exec(("UPDATE sessions SET name='changed-" + std::to_string(commits) + "' WHERE id='" +
                (unrelated ? other.id : f.session.id) + "'")
                   .c_str());
        if (commits < invalidations)
          fault::before_write = commit;
      };
      fault::before_write = commit;
      std::string failure;
      Json report;
      try {
        report = f.store.repair(f.session.id, false, true, false, tools);
      } catch (const Error &error) {
        failure = error.what();
      }
      fault::before_write = {};
      if (unrelated || invalidations == 1) {
        EXPECT_TRUE(failure.empty()) << failure;
        EXPECT_EQ(commits, unrelated ? 1 : invalidations);
        EXPECT_EQ(report.contains("backups"), !unrelated && invalidations == 1);
      } else {
        EXPECT_NE(failure.find("changed twice"), std::string::npos) << failure;
        EXPECT_NE(failure.find("; backup:"), std::string::npos);
      }
    }
}
TEST_F(PublicationFault, RequiredSessionRevisionFailuresAbortWithoutMutationOrFurtherReads) {
  for (bool rollback : {false, true})
    for (int status : {SQLITE_IOERR, SQLITE_BUSY, SQLITE_CORRUPT, SQLITE_NOMEM}) {
      ProjectionFixture f;
      const auto stored = f.value("SELECT payload FROM session_messages WHERE position=0");
      fault::failed_query = "SELECT revision FROM session_revisions WHERE session=?";
      fault::failed_status = status;
      fault::failed_allocation = status == SQLITE_NOMEM;
      fault::failed_rollback = rollback;
      fault::steps_after_failure = 0;
      std::string failure;
      try {
        (void)f.store.load_id(f.session.id);
        FAIL();
      } catch (const std::bad_alloc &) {
        failure = "allocation";
      } catch (const Error &error) {
        failure = error.what();
      }
      const auto consumed = fault::failed_query == nullptr;
      const auto steps = fault::steps_after_failure;
      fault::disable();
      EXPECT_TRUE(consumed);
      EXPECT_EQ(steps, 0u);
      EXPECT_NE(
          failure.find(status == SQLITE_NOMEM ? "allocation" : "Cannot inspect session revision"),
          std::string::npos)
          << failure;
      EXPECT_EQ(f.value("SELECT payload FROM session_messages WHERE position=0"), stored);
      EXPECT_NO_THROW(f.store.save(f.session));
    }
}
TEST_F(PublicationFault, ConstLegacyValidationSerializesOnlySuffixAndMutableHandoffPublishesIds) {
  ProjectionFixture f(true);
  std::vector<Message> history = f.session.messages;
  for (int i = 0; i < 128; ++i)
    history.push_back(Message{"user", std::string(1024, 'a')});
  const auto blob = Json(history).dump();
  sqlite3_stmt *update = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(f.db, "UPDATE sessions SET messages=?", -1, &update, nullptr),
            SQLITE_OK);
  sqlite3_bind_text(update, 1, blob.c_str(), -1, SQLITE_TRANSIENT);
  ASSERT_EQ(sqlite3_step(update), SQLITE_DONE);
  sqlite3_finalize(update);
  f.session = f.store.load_id(f.session.id, false);
  const Session &view = f.session;
  f.store.revise_messages(view, {});
  for (bool mutable_write : {false, true}) {
    auto first = f.session.messages.size();
    f.session.messages.push_back(Message{"user", "new suffix"});
    fault::transcript_rows_read = fault::checkpoint_reads = 0;
    const auto bytes = f.store.serialized_bytes();
    if (mutable_write)
      f.store.append_messages(f.session, first);
    else
      f.store.append_messages(view, first);
    EXPECT_GT(fault::transcript_rows_read, 128u);
    EXPECT_LT(f.store.serialized_bytes() - bytes, 1024u);
  }
  EXPECT_EQ(Json(f.store.load_id(f.session.id, false).messages), Json(f.session.messages));
  fault::transcript_rows_read = fault::checkpoint_reads = 0;
  auto first = f.session.messages.size();
  f.session.messages.push_back(Message{"user", "ordinary append"});
  f.store.append_messages(f.session, first);
  EXPECT_EQ(fault::transcript_rows_read, 0u);
  EXPECT_EQ(fault::checkpoint_reads, 0u);
}
