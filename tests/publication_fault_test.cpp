// A separate executable confines allocator/SQLite interception to deterministic fault tests.
#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/detail/cli_diagnostics.hpp"
#include "codet/detail/terminal_resources.hpp"
#include "codet/diagnostics.hpp"
#include "codet/prompt.hpp"
#include "journal_fixture.hpp"
#include "journal_metrics.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <new>
#include <sqlite3.h>
#include <typeinfo>
using namespace codet;
namespace fault {
bool after_commit = false, after_token = false, fired = false;
bool provider_read_error = false, rollback_read = false, token_error = false;
bool infrastructure_error = false;
bool candidate_read_error = false;
size_t candidate_read_skip = 0;
bool read_failed = false;
sqlite3 *observed_connection = nullptr;
sqlite3 *before_write_connection = nullptr;
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
  codet::test::journal_metrics = {};
  after_commit = after_token = false;
  countdown = -1;
  provider_read_error = rollback_read = token_error = read_failed = false;
  infrastructure_error = candidate_read_error = false;
  candidate_read_skip = 0;
  provider_read_done = {};
  journal_read_done = {};
  before_write = {};
  before_write_connection = nullptr;
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
  if (std::strcmp(sql, "BEGIN IMMEDIATE") == 0 && fault::before_write &&
      (!fault::before_write_connection || fault::before_write_connection == db)) {
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
  codet::test::journal_metrics.before(statement);
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
  if (fault::candidate_read_error && codet::test::JournalMetrics::candidate(statement) &&
      !sqlite3_stmt_busy(statement)) {
    if (fault::candidate_read_skip)
      --fault::candidate_read_skip;
    else {
      fault::candidate_read_error = false;
      fault::read_failed = true;
      return SQLITE_IOERR;
    }
  }
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
  codet::test::journal_metrics.after(statement, result);
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
TEST(FileDiagnosticsFault, OwnershipTransferAndEmergencyFlushDoNotAllocate) {
  FileDiagnosticQueue queue;
  auto notice = std::make_unique<FileNotice>(std::string(4096, 'p'), FileNoticeKind::tool_result);
  notice->confirmed = true;
  testing::internal::CaptureStderr();
  fault::fired = false;
  fault::countdown = 0;
  queue.submit(std::move(notice));
  queue.flush();
  const bool allocated = fault::fired;
  fault::disable();
  const auto output = testing::internal::GetCapturedStderr();
  EXPECT_FALSE(allocated);
  EXPECT_EQ(output, "Uncommitted tool result preserved at " + std::string(4096, 'p') + "\n");
}
TEST(FileDiagnosticsFault, ClearingOrdinaryDiagnosticsDoesNotAllocateOrLoseTheBudget) {
  DiagnosticBuffer buffer(256);
  buffer.add(std::string(1024, 'x'));
  fault::fired = false;
  fault::countdown = 0;
  buffer.clear();
  const bool allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(allocated);
  EXPECT_EQ(buffer.add("after clear"), "after clear");
}
TEST(FileDiagnosticsFault, PartialDiagnosticRecoveryAndSeparatorRetentionDoNotAllocate) {
  struct FixedStream : std::streambuf {
    char bytes[64]{};
    size_t used = 0;
    bool partial = true;
    std::streamsize xsputn(const char *source, std::streamsize count) override {
      const auto size =
          std::min(static_cast<size_t>(count), partial ? size_t{2} : sizeof(bytes) - used);
      std::memcpy(bytes + used, source, size);
      used += size;
      return static_cast<std::streamsize>(size);
    }
    int overflow(int character) override {
      const auto byte = static_cast<char>(character);
      return xsputn(&byte, 1) == 1 ? character : traits_type::eof();
    }
  } buffer;
  std::ostream output(&buffer);
  detail::DiagnosticWriter writer(output);
  fault::fired = false;
  fault::countdown = 0;
  const bool first = writer.write({"first"});
  buffer.partial = false;
  const bool second = writer.write({"second"});
  const bool allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(allocated);
  EXPECT_FALSE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(std::string(buffer.bytes, buffer.used), "fi\nsecond\n");
}
TEST(FileDiagnosticsFault, LiveReplayAllocationFailureDoesNotDiscardCleanupOrNotices) {
  struct FixedStream : std::streambuf {
    char bytes[4096]{};
    size_t used = 0;
    std::streamsize xsputn(const char *source, std::streamsize count) override {
      const auto size = std::min(static_cast<size_t>(count), sizeof(bytes) - used);
      std::memcpy(bytes + used, source, size);
      used += size;
      return static_cast<std::streamsize>(size);
    }
    int overflow(int character) override {
      if (character == traits_type::eof())
        return traits_type::not_eof(character);
      const auto byte = static_cast<char>(character);
      return xsputn(&byte, 1) == 1 ? character : traits_type::eof();
    }
  } buffer;
  std::ostream output(&buffer);
  TempDirectory temp;
  StateStore state(temp.root / "state");
  detail::CliDiagnostics diagnostics(output);
  diagnostics.live = DiagnosticBuffer(256);
  diagnostics.live.add(std::string(1024, 'x'));
  diagnostics.cleanup.add("SessionEnd survives allocation failure");
  diagnostics.files->submit(std::make_unique<FileNotice>("held-path", FileNoticeKind::tool_result));
  fault::fired = false;
  fault::countdown = 0;
  diagnostics.restored(state);
  const bool injected = fault::fired;
  fault::disable();
  EXPECT_TRUE(injected);
  const std::string text(buffer.bytes, buffer.used);
  EXPECT_NE(text.find("SessionEnd survives allocation failure\n"), std::string::npos);
  EXPECT_NE(text.find("held-path\n"), std::string::npos);
}
TEST(FileDiagnosticsFault, DefaultFileSinkIsAvailableWithoutAllocating) {
  const auto child = test::start_helper({"--codet-file-sink-helper", "first-use"});
  ASSERT_GT(child, 0);
  int status = 0;
  ASSERT_TRUE(test::wait_helper(child, status));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}
TEST(FileDiagnosticsFault, DefaultFileSinkRemainsAvailableDuringLateStaticDestruction) {
  testing::internal::CaptureStderr();
  const auto child = test::start_helper({"--codet-file-sink-helper", "late-static"});
  int status = 0;
  const bool waited = child > 0 && test::wait_helper(child, status);
  const auto output = testing::internal::GetCapturedStderr();
  ASSERT_TRUE(waited);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_EQ(output, "late static sink\n");
}
TEST(FileDiagnosticsFault, NativeResourceCleanupDoesNotAllocate) {
  struct Ops final : detail::TerminalResourceOps {
    size_t released = 0;
    bool capture(termios &) noexcept override {
      return true;
    }
    bool apply(const termios &) noexcept override {
      return true;
    }
    void restore(const termios &) noexcept override {
      ++released;
    }
    bool start_screen() override {
      return true;
    }
    void stop_screen() noexcept override {
      ++released;
    }
    EditLine *start_editor() override {
      return reinterpret_cast<EditLine *>(this);
    }
    void stop_editor(EditLine *) noexcept override {
      ++released;
    }
    History *start_history() override {
      return reinterpret_cast<History *>(this);
    }
    void stop_history(History *) noexcept override {
      ++released;
    }
  } ops;
  fault::disable();
  {
    detail::TerminalResources resources(ops);
    resources.start_screen();
    resources.start_editor();
    resources.start_history();
    fault::fired = false;
    fault::countdown = 0;
    resources.close();
    resources.close();
  }
  const bool allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(allocated);
  EXPECT_EQ(ops.released, 4u);
}
TEST(FileDiagnosticsFault, PreviewAllocationFailureRetainsTheOwnedPathUntilFinish) {
  FileDiagnosticQueue queue;
  auto notice = std::make_unique<FileNotice>(std::string(4096, 'p'), FileNoticeKind::repair_backup);
  DiagnosticSink sink = [](const auto &) { ADD_FAILURE() << "text allocation should fail first"; };
  queue.submit(std::move(notice));
  testing::internal::CaptureStderr();
  fault::fired = false;
  fault::countdown = 0;
  queue.preview(sink);
  const bool failed_text = fault::fired;
  fault::fired = false;
  fault::countdown = 0;
  queue.finish(sink);
  const bool allocated_during_fallback = fault::fired;
  fault::disable();
  const auto output = testing::internal::GetCapturedStderr();
  EXPECT_TRUE(failed_text);
  EXPECT_FALSE(allocated_during_fallback);
  EXPECT_EQ(output, "Session repair backup candidate (publication unconfirmed): " +
                        std::string(4096, 'p') + "\n");
}
TEST(FileDiagnosticsFault, SuccessfulPreviewRetentionAndEmergencyFlushDoNotAllocate) {
  FileDiagnosticQueue queue;
  queue.submit(std::make_unique<FileNotice>(std::string(4096, 'p'), FileNoticeKind::tool_result));
  DiagnosticSink sink = [](const auto &) { fault::countdown = 0; };
  testing::internal::CaptureStderr();
  fault::fired = false;
  queue.preview(sink);
  queue.flush();
  const bool allocated = fault::fired;
  fault::disable();
  const auto output = testing::internal::GetCapturedStderr();
  EXPECT_FALSE(allocated);
  EXPECT_EQ(output, "Tool result receipt candidate (publication unconfirmed): " +
                        std::string(4096, 'p') + "\n");
}
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
      std::string diagnostics;
      f.store.set_diagnostics([&](const auto &text) { diagnostics += text; });
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
      int commits = 0;
      Message result;
      result.role = "tool";
      result.tool_call_id = call.id;
      result.text = "returned output";
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
      if (unrelated || invalidations == 1) {
        EXPECT_TRUE(failure.empty()) << failure;
        EXPECT_EQ(copied.messages.back().text, result.text);
        EXPECT_EQ(f.value("SELECT result FROM tools WHERE call_id='completed-once'"),
                  Json(copied.messages.back()).dump());
        EXPECT_EQ(commits, unrelated ? 1 : invalidations);
      } else {
        EXPECT_NE(failure.find("changed twice"), std::string::npos) << failure;
        EXPECT_NE(diagnostics.find("tool result preserved at"), std::string::npos) << failure;
        EXPECT_EQ(f.value("SELECT status FROM tools WHERE call_id='completed-once'"), "running");
        EXPECT_EQ(copied.messages.size(), 2u);
      }
    }
}
TEST_F(PublicationFault, RepairSelectedSameConnectionChangesRetryAndUnrelatedChangesDoNot) {
  for (bool unrelated : {false, true})
    for (int invalidations : {1, 2}) {
      ProjectionFixture f;
      std::string diagnostics;
      f.store.set_diagnostics([&](const auto &text) { diagnostics += text; });
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
        EXPECT_NE(diagnostics.find("Session repair backup:"), std::string::npos);
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

namespace {
Message pending_call(const std::string &id) {
  Message message;
  message.role = "assistant";
  message.calls = {{id, "exec_command", Json::object()}};
  return message;
}
Message returned_result(const std::string &id) {
  Message result;
  result.role = "tool";
  result.tool_call_id = id;
  result.text = "actual returned output";
  return result;
}
void make_legacy(ProjectionFixture &f) {
  const auto bytes = Json(f.session.messages).dump();
  sqlite3_stmt *update = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(f.db, "UPDATE sessions SET message_format=0,messages=?", -1, &update,
                               nullptr),
            SQLITE_OK);
  sqlite3_bind_text(update, 1, bytes.c_str(), -1, SQLITE_TRANSIENT);
  ASSERT_EQ(sqlite3_step(update), SQLITE_DONE);
  sqlite3_finalize(update);
  f.exec("DELETE FROM session_messages");
}
} // namespace
TEST_F(PublicationFault, RecoverySeparatesStoredBaselineFromUnsavedSuffixAndRequestedEdits) {
  for (bool blob : {false, true})
    for (bool immutable : {false, true})
      for (bool revision : {false, true}) {
        ProjectionFixture f;
        if (revision) {
          f.session.messages.push_back(pending_call("historical"));
          f.store.append_messages(f.session, 1);
        }
        if (blob)
          make_legacy(f);
        f.session = f.store.load_id(f.session.id, false);
        if (revision)
          f.session.messages[0].text = "requested caller edit";
        else
          f.session.messages.push_back(pending_call("unsaved"));
        f.store.reconcile(f.session, false);
        const Session &view = f.session;
        if (revision) {
          if (immutable) {
            ASSERT_NO_THROW(f.store.revise_messages(view, {0}));
          } else {
            ASSERT_NO_THROW(f.store.revise_messages(f.session, {0}));
          }
        } else if (immutable) {
          ASSERT_NO_THROW(f.store.append_messages(view, 1));
        } else {
          ASSERT_NO_THROW(f.store.append_messages(f.session, 1));
        }
        const auto loaded = f.store.load_id(f.session.id);
        ASSERT_EQ(loaded.messages.size(), 3u);
        EXPECT_EQ(loaded.messages[0].text, revision ? "requested caller edit" : "summary");
        EXPECT_EQ(loaded.messages.back().tool_call_id, revision ? "historical" : "unsaved");
      }
}
TEST_F(PublicationFault, ReadOnlyRecoveryAndForkSupportMissingTokensAndNeverSavedParents) {
  ProjectionFixture f;
  auto parent = f.session;
  parent.messages.push_back(Message{"user", "unsaved suffix"});
  f.exec("DELETE FROM session_revisions");
  ASSERT_NO_THROW(f.store.reconcile(parent, false));
  EXPECT_EQ(f.value("SELECT count(*) FROM session_revisions"), "0");
  auto child = f.store.fork(parent);
  EXPECT_EQ(child.messages.back().text, "unsaved suffix");
  EXPECT_EQ(f.value("SELECT count(*) FROM session_revisions"), "1");
  Session memory{unique_id(), "never saved", {Message{"user", "memory only"}}};
  child = f.store.fork(memory);
  ASSERT_EQ(child.messages.size(), 1u);
  EXPECT_EQ(child.messages[0].text, "memory only");
}
TEST_F(PublicationFault, EveryExistingSessionMutatorRejectsDamagedTokensBeforeRotatingThem) {
  for (bool missing : {false, true})
    for (const auto *mode :
         {"save", "const-save", "append", "revise", "replace", "metadata", "provider", "begin",
          "finish", "start", "finished", "complete", "references"}) {
      SCOPED_TRACE(std::string(mode) + (missing ? ":missing" : ":malformed"));
      ProjectionFixture f;
      f.session.messages.push_back(pending_call("running"));
      f.store.append_messages(f.session, 1);
      const auto call = f.session.messages.back().calls.front();
      f.store.tool_started(f.session.id, call);
      f.exec(missing ? "DELETE FROM session_revisions"
                     : "UPDATE session_revisions SET revision='broken'");
      const auto rows = f.value("SELECT group_concat(payload) FROM session_messages");
      auto result = returned_result(call.id);
      auto &s = f.session;
      const Session &view = s;
      std::string diagnostics;
      f.store.set_diagnostics([&](const auto &text) { diagnostics += text; });
      EXPECT_NO_THROW(f.store.load_id(s.id));
      EXPECT_NO_THROW(f.store.last());
      const auto action = [&] {
        if (std::strcmp(mode, "save") == 0)
          f.store.save(s);
        else if (std::strcmp(mode, "const-save") == 0)
          f.store.save(view);
        else if (std::strcmp(mode, "append") == 0)
          f.store.append_messages(s, s.messages.size());
        else if (std::strcmp(mode, "revise") == 0)
          f.store.revise_messages(s, {});
        else if (std::strcmp(mode, "replace") == 0)
          f.store.replace_history(s);
        else if (std::strcmp(mode, "metadata") == 0)
          f.store.update_metadata(s);
        else if (std::strcmp(mode, "provider") == 0)
          f.store.save_provider_state(s);
        else if (std::strcmp(mode, "begin") == 0)
          f.store.begin_turn(s.id);
        else if (std::strcmp(mode, "finish") == 0)
          f.store.finish_turn(s.id, "complete");
        else if (std::strcmp(mode, "start") == 0)
          f.store.tool_started(s.id, {"new", "exec_command", Json::object()});
        else if (std::strcmp(mode, "finished") == 0)
          f.store.tool_finished(s.id, call, Json(result));
        else if (std::strcmp(mode, "complete") == 0)
          f.store.complete_tool(s, call, result);
        else {
          result.result_file = "results/owner/new.json";
          f.store.remember_references(s.id, {result});
        }
      };
      EXPECT_THROW(action(), Error);
      EXPECT_EQ(f.value("SELECT group_concat(payload) FROM session_messages"), rows);
      EXPECT_EQ(f.value("SELECT status FROM tools WHERE call_id='running'"), "running");
      EXPECT_EQ(f.value("SELECT count(*) FROM session_revisions"), missing ? "0" : "1");
      if (!missing)
        EXPECT_EQ(f.value("SELECT revision FROM session_revisions"), "broken");
    }
}
TEST_F(PublicationFault, TokenOnlyRepairPreservesRawRowsAndRequiresNoDecoding) {
  for (bool missing : {false, true}) {
    ProjectionFixture f(true);
    f.exec("UPDATE sessions SET messages='unreadable legacy JSON';INSERT INTO provider_parts "
           "VALUES((SELECT id FROM sessions),0,'unreadable provider JSON')");
    f.exec(missing ? "DELETE FROM session_revisions"
                   : "UPDATE session_revisions SET revision='broken'");
    const auto messages = f.value("SELECT messages FROM sessions");
    const auto provider = f.value("SELECT payload FROM provider_parts");
    const auto provider_token = f.value("SELECT revision FROM provider_revisions");
    const auto metadata = f.value("SELECT name||status||updated||message_format FROM sessions");
    auto report = f.store.repair_session_token(f.session.id, false);
    EXPECT_EQ(report["session_token_regenerated"], true);
    EXPECT_EQ(report["provider_cache_reset"], false);
    const auto backup = Json::parse(f.files.read(report["backup"].get<std::string>()));
    EXPECT_EQ(backup["session_revisions"].size(), missing ? 0u : 1u);
    EXPECT_EQ(f.value("SELECT messages FROM sessions"), messages);
    EXPECT_EQ(f.value("SELECT payload FROM provider_parts"), provider);
    EXPECT_EQ(f.value("SELECT revision FROM provider_revisions"), provider_token);
    EXPECT_EQ(f.value("SELECT name||status||updated||message_format FROM sessions"), metadata);
    report = f.store.repair_session_token("", true);
    EXPECT_EQ(report["changed"], false);
    EXPECT_FALSE(report.contains("backup"));
  }
}
TEST_F(PublicationFault, TokenOnlyRepairRetriesOnceAndReportsBackupsSeparately) {
  for (int invalidations : {1, 2}) {
    ProjectionFixture f;
    f.exec("UPDATE session_revisions SET revision='broken'");
    int changed = 0;
    std::string diagnostics;
    f.store.set_diagnostics([&](const auto &text) { diagnostics += text; });
    std::function<void()> mutate;
    mutate = [&] {
      ++changed;
      f.exec(("UPDATE session_revisions SET revision='broken-" + std::to_string(changed) + "'")
                 .c_str());
      if (changed < invalidations)
        fault::before_write = mutate;
    };
    fault::before_write = mutate;
    if (invalidations == 1) {
      auto report = f.store.repair_session_token(f.session.id, false);
      EXPECT_EQ(report["backups"].size(), 2u);
    } else {
      EXPECT_THROW(f.store.repair_session_token(f.session.id, false), Error);
      EXPECT_NE(diagnostics.find("Session repair backup:"), std::string::npos);
      EXPECT_EQ(f.value("SELECT revision FROM session_revisions"), "broken-2");
    }
    EXPECT_EQ(changed, invalidations);
  }
}
TEST_F(PublicationFault, ToolCompletionReplacesProvisionalResultsAndCanRepeatIdentically) {
  for (bool blob : {false, true}) {
    ProjectionFixture f;
    f.session.messages.push_back(pending_call("running"));
    f.store.append_messages(f.session, 1);
    const auto call = f.session.messages.back().calls.front();
    f.store.tool_started(f.session.id, call);
    if (blob)
      make_legacy(f);
    auto recovered = f.store.load_id(f.session.id);
    ASSERT_EQ(recovered.messages.size(), 3u);
    const auto result_id = recovered.messages.back().message_id;
    ASSERT_NO_THROW(f.store.complete_tool(recovered, call, returned_result(call.id)));
    EXPECT_EQ(recovered.messages.size(), 3u);
    EXPECT_EQ(recovered.messages.back().message_id, result_id);
    EXPECT_EQ(recovered.messages.back().text, "actual returned output");
    EXPECT_EQ(Json(f.store.load_id(recovered.id).messages), Json(recovered.messages));
    ASSERT_NO_THROW(f.store.complete_tool(recovered, call, returned_result(call.id)));
    auto different = returned_result(call.id);
    different.text = "different output";
    std::string diagnostics;
    f.store.set_diagnostics([&](const auto &text) { diagnostics += text; });
    EXPECT_THROW(f.store.complete_tool(recovered, call, different), Error);
    EXPECT_EQ(f.store.load_id(recovered.id).messages.back().text, "actual returned output");
    EXPECT_NE(diagnostics.find("Uncommitted tool result preserved at"), std::string::npos);
  }
}
TEST_F(PublicationFault, RetainedConstRecoveryDoesNotSynthesizeOtherLiveCalls) {
  ProjectionFixture f;
  f.session.messages.push_back(pending_call("historical"));
  f.store.append_messages(f.session, 1);
  auto recovered = f.store.load_id(f.session.id);
  auto live = pending_call("first-live");
  live.calls.push_back({"second-live", "exec_command", Json::object()});
  const auto first = recovered.messages.size();
  recovered.messages.push_back(live);
  const Session &view = recovered;
  f.store.append_messages(view, first);
  for (const auto &call : live.calls)
    f.store.tool_started(recovered.id, call);
  ASSERT_NO_THROW(f.store.complete_tool(recovered, live.calls[0], returned_result("first-live")));
  EXPECT_EQ(f.value("SELECT status FROM tools WHERE call_id='second-live'"), "running");
  EXPECT_EQ(recovered.messages.size(), 5u);
  ASSERT_NO_THROW(f.store.complete_tool(recovered, live.calls[1], returned_result("second-live")));
  EXPECT_EQ(recovered.messages.size(), 6u);
  EXPECT_EQ(Json(f.store.load_id(recovered.id).messages), Json(recovered.messages));
}
TEST_F(PublicationFault, UnavailableAbsoluteCheckpointCanRepeatConstRecoveryWritesWithoutReload) {
  for (bool blob : {false, true}) {
    ProjectionFixture f;
    f.session.messages.push_back(pending_call("historical"));
    f.store.append_messages(f.session, 1);
    if (blob)
      make_legacy(f);
    fs::remove(f.archive);
    auto recovered = f.store.load_id(f.session.id);
    const Session &view = recovered;
    for (int i = 0; i < 3; ++i) {
      const auto first = recovered.messages.size();
      recovered.messages.push_back(Message{"user", "suffix"});
      ASSERT_NO_THROW(f.store.append_messages(view, first));
      // Token changes force the strict fallback rather than the receipt fast path.
      f.exec("UPDATE sessions SET status='unrelated metadata change'");
    }
    EXPECT_EQ(f.store.load_id(recovered.id).messages.size(), recovered.messages.size());
  }
}
TEST_F(PublicationFault, ConstRecoveryReceiptSkipsSqlScansButChecksCallerPrefixAndExternalBytes) {
  ProjectionFixture f;
  f.session.messages.push_back(pending_call("historical"));
  f.store.append_messages(f.session, 1);
  auto recovered = f.store.load_id(f.session.id);
  const Session &view = recovered;
  for (int i = 0; i < 3; ++i) {
    const auto first = recovered.messages.size();
    recovered.messages.push_back(Message{"user", "suffix"});
    fault::transcript_rows_read = 0;
    ASSERT_NO_THROW(f.store.append_messages(view, first));
    if (i) {
      EXPECT_EQ(fault::transcript_rows_read, 0u);
    }
  }
  auto previous = recovered.messages[0].text;
  recovered.messages[0].text = "unrequested edit";
  EXPECT_THROW(f.store.revise_messages(view, {}), Error);
  recovered.messages[0].text = previous;
  f.exec("UPDATE session_messages SET payload=json_set(payload,'$.unknown_external_field',true) "
         "WHERE position=0");
  EXPECT_THROW(f.store.revise_messages(view, {}), Error);
}
TEST_F(PublicationFault, CompletionKeepsExceptionTypesAndDiagnosticFailuresCannotReplaceThem) {
  ProjectionFixture f;
  f.session.messages.push_back(pending_call("running"));
  f.store.append_messages(f.session, 1);
  const auto call = f.session.messages.back().calls.front();
  f.store.tool_started(f.session.id, call);
  f.store.set_diagnostics([](const auto &) { throw std::bad_alloc(); });
  const auto before = Json(f.session.messages);
  fault::before_write = [] { throw HookFailure("original subtype"); };
  EXPECT_THROW(f.store.complete_tool(f.session, call, returned_result(call.id)), HookFailure);
  fault::before_write = [] { throw std::logic_error("original logic error"); };
  EXPECT_THROW(f.store.complete_tool(f.session, call, returned_result(call.id)), std::logic_error);
  fault::before_write = [] {
    const auto invalid = Json::parse("{invalid");
    (void)invalid;
  };
  EXPECT_THROW(f.store.complete_tool(f.session, call, returned_result(call.id)), Json::exception);
  fault::before_write = [] { throw std::bad_alloc(); };
  EXPECT_THROW(f.store.complete_tool(f.session, call, returned_result(call.id)), std::bad_alloc);
  fault::before_write = [] { throw 42; };
  try {
    f.store.complete_tool(f.session, call, returned_result(call.id));
    FAIL();
  } catch (int value) {
    EXPECT_EQ(value, 42);
  }
  EXPECT_EQ(Json(f.session.messages), before);
  fault::before_write = [] { throw ProviderError("original provider", 429, "original body"); };
  try {
    f.store.complete_tool(f.session, call, returned_result(call.id));
    FAIL();
  } catch (const ProviderError &error) {
    EXPECT_EQ(error.http_status, 429);
    EXPECT_EQ(error.body, "original body");
    EXPECT_STREQ(error.what(), "original provider");
  }
  fault::before_write = [] { throw Error("original cancellation", 130); };
  try {
    f.store.complete_tool(f.session, call, returned_result(call.id));
    FAIL();
  } catch (const Error &error) {
    EXPECT_EQ(error.code, 130);
    EXPECT_STREQ(error.what(), "original cancellation");
  }
}
TEST_F(PublicationFault, ReceiptPathIsReportedWhenDiagnosticConstructionCannotAllocate) {
  struct ReceiptFiles : FileSystem {
    NativeFileSystem native;
    bool arm = false;
    std::string read(const fs::path &path, size_t limit) const override {
      return native.read(path, limit);
    }
    fs::path canonical(const fs::path &path) const override {
      return native.canonical(path);
    }
    void remove(const fs::path &path) override {
      native.remove(path);
    }
    void atomic_write(const fs::path &path, const std::string &bytes, bool secret) override {
      native.atomic_write(path, bytes, secret);
      if (arm) {
        arm = false;
        fault::countdown = 0;
      }
    }
  } files;
  TempDirectory temp;
  StateStore store(files, temp.root / "state");
  auto session = store.create();
  ToolCall call{"call", "exec_command", Json::object()};
  store.tool_started(session.id, call);
  call.name = "different";
  files.arm = true;
  fault::fired = false;
  testing::internal::CaptureStderr();
  std::string failure;
  try {
    store.complete_tool(session, call, returned_result(call.id));
  } catch (const Error &error) {
    failure = error.what();
  }
  const bool allocated = fault::fired;
  fault::disable();
  const auto diagnostic = testing::internal::GetCapturedStderr();
  EXPECT_TRUE(allocated);
  EXPECT_EQ(failure, "Ambiguous journaled tool call id");
  EXPECT_NE(diagnostic.find("Uncommitted tool result preserved at "), std::string::npos);
  size_t receipts = 0;
  for (const auto &entry : fs::directory_iterator(temp.root / "state/repairs")) {
    EXPECT_NE(diagnostic.find(entry.path().string()), std::string::npos);
    ++receipts;
  }
  EXPECT_EQ(receipts, 1u);
}
TEST_F(PublicationFault, RevisionSchemaOperationalFailuresRetainSqliteResultCodes) {
  for (const auto *query : {"PRAGMA schema_version", "SELECT sql FROM sqlite_master WHERE name=?"})
    for (int status : {SQLITE_BUSY, SQLITE_IOERR}) {
      ProjectionFixture f;
      if (std::strcmp(query, "PRAGMA schema_version"))
        f.exec("CREATE TABLE changed_schema(value)");
      fault::failed_query = query;
      fault::failed_status = status;
      try {
        f.store.load_id(f.session.id);
        FAIL();
      } catch (const Error &error) {
        EXPECT_NE(std::string(error.what()).find("SQLite " + std::to_string(status)),
                  std::string::npos);
      }
      fault::disable();
      EXPECT_NO_THROW(f.store.load_id(f.session.id));
    }
}

TEST_F(PublicationFault, AgentExecutesACompletedToolOnceAcrossPublicationRetries) {
  struct OneTool : Provider {
    int requests = 0;
    ProviderResult complete(const Config &, const std::string &, const std::vector<Message> &,
                            const Json &, const Cancellation &, const ChunkSink &,
                            const ItemSink &) override {
      ProviderResult response;
      response.message.role = "assistant";
      if (++requests == 1)
        response.message.calls = {
            {"once", "exec_command", {{"cmd", "printf returned-once"}, {"yield_time_ms", 1000}}}};
      else
        response.message.text = "finished";
      return response;
    }
  };
  struct CountedProcesses : Processes {
    NativeProcesses native;
    int executions = 0;
    std::function<void()> dispatch;
    ProcessResult run(const ProcessSpec &spec, const Cancellation &cancel) override {
      ++executions;
      dispatch();
      return native.run(spec, cancel);
    }
    std::string start(const ProcessSpec &spec) override {
      ++executions;
      dispatch();
      return native.start(spec);
    }
    ProcessResult poll(const std::string &id) override {
      return native.poll(id);
    }
    ProcessStatus status(const std::string &id) override {
      return native.status(id);
    }
    ProcessResult take_output(const std::string &id) override {
      return native.take_output(id);
    }
    void stop(const std::string &id) override {
      native.stop(id);
    }
  };
  for (int invalidations : {1, 2}) {
    TempDirectory temp;
    Config config;
    config.home = temp.root / "home";
    config.cwd = temp.root / "work";
    config.state_root = temp.root / "state";
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "fixture";
    config.approval = "never";
    config.permission_mode = "bypass";
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
    NativeFileSystem files;
    CountedProcesses processes;
    NoHttp http;
    SystemClock clock;
    Cancellation cancel;
    StateStore store(config.state_root);
    auto session = store.create();
    auto *state_connection = fault::observed_connection;
    int changed = 0;
    std::function<void()> invalidate;
    invalidate = [&] {
      ++changed;
      const auto query = "UPDATE sessions SET status='changed-" + std::to_string(changed) + "'";
      ASSERT_EQ(sqlite3_exec(state_connection, query.c_str(), nullptr, nullptr, nullptr),
                SQLITE_OK);
      if (changed < invalidations)
        fault::before_write = invalidate;
    };
    processes.dispatch = [&] {
      fault::before_write_connection = state_connection;
      fault::before_write = invalidate;
    };
    std::string diagnostics;
    store.set_diagnostics([&](const auto &text) { diagnostics += text; });
    OneTool provider;
    Tools tools(config, files, processes, http, clock);
    Agent agent(config, provider, tools, store, {});
    if (invalidations == 1) {
      EXPECT_NO_THROW(agent.run(session, "run the fixture command", cancel));
      const auto loaded = store.load_id(session.id);
      EXPECT_NE(Json(loaded.messages).dump().find("returned-once"), std::string::npos);
      EXPECT_EQ(provider.requests, 2);
    } else {
      EXPECT_THROW(agent.run(session, "run the fixture command", cancel), Error);
      EXPECT_NE(diagnostics.find("Uncommitted tool result preserved at"), std::string::npos);
      EXPECT_EQ(provider.requests, 1);
    }
    fault::disable();
    EXPECT_EQ(changed, invalidations);
    EXPECT_EQ(processes.executions, 1);
  }
}

TEST_F(PublicationFault, CompletionAndConstReceiptAcknowledgementsDoNotAllocateAfterCommit) {
  for (bool completion : {false, true}) {
    ProjectionFixture f;
    f.session.messages.push_back(pending_call("running"));
    f.store.append_messages(f.session, 1);
    const auto call = f.session.messages.back().calls.front();
    f.store.tool_started(f.session.id, call);
    auto recovered = f.store.load_id(f.session.id);
    auto result = returned_result(call.id);
    result.replay = Json::array({{{"nested", Json::array({Json::object(), 1, 2})}}});
    bool failed = false;
    fault::after_commit = true;
    fault::fired = false;
    try {
      if (completion)
        f.store.complete_tool(recovered, call, result);
      else {
        const auto first = recovered.messages.size();
        recovered.messages.push_back(Message{"user", "suffix"});
        const Session &view = recovered;
        f.store.append_messages(view, first);
      }
    } catch (...) {
      failed = true;
    }
    const bool allocated = fault::fired;
    fault::disable();
    EXPECT_FALSE(failed);
    EXPECT_FALSE(allocated);
    EXPECT_NO_THROW(f.store.load_id(f.session.id));
  }
}
TEST_F(PublicationFault, ConstReceiptEvictionUsesFullValidationAndMutableHandoffKeepsCanonicalIds) {
  ProjectionFixture f;
  f.session.messages.push_back(pending_call("historical"));
  f.store.append_messages(f.session, 1);
  auto recovered = f.store.load_id(f.session.id);
  const Session &view = recovered;
  auto first = recovered.messages.size();
  recovered.messages.push_back(Message{"user", "first suffix"});
  f.store.append_messages(view, first);
  auto other = f.store.create();
  f.store.save(other); // Another publication evicts the one optional receipt.
  fault::transcript_rows_read = 0;
  EXPECT_NO_THROW(f.store.revise_messages(view, {}));
  EXPECT_GT(fault::transcript_rows_read, 0u);
  first = recovered.messages.size();
  recovered.messages.push_back(Message{"user", "mutable suffix"});
  EXPECT_NO_THROW(f.store.append_messages(recovered, first));
  EXPECT_EQ(Json(f.store.load_id(recovered.id, false).messages), Json(recovered.messages));
}

TEST_F(PublicationFault, CompletionPreservesPrivateRevisionExceptionIdentity) {
  for (bool infrastructure : {false, true}) {
    ProjectionFixture f;
    f.session.messages.push_back(pending_call("running"));
    f.store.append_messages(f.session, 1);
    const auto call = f.session.messages.back().calls.front();
    f.store.tool_started(f.session.id, call);
    f.store.set_diagnostics([](const auto &) {});
    if (infrastructure)
      f.exec("DROP TRIGGER session_revision_tools_UPDATE");
    else
      f.exec("UPDATE session_revisions SET revision='broken'");
    std::string expected, observed;
    try {
      f.store.save(f.session);
      FAIL();
    } catch (const Error &error) {
      expected = typeid(error).name();
    }
    try {
      f.store.complete_tool(f.session, call, returned_result(call.id));
      FAIL();
    } catch (const Error &error) {
      observed = typeid(error).name();
    }
    EXPECT_EQ(observed, expected);
    EXPECT_NE(observed, typeid(Error).name());
  }
}

TEST_F(PublicationFault, ProvisionalCandidatesUseBoundedIndexedBatches) {
  for (const size_t count : {1u, 2u, 63u, 64u, 65u, 128u, 129u}) {
    ProjectionFixture f;
    codet::test::journal_history(f.store, f.session, f.db, count, "provisional");
    f.exec("INSERT INTO tools(session,call_id,name,args,result,status) "
           "SELECT id,'unrelated','different','{broken','do not read','uncertain' FROM sessions");
    for (auto &message : f.session.messages)
      if (message.role == "tool")
        message.text = "new-" + message.tool_call_id;
    codet::test::journal_metrics = {true};
    ASSERT_NO_THROW(f.store.save(f.session));
    const auto metrics = codet::test::journal_metrics;
    fault::disable();
    EXPECT_EQ(metrics.queries, (count + 63) / 64);
    EXPECT_EQ(metrics.preparations, count <= 64 || count % 64 == 0 ? 1u : 2u);
    for (auto *statement = sqlite3_next_stmt(f.db, nullptr); statement;
         statement = sqlite3_next_stmt(f.db, statement))
      EXPECT_FALSE(codet::test::JournalMetrics::candidate(statement));
    EXPECT_EQ(metrics.rows, count);
    EXPECT_EQ(metrics.fullscan_steps, 0u);
    EXPECT_LE(metrics.max_bindings, 65u);
    const auto loaded = f.store.load_id(f.session.id);
    for (size_t i = 0; i < count; ++i) {
      const auto position = 2 * i + 2;
      EXPECT_EQ(loaded.messages[position].text, f.session.messages[position].text);
      const auto query = "SELECT result FROM tools WHERE call_id='call-" + std::to_string(i) + "'";
      EXPECT_EQ(f.value(query.c_str()), Json(f.session.messages[position]).dump());
    }
    EXPECT_EQ(f.value("SELECT args FROM tools WHERE call_id='unrelated'"), "{broken");
  }
}
TEST_F(PublicationFault, ProvisionalBatchesRespectTheConnectionsVariableLimit) {
  ProjectionFixture f;
  codet::test::journal_history(f.store, f.session, f.db, 15, "provisional");
  const auto previous = sqlite3_limit(f.db, SQLITE_LIMIT_VARIABLE_NUMBER, 8);
  codet::test::journal_metrics = {true};
  EXPECT_NO_THROW(f.store.save(f.session));
  const auto metrics = codet::test::journal_metrics;
  sqlite3_limit(f.db, SQLITE_LIMIT_VARIABLE_NUMBER, previous);
  fault::disable();
  EXPECT_EQ(metrics.queries, 3u);
  EXPECT_EQ(metrics.rows, 15u);
  EXPECT_LE(metrics.max_bindings, 8u);
}
TEST_F(PublicationFault, ProvisionalDuplicatesWithinAndAcrossBatchesRollBack) {
  for (const size_t count : {2u, 64u}) {
    ProjectionFixture f;
    codet::test::journal_history(f.store, f.session, f.db, count, "provisional");
    auto duplicate = f.session.messages[2];
    duplicate.message_id.clear();
    duplicate.text = "duplicate";
    f.session.messages.push_back(std::move(duplicate));
    EXPECT_THROW(f.store.save(f.session), Error);
    EXPECT_EQ(f.value("SELECT count(*) FROM session_messages"), std::to_string(count * 2 + 1));
    EXPECT_EQ(f.store.load_id(f.session.id).messages[2].text, "old-0");
  }
}
TEST_F(PublicationFault, ProvisionalBatchReadFailureLeavesBothTablesUnchanged) {
  for (size_t skip : {0u, 1u}) {
    ProjectionFixture f;
    codet::test::journal_history(f.store, f.session, f.db, 65, "provisional");
    f.session.messages[2].text = "new";
    fault::candidate_read_error = true;
    fault::candidate_read_skip = skip;
    EXPECT_THROW(f.store.save(f.session), Error);
    const auto failed = fault::read_failed;
    fault::disable();
    EXPECT_TRUE(failed);
    EXPECT_EQ(f.store.load_id(f.session.id).messages[2].text, "old-0");
    EXPECT_EQ(f.value("SELECT result FROM tools WHERE call_id='call-0'"),
              Json(f.store.load_id(f.session.id).messages[2]).dump());
  }
}
TEST_F(PublicationFault, ProvisionalCandidateQueriesDoNotMaterializeResultPayloads) {
  for (const size_t count : {1u, 65u}) {
    ProjectionFixture f;
    codet::test::journal_history(f.store, f.session, f.db, count, "provisional");
    f.exec("UPDATE tools SET result=printf('%.*c',8388608,'x') WHERE call_id='call-0'");
    fault::largest_allocation = 0;
    fault::measure_allocations = true;
    EXPECT_NO_THROW(f.store.save(f.session));
    const auto largest = fault::largest_allocation;
    fault::disable();
    EXPECT_LT(largest, 131072u);
    EXPECT_EQ(f.store.load_id(f.session.id).messages[2].text, "old-0");
  }
}
TEST_F(PublicationFault, ProvisionalBatchPublicationRejectsStatusAndIdentityConflicts) {
  for (const auto *change : {"status='running'", "name='different'"}) {
    ProjectionFixture f;
    codet::test::journal_history(f.store, f.session, f.db, 65, "provisional");
    f.session.messages[2].text = "new";
    const auto trigger =
        std::string("CREATE TRIGGER change_later_candidate BEFORE UPDATE OF result ON tools ") +
        "WHEN OLD.call_id='call-0' BEGIN UPDATE tools SET " + change +
        " WHERE call_id='call-64'; END";
    f.exec(trigger.c_str());
    EXPECT_THROW(f.store.save(f.session), Error);
    fault::disable();
    const auto stored = f.value("SELECT payload FROM session_messages WHERE position=2");
    EXPECT_EQ(Json::parse(stored).at("text"), "old-0");
    EXPECT_EQ(f.value("SELECT result FROM tools WHERE call_id='call-0'"), stored);
  }
}
TEST_F(PublicationFault, ProvisionalBatchAcknowledgementDoesNotAllocateAfterCommit) {
  ProjectionFixture f;
  codet::test::journal_history(f.store, f.session, f.db, 65, "provisional");
  f.session.messages[2].text = "new";
  fault::after_commit = true;
  fault::fired = false;
  bool failed = false;
  try {
    f.store.save(f.session);
  } catch (...) {
    failed = true;
  }
  const auto allocated = fault::fired;
  fault::disable();
  EXPECT_FALSE(failed);
  EXPECT_FALSE(allocated);
  EXPECT_EQ(f.store.load_id(f.session.id).messages[2].text, "new");
}

std::string codet::test::executable;
namespace {
int check_default_sink() {
  fault::fired = false;
  fault::countdown = 0;
  const auto &sink = default_file_diagnostic_sink();
  const bool allocated = fault::fired;
  fault::disable();
  return allocated || !sink ? 3 : 0;
}
void report_from_late_static() {
  if (check_default_sink() != 0)
    std::_Exit(4);
  default_file_diagnostic_sink()("late static sink");
}
} // namespace
int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  codet::test::executable = fs::absolute(argv[0]).string();
  if (argc > 1 && std::string_view(argv[1]) == "--codet-file-sink-helper") {
    if (argc != 3 ||
        (std::string_view(argv[2]) != "first-use" && std::string_view(argv[2]) != "late-static")) {
      std::fputs("file-sink helper expects first-use or late-static\n", stderr);
      return 2;
    }
    if (std::string_view(argv[2]) == "late-static" && std::atexit(report_from_late_static) != 0)
      return 5;
    return check_default_sink();
  }
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
