#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/prompt.hpp"
#include "codet/support.hpp"
#include "prompt_support.hpp"
#include "temp.hpp"
#include <fstream>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
Json wire_state(const std::string &text) {
  return {{"provider", "anthropic"},
          {"messages",
           Json::array({{{"source", "source"},
                         {"wire", Json::array({{{"role", "user"}, {"content", text}}})}}})}};
}
struct ProbeFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t archive_reads = 0;
  mutable std::function<void()> before_archive_read;
  std::string read(const fs::path &p, size_t n) const override {
    if (p.string().find("/results/") != std::string::npos) {
      ++archive_reads;
      if (before_archive_read) {
        auto f = std::move(before_archive_read);
        before_archive_read = {};
        f();
      }
    }
    return native.read(p, n);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    native.atomic_write(p, s, secret);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    return native.canonical(p);
  }
  std::unique_ptr<ReadHandle> open_read(const fs::path &p) const override {
    return native.open_read(p);
  }
};
struct ProbeHttp : HttpTransport {
  std::string stream;
  size_t requests = 0;
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &sink) override {
    ++requests;
    if (sink)
      sink(stream);
    return {200, stream};
  }
};
struct ProbeProvider : Provider {
  Json state;
  std::vector<codet::Message> observed;
  std::function<void()> before_return;
  bool fail_snapshot = false, calls = false;
  void restore_conversation(const Json &s) override {
    state = s;
  }
  Json conversation_state() const override {
    if (fail_snapshot)
      return Json::parse("{broken");
    return state;
  }
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &m,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink &) override {
    observed = m;
    if (before_return)
      before_return();
    ProviderResult r;
    r.message.role = "assistant";
    r.message.text = "received response";
    if (calls)
      r.message.calls = {{"never-dispatch", "exec_command", {{"cmd", "touch unexpected"}}}};
    return r;
  }
};
struct Pr17 : Test {
  TempDirectory temp;
  Config config;
  ProbeFiles files;
  NativeProcesses processes;
  ProbeHttp http;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "work";
    config.state_root = temp.root / "state";
    config.roots = {config.cwd};
    config.approval = "never";
    config.provider = "openai";
    config.model = "gpt-5.5";
    config.providers["openai"].api_key = "fixture";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
  }
  struct Db {
    sqlite3 *p = nullptr;
    explicit Db(const fs::path &path) {
      if (sqlite3_open(path.c_str(), &p) != SQLITE_OK)
        throw Error("open probe database");
    }
    ~Db() {
      sqlite3_close(p);
    }
    void exec(const std::string &s) {
      if (sqlite3_exec(p, s.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        throw Error(sqlite3_errmsg(p));
    }
    std::string value(const std::string &s) {
      sqlite3_stmt *q = nullptr;
      if (sqlite3_prepare_v2(p, s.c_str(), -1, &q, nullptr) != SQLITE_OK)
        throw Error(sqlite3_errmsg(p));
      std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> holder(q, sqlite3_finalize);
      if (sqlite3_step(q) != SQLITE_ROW)
        throw Error("no probe row");
      const auto *v = sqlite3_column_text(q, 0);
      return v ? reinterpret_cast<const char *>(v) : "";
    }
  };
  codet::Message recovery_record(Tools &tools, std::string detail = "") {
    codet::Message record;
    record.role = "system";
    record.origin = "recovery";
    record.text =
        Json::array({{{"call_id", unique_id()}, {"recovery", "uncertain"}, {"detail", detail}}})
            .dump();
    record.message_id = digest(record.text);
    tools.retain_result(record);
    codet::Message checkpoint;
    checkpoint.role = "system";
    checkpoint.origin = "compaction";
    checkpoint.text = "Previous summary";
    checkpoint.checkpoint_state = {{"records", 1}, {"recovery_record", record.result_file}};
    return checkpoint;
  }
};
TEST_F(Pr17, FailedResponseRestoresBothSnapshotsBeforeTheNextSave) {
  for (const auto *mode : {"sql", "json", "snapshot", "deferred"}) {
    SCOPED_TRACE(mode);
    config.state_root = temp.root / mode;
    StateStore state(config.state_root);
    auto session = state.create();
    session.provider_state = wire_state("committed");
    state.save(session);
    Db raw(config.state_root / "state.sqlite3");
    const std::string kind = mode;
    if (kind == "sql")
      raw.exec("CREATE TRIGGER refuse BEFORE INSERT ON session_messages WHEN "
               "json_extract(NEW.payload,'$.role')='assistant' BEGIN SELECT RAISE(FAIL,'save "
               "failed'); END");
    if (kind == "deferred")
      raw.exec(
          "CREATE TABLE parent(id INTEGER PRIMARY KEY);CREATE TABLE poison(id INTEGER REFERENCES "
          "parent(id) DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER refuse AFTER INSERT ON "
          "provider_parts WHEN NEW.position=1 BEGIN INSERT INTO poison VALUES(1); END");
    ProbeProvider provider;
    provider.calls = true;
    provider.fail_snapshot = kind == "snapshot";
    provider.before_return = [&] {
      provider.state = wire_state(kind == "json" ? std::string(1, char(255)) : "uncommitted");
    };
    Tools tools(config, files, processes, http, clock);
    Agent agent(config, provider, tools, state, {});
    try {
      agent.run(session, "continue", cancel);
      FAIL();
    } catch (const Error &e) {
      EXPECT_THAT(e.what(), HasSubstr("uncommitted response preserved at"));
    }
    EXPECT_EQ(session.provider_state, wire_state("committed"));
    EXPECT_EQ(provider.state, session.provider_state);
    ASSERT_EQ(session.messages.size(), 1u);
    EXPECT_EQ(session.messages.back().role, "user");
    EXPECT_FALSE(fs::exists(config.cwd / "unexpected"));
    if (kind == "sql" || kind == "deferred")
      raw.exec("DROP TRIGGER refuse");
    EXPECT_NO_THROW(state.save(session)); // Verify before a reload can mask the broken snapshot.
    EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
    size_t receipts = 0;
    for (const auto &entry : fs::directory_iterator(config.state_root / "repairs")) {
      if (entry.path().string().find("-response-") == std::string::npos)
        continue;
      ++receipts;
      auto receipt = Json::parse(files.native.read(entry.path()));
      EXPECT_EQ(receipt["dispatch_allowed"], false);
      EXPECT_EQ(receipt["message"]["calls"].size(), 1u);
    }
    EXPECT_EQ(receipts, 1u);
  }
}
TEST_F(Pr17, InterleavedLargeSessionsKeepTheirCommittedPrefixTickets) {
  StateStore left(config.state_root), right(config.state_root);
  auto a = left.create(), b = right.create();
  auto lease_a = left.acquire(a.id), lease_b = right.acquire(b.id);
  a.provider_state = wire_state(std::string(9 * 1024 * 1024, 'a'));
  b.provider_state = wire_state(std::string(9 * 1024 * 1024, 'b'));
  left.save_provider_state(a);
  right.save_provider_state(b);
  const auto reads_a = left.provider_rows_read(), reads_b = right.provider_rows_read();
  const auto bytes_a = left.serialized_bytes(), bytes_b = right.serialized_bytes();
  for (int i = 0; i < 8; ++i) {
    const std::pair<StateStore *, Session *> stores[] = {{&left, &a}, {&right, &b}};
    for (auto pair : stores) {
      auto &store = *pair.first;
      auto &s = *pair.second;
      ProviderStateUpdate hint{store.provider_revision(s.id), s.provider_state["messages"].size()};
      ASSERT_NE(hint.revision, 0u);
      s.provider_state["messages"].push_back(wire_state("suffix")["messages"][0]);
      store.append_messages(s, 0, true, hint);
    }
  }
  EXPECT_EQ(left.provider_rows_read(), reads_a);
  EXPECT_EQ(right.provider_rows_read(), reads_b);
  EXPECT_LT(left.serialized_bytes() - bytes_a, 4096u);
  EXPECT_LT(right.serialized_bytes() - bytes_b, 4096u);
  EXPECT_EQ(left.provider_cache_bytes(), 0u);
  EXPECT_EQ(right.provider_cache_bytes(), 0u);
  EXPECT_EQ(left.load_id(a.id, false).provider_state, a.provider_state);
  EXPECT_EQ(right.load_id(b.id, false).provider_state, b.provider_state);
}
TEST_F(Pr17, ExternalChangesAndMovedRowsRotateTheAffectedSessionTokens) {
  StateStore state(config.state_root);
  auto a = state.create(), b = state.create();
  a.provider_state = wire_state("a");
  b.provider_state = wire_state("b");
  state.save(a);
  state.save(b);
  Db raw(config.state_root / "state.sqlite3");
  const auto token = [&](const Session &s) {
    return raw.value("SELECT revision FROM provider_revisions WHERE session='" + s.id + "'");
  };
  for (const auto *operation :
       {"UPDATE provider_parts SET payload=payload",
        "INSERT OR REPLACE INTO provider_parts SELECT session,position,payload FROM provider_parts",
        "DELETE FROM provider_parts"}) {
    state.save(a);
    a = state.load_id(a.id, false);
    auto before = token(a);
    auto other = token(b);
    raw.exec(std::string(operation) + " WHERE session='" + a.id + "' AND position=1");
    EXPECT_NE(token(a), before);
    EXPECT_EQ(token(b), other);
    EXPECT_EQ(state.provider_revision(a.id), 0u);
  }
  state.save(a);
  b.provider_state["messages"] = Json::array();
  state.save(b);
  auto ta = token(a), tb = token(b);
  raw.exec("UPDATE provider_parts SET session='" + b.id + "' WHERE session='" + a.id +
           "' AND position=1");
  EXPECT_NE(token(a), ta);
  EXPECT_NE(token(b), tb);
}
TEST_F(Pr17, UnexpectedTailAndAlteredRevisionInfrastructureFailClosed) {
  StateStore state(config.state_root);
  auto s = state.create();
  s.provider_state = wire_state("old");
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("CREATE TRIGGER extra AFTER UPDATE ON sessions WHEN NEW.id='" + s.id +
           "' BEGIN INSERT OR IGNORE INTO provider_parts VALUES(NEW.id,99,'{broken'); END");
  s = state.load_id(s.id, false);
  ProviderStateUpdate hint{state.provider_revision(s.id), 1};
  s.provider_state["messages"].push_back(wire_state("new")["messages"][0]);
  EXPECT_THROW(state.append_messages(s, 0, true, hint), Error);
  raw.exec("DROP TRIGGER extra");
  EXPECT_EQ(state.load_id(s.id, false).provider_state, wire_state("old"));
  raw.exec("DROP TRIGGER provider_revision_update");
  EXPECT_THROW(state.provider_revision(s.id), Error);
  EXPECT_THROW(StateStore broken(config.state_root), Error);
}
TEST_F(Pr17, TailIntroducedDuringSuffixWritesRollsBackBeforePublication) {
  StateStore state(config.state_root);
  auto s = state.create();
  s.provider_state = wire_state("old");
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("CREATE TRIGGER suffix_tail AFTER INSERT ON provider_parts WHEN NEW.position=2 BEGIN "
           "INSERT INTO provider_parts VALUES(NEW.session,99,'{broken'); END");
  s = state.load_id(s.id, false);
  ProviderStateUpdate hint{state.provider_revision(s.id), 1};
  s.provider_state["messages"].push_back(wire_state("new")["messages"][0]);
  EXPECT_THROW(state.append_messages(s, 0, true, hint), Error);
  raw.exec("DROP TRIGGER suffix_tail");
  EXPECT_NO_THROW(state.save_provider_state(s));
  EXPECT_EQ(state.load_id(s.id, false).provider_state, s.provider_state);
}
TEST_F(Pr17, SchemaSixUpgradeBacksUpAndFailureRollsBackItsSchemaChanges) {
  std::string id;
  {
    StateStore state(config.state_root);
    auto s = state.create();
    id = s.id;
    s.provider_state = wire_state("original");
    state.save(s);
  }
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("DROP TRIGGER provider_revision_insert;DROP TRIGGER provider_revision_update;DROP "
           "TRIGGER provider_revision_delete;DROP TABLE provider_revisions;PRAGMA "
           "user_version=6;CREATE INDEX provider_revision_insert ON provider_parts(position)");
  EXPECT_THROW(StateStore failed(config.state_root), Error);
  EXPECT_EQ(raw.value("PRAGMA user_version"), "6");
  EXPECT_EQ(raw.value("SELECT count(*) FROM sqlite_master WHERE name='provider_revision_delete'"),
            "0");
  raw.exec("DROP INDEX provider_revision_insert");
  StateStore upgraded(config.state_root);
  EXPECT_EQ(raw.value("PRAGMA user_version"), "8");
  EXPECT_EQ(upgraded.load_id(id, false).provider_state, wire_state("original"));
  size_t backups = 0;
  for (const auto &entry : fs::directory_iterator(config.state_root))
    if (entry.path().filename().string().find(".pre-v8-") != std::string::npos) {
      ++backups;
      struct stat st {};
      ASSERT_EQ(::stat(entry.path().c_str(), &st), 0);
      EXPECT_EQ(st.st_mode & 0777, 0600);
    }
  EXPECT_EQ(backups, 2u);
}
TEST_F(Pr17, SchemaSixUpgradeRequiresActiveSessionsToStop) {
  std::shared_ptr<void> lease;
  {
    StateStore state(config.state_root);
    auto session = state.create();
    lease = state.acquire(session.id);
  }
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("DROP TRIGGER provider_revision_insert;DROP TRIGGER provider_revision_update;DROP "
           "TRIGGER provider_revision_delete;DROP TABLE provider_revisions;PRAGMA user_version=6");
  try {
    StateStore blocked(config.state_root);
    FAIL();
  } catch (const Error &error) {
    EXPECT_THAT(error.what(), HasSubstr("Stop active Codet sessions"));
  }
  EXPECT_EQ(raw.value("PRAGMA user_version"), "6");
  lease.reset();
  EXPECT_NO_THROW(StateStore upgraded(config.state_root));
  EXPECT_EQ(raw.value("PRAGMA user_version"), "8");
}
TEST_F(Pr17, CodexQualificationIsStableAcrossRuntimeAndLaterTurns) {
  config.provider = "codex";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message call, result;
  call.role = "assistant";
  call.calls = {{"issue", "mcp__github__create_issue", Json::object()}};
  result.role = "tool";
  result.tool_call_id = "issue";
  result.text = "done";
  for (bool replay : {false, true})
    for (bool runtime : {false, true}) {
      SCOPED_TRACE(std::to_string(replay) + "/" + std::to_string(runtime));
      if (replay)
        call.replay = Json::array({{{"type", "function_call"},
                                    {"call_id", "issue"},
                                    {"namespace", "mcp"},
                                    {"name", "github__create_issue"},
                                    {"arguments", "{}"}}});
      std::vector<codet::Message> messages{call, result};
      if (runtime)
        messages.push_back(runtime_message("facts"));
      for (int i = 0; i < 3; ++i) {
        codet::Message next;
        next.role = "user";
        next.text = "next";
        messages.push_back(next);
        auto body = Json::parse(
            provider.request(config, "system", messages, Json::array(), {{"access_token", "fake"}})
                .body);
        size_t found = 0;
        for (const auto &item : body["input"])
          if (item.value("type", "") == "function_call") {
            ++found;
            EXPECT_EQ(item["namespace"], "mcp");
            EXPECT_EQ(item["name"], "github__create_issue");
          }
        EXPECT_EQ(found, 1u);
      }
    }
}
TEST_F(Pr17, MalformedStreamFieldsAreTypedObservedErrorsAndConsumerFailuresRemainDistinct) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  for (const auto &frame :
       {Json({{"type", "response.output_text.delta"}, {"delta", 42}}),
        Json({{"type", "response.completed"}, {"response", {{"output", 42}}}})}) {
    http.requests = 0;
    http.stream = "data: " + frame.dump() + "\n\n";
    try {
      provider.complete(config, "", {}, Json::array(), cancel, {});
      FAIL();
    } catch (const ProviderError &e) {
      EXPECT_EQ(e.body, frame.dump());
      EXPECT_THAT(e.what(), HasSubstr("Malformed provider stream"));
    }
    EXPECT_EQ(http.requests, 1u);
  }
  http.stream = "data: {\"type\":\"response.output_text.delta\",\"delta\":\"text\"}\n\n";
  EXPECT_THROW(provider.complete(config, "", {}, Json::array(), cancel,
                                 [](const auto &) { throw std::logic_error("consumer failed"); }),
               std::logic_error);
}
TEST_F(Pr17, TransportWrappingCannotReplaceAnOutputConsumerFailure) {
  struct WrappingTransport : HttpTransport {
    ProbeHttp &inner;
    explicit WrappingTransport(ProbeHttp &source) : inner(source) {}
    HttpResponse perform(const HttpRequest &request, const Cancellation &cancel,
                         const ChunkSink &sink) override {
      try {
        return inner.perform(request, cancel, sink);
      } catch (const Error &) {
        throw Error("transport wrapper");
      }
    }
  } transport(http);
  http.stream = "data: {\"type\":\"response.output_text.delta\",\"delta\":\"text\"}\n\n";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(transport, credentials, clock);
  const auto original = std::make_exception_ptr(Error("stdout failed"));
  try {
    provider.complete(config, "", {}, Json::array(), cancel,
                      [&](const auto &) { std::rethrow_exception(original); });
    FAIL();
  } catch (...) {
    EXPECT_EQ(std::current_exception(), original);
  }
  EXPECT_EQ(http.requests, 1u);
}
TEST_F(Pr17, RetiredFactsInEitherLegacySystemFieldProduceAnExplicitRuntimeUpdate) {
  for (const auto *key : {"system", "latest_system"}) {
    StateStore state(config.state_root);
    auto s = state.create();
    s.provider_state = wire_state("old");
    s.provider_state[key] = "# Current execution and recovery facts\nstale handle";
    state.save(s);
    Tools tools(config, files, processes, http, clock);
    ProbeProvider provider;
    Agent agent(config, provider, tools, state, {});
    agent.run(s, "continue", cancel);
    size_t count = 0;
    for (const auto &m : provider.observed)
      if (m.origin == "runtime") {
        ++count;
        EXPECT_THAT(m.text, HasSubstr("[]"));
      }
    EXPECT_EQ(count, 1u);
  }
}
TEST_F(Pr17, RecoveryArchivesAreCachedRevalidatedAndFailuresAreNotSticky) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("cache-test");
  auto checkpoint = recovery_record(tools);
  for (int i = 0; i < 5; ++i)
    ASSERT_EQ(tools.execution_state({checkpoint, checkpoint}).size(), 1u);
  EXPECT_EQ(files.archive_reads, 1u);
  auto path = artifact_directory(config) /
              checkpoint.checkpoint_state["recovery_record"].get<std::string>();
  auto bytes = files.native.read(path);
  files.native.atomic_write(path, "{broken", true);
  EXPECT_EQ(tools.execution_state({checkpoint}).back()["kind"], "recovery_unavailable");
  auto reads = files.archive_reads;
  tools.execution_state({checkpoint});
  EXPECT_EQ(files.archive_reads, reads + 1);
  files.native.atomic_write(path, bytes, true);
  EXPECT_EQ(tools.execution_state({checkpoint}).size(), 1u);
  EXPECT_EQ(files.archive_reads, reads + 2);
  fs::remove(path);
  EXPECT_EQ(tools.execution_state({checkpoint}).back()["kind"], "recovery_unavailable");
  files.native.atomic_write(path, bytes, true);
  EXPECT_EQ(tools.execution_state({checkpoint}).size(), 1u);
}
TEST_F(Pr17, RecoveryArchiveCacheEvictsWithinEightMiB) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("bounded-cache");
  auto a = recovery_record(tools, std::string(5 * 1024 * 1024, 'a')),
       b = recovery_record(tools, std::string(5 * 1024 * 1024, 'b'));
  tools.execution_state({a});
  tools.execution_state({a});
  EXPECT_EQ(files.archive_reads, 1u);
  tools.execution_state({b});
  EXPECT_EQ(files.archive_reads, 2u);
  tools.execution_state({a});
  EXPECT_EQ(files.archive_reads, 3u);
  Tools segment(config, files, processes, http, clock);
  const auto before = files.archive_reads;
  EXPECT_EQ(segment.execution_state({a, b, a}).size(), 2u);
  EXPECT_EQ(files.archive_reads - before,
            2u); // Segment references survive reusable-cache eviction.
}
TEST_F(Pr17, CompactedTurnReadsTheUnchangedHistoricalArchiveOnce) {
  StateStore state(config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  auto checkpoint = recovery_record(tools);
  codet::Message old, user;
  old.role = "assistant";
  old.text = std::string(20000, 'x');
  user.role = "user";
  user.origin = "human";
  user.text = "continue";
  s.messages = {checkpoint, old};
  state.save(s);
  config.context_tokens =
      test::context_tokens(tools, config, tools.history_bytes({checkpoint, old, user}) + 4096);
  ProbeProvider provider;
  Agent agent(config, provider, tools, state, {});
  files.archive_reads = 0;
  EXPECT_NO_THROW(agent.run(s, "continue", cancel));
  EXPECT_EQ(files.archive_reads, 1u);
}
TEST_F(Pr17, RepairQuarantinesUnavailableReferencesWithoutChangingTheJournal) {
  for (const auto *kind : {"missing", "outside", "symlink", "unreadable", "available"}) {
    SCOPED_TRACE(kind);
    config.state_root = temp.root / kind;
    StateStore state(config.state_root);
    auto s = state.create();
    ToolCall call{"effect", "exec_command", {{"cmd", "historical only"}}};
    codet::Message assistant, result;
    assistant.role = "assistant";
    assistant.calls = {call};
    result.role = "tool";
    result.tool_call_id = call.id;
    result.text = "{\"content\":\"surviving preview\",\"is_error\":false,\"metadata\":{}}";
    const auto ref = fs::path("results") / s.id / (unique_id() + ".json");
    const auto path = config.state_root / "artifacts" / ref;
    fs::create_directories(path.parent_path());
    const std::string mode = kind;
    if (mode == "available" || mode == "unreadable")
      files.native.atomic_write(path, "original", true);
    if (mode == "unreadable") {
      ASSERT_EQ(::chmod(path.c_str(), 0), 0);
    }
    if (mode == "symlink")
      fs::create_symlink(config.cwd, path);
    result.result_file = mode == "outside" ? (temp.root / "outside" / ref).string() : path.string();
    state.tool_started(s.id, call);
    state.tool_finished(s.id, call, Json(result));
    s.messages = {assistant, result};
    state.save(s);
    Db raw(config.state_root / "state.sqlite3");
    const auto journal = raw.value("SELECT result FROM tools WHERE session='" + s.id + "'");
    Tools tools(config, files, processes, http, clock);
    const auto before_inspection = raw.value(
        "SELECT payload FROM session_messages WHERE session='" + s.id + "' AND position=1");
    auto inspection = state.repair(s.id, false, false, false, tools);
    EXPECT_FALSE(inspection["changed"]);
    EXPECT_EQ(raw.value("SELECT payload FROM session_messages WHERE session='" + s.id +
                        "' AND position=1"),
              before_inspection);
    if (mode == "missing" || mode == "outside" || mode == "symlink") {
      EXPECT_FALSE(inspection["issues"].empty());
    }
    auto report = state.repair(s.id, false, false, true, tools);
    EXPECT_TRUE(report["changed"]);
    EXPECT_EQ(raw.value("SELECT result FROM tools WHERE session='" + s.id + "'"), journal);
    auto saved = state.load_id(s.id, false);
    ASSERT_TRUE(result_name(saved.messages[1].result_file));
    const bool unavailable = mode != "available" && (mode != "unreadable" || ::geteuid() != 0);
    if (unavailable) {
      EXPECT_NE(saved.messages[1].result_file, ref.string());
      EXPECT_THAT(
          files.native.read(config.state_root / "artifacts" / saved.messages[1].result_file),
          HasSubstr("Partial surviving observation"));
      EXPECT_THAT(tools.execution_state(saved.messages).dump(), HasSubstr("original_reference"));
      state.reconcile(saved);
      EXPECT_THAT(tools.execution_state(saved.messages).dump(), HasSubstr("original_reference"));
      auto child = state.fork(saved);
      EXPECT_THAT(tools.execution_state(child.messages).dump(), HasSubstr("original_reference"));
    }
    EXPECT_NO_THROW(state.repair(s.id, false, false, true, tools));
    EXPECT_FALSE(fs::exists(config.cwd / "historical only"));
    if (mode == "unreadable")
      ::chmod(path.c_str(), 0600);
  }
}
TEST_F(Pr17, RepairFailureNamesItsBackupAndRemovesOnlyNewPartialArchives) {
  StateStore state(config.state_root);
  std::string diagnostics;
  state.set_diagnostics([&](const auto &text) { diagnostics += text; });
  auto s = state.create();
  codet::Message message;
  message.role = "assistant";
  message.text = "partial";
  message.result_file = "/missing/results/old/result.json";
  s.messages = {message};
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("CREATE TRIGGER refuse BEFORE DELETE ON session_messages BEGIN SELECT "
           "RAISE(FAIL,'publication failed'); END");
  Tools tools(config, files, processes, http, clock);
  try {
    state.repair(s.id, false, false, true, tools);
    FAIL();
  } catch (const Error &e) {
    EXPECT_STREQ(e.what(), "publication failed");
    EXPECT_THAT(diagnostics, HasSubstr("Session repair backup:"));
  }
  EXPECT_EQ(state.load_id(s.id, false).messages[0].result_file, message.result_file);
  const auto results = config.state_root / "artifacts/results";
  size_t count = 0;
  if (fs::exists(results))
    for (const auto &entry : fs::recursive_directory_iterator(results))
      if (entry.is_regular_file())
        ++count;
  EXPECT_EQ(count, 0u);
}
TEST_F(Pr17, RepairQueuesConcurrentWritersWithoutKeepingALookupSnapshot) {
  StateStore state(config.state_root);
  auto s = state.create(), other = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  s.messages = {recovery_record(tools)};
  state.save(s);
  std::thread writer;
  std::exception_ptr error;
  std::promise<void> started;
  files.before_archive_read = [&] {
    writer = std::thread([&] {
      try {
        Db db(config.state_root / "state.sqlite3");
        db.exec("PRAGMA busy_timeout=5000");
        started.set_value();
        db.exec("UPDATE sessions SET status='other committed' WHERE id='" + other.id + "'");
      } catch (...) {
        error = std::current_exception();
      }
    });
    started.get_future().wait();
  };
  EXPECT_NO_THROW(state.repair(s.id, false, false, true, tools));
  ASSERT_TRUE(writer.joinable());
  writer.join();
  EXPECT_FALSE(error);
  Db raw(config.state_root / "state.sqlite3");
  EXPECT_EQ(raw.value("SELECT status FROM sessions WHERE id='" + other.id + "'"),
            "other committed");
}
TEST_F(Pr17, RepairedUnknownsSurviveRelocationAndSubsequentCompaction) {
  StateStore state(config.state_root);
  auto s = state.create();
  codet::Message message;
  message.role = "assistant";
  message.text = "surviving data";
  message.result_file = "/lost/results/old/result.json";
  s.messages = {message};
  state.save(s);
  Tools tools(config, files, processes, http, clock);
  state.repair(s.id, false, false, true, tools);
  s = state.load_id(s.id, false);
  state.reconcile(s);
  auto child = state.fork(s);
  const auto relocated = temp.root / "relocated";
  fs::copy(config.state_root, relocated, fs::copy_options::recursive);
  config.state_root = relocated;
  StateStore moved(relocated);
  Tools resumed(config, files, processes, http, clock);
  ProbeProvider provider;
  Agent agent(config, provider, resumed, moved, {});
  EXPECT_NO_THROW(agent.compact(child, cancel));
  EXPECT_THAT(resumed.execution_state(moved.load_id(child.id, false).messages).dump(),
              HasSubstr("/lost/results/old/result.json"));
  EXPECT_NO_THROW(agent.compact(child, cancel));
}
TEST_F(Pr17, RuntimeBudgetReservesTheExactEnvelopeForEveryAdapter) {
  for (const auto *provider : {"openai", "codex", "anthropic", "google", "ollama"}) {
    config.provider = provider;
    auto base = prompt_budget(config, "system", Json::array());
    auto budget = request_budget(config, "system", Json::array(), "facts");
    EXPECT_EQ(budget.fixed - base.fixed,
              provider_history_bytes(config, {runtime_message("facts")}));
    EXPECT_EQ(request_budget(config, "system", Json::array(), "").fixed, base.fixed);
  }
}
} // namespace
