// A separate executable confines allocator/SQLite interception to deterministic fault tests.
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
size_t steps_after_failure = 0;
int countdown = -1, skip = 0;
std::function<void()> provider_read_done;
void arm() noexcept {
  countdown = skip;
}
void disable() noexcept {
  after_commit = after_token = false;
  countdown = -1;
  provider_read_error = rollback_read = token_error = read_failed = false;
  infrastructure_error = false;
  provider_read_done = {};
}
} // namespace fault
void *operator new(std::size_t n) {
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
extern "C" int sqlite3_exec(sqlite3 *db, const char *sql,
                            int (*callback)(void *, int, char **, char **), void *context,
                            char **error) {
  using Execute =
      int (*)(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **);
  static const auto execute = reinterpret_cast<Execute>(dlsym(RTLD_NEXT, "sqlite3_exec"));
  const int result = execute(db, sql, callback, context, error);
  if (result == SQLITE_OK && fault::after_commit && std::strcmp(sql, "COMMIT") == 0) {
    fault::after_commit = false;
    fault::arm();
  }
  return result;
}
extern "C" int sqlite3_step(sqlite3_stmt *statement) {
  using Step = int (*)(sqlite3_stmt *);
  static const auto step = reinterpret_cast<Step>(dlsym(RTLD_NEXT, "sqlite3_step"));
  if (fault::read_failed)
    ++fault::steps_after_failure;
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
  if (result == SQLITE_ROW && fault::after_token &&
      std::strcmp(sqlite3_sql(statement),
                  "SELECT revision FROM provider_revisions WHERE session=?") == 0) {
    fault::after_token = false;
    fault::arm();
  }
  return result;
}
namespace {
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
TEST(PublicationFault, OwningWritesRemainSuccessfulAndTheNextSaveWorksWithoutReload) {
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
TEST(PublicationFault, LoadCacheFailureDoesNotFailAValidLoad) {
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
TEST(PublicationFault, AgentKeepsCommittedResponseAndProviderAdapterWithoutAReceipt) {
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

TEST(PublicationFault, ActualLoadKeepsOldProviderRowsUnderTheOldRevisionToken) {
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

TEST(PublicationFault, TokenAllocationAndQueryFailuresLeaveValidLoadsSuccessful) {
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
    EXPECT_FALSE(failed);
    if (mode == 0)
      EXPECT_TRUE(fault::fired);
    EXPECT_EQ(loaded.provider_state, s.provider_state);
    EXPECT_EQ(store.provider_cache_bytes(), 0u);
    EXPECT_EQ(store.provider_revision(s.id), 0u);
    EXPECT_NO_THROW(store.save(loaded));
  }
}
TEST(PublicationFault, RepairStopsImmediatelyOnOperationalReadFailureWithOrWithoutRollback) {
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
    fault::steps_after_failure = 0;
    std::string failure;
    try {
      (void)store.repair(s.id, false, true, false, tools, files);
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
TEST(PublicationFault, InvalidRevisionTokensRemainMandatoryLoadErrors) {
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
