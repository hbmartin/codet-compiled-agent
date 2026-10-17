#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "temp.hpp"
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
struct FixtureHttp : HttpTransport {
  int status = 400, requests = 0;
  std::string body, stream;
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &sink) override {
    ++requests;
    if (sink && status == 200)
      for (char c : stream)
        sink(std::string(1, c));
    return {status, body};
  }
};
struct FixtureClock : Clock {
  int waits = 0;
  int64_t seconds() const override {
    return 1000;
  }
  void sleep_ms(int, const Cancellation &c) override {
    c.check();
    ++waits;
  }
};
struct HookProcesses : Processes {
  ProcessResult result;
  ProcessResult run(const ProcessSpec &, const Cancellation &) override {
    return result;
  }
  std::string start(const ProcessSpec &) override {
    throw Error("Unexpected process");
  }
  ProcessResult poll(const std::string &) override {
    throw Error("Unexpected poll");
  }
  void stop(const std::string &) override {}
};
struct Modernization : Test {
  TempDirectory temp;
  Config config;
  NativeFileSystem files;
  FixtureClock clock;
  FixtureHttp http;
  HookProcesses processes;
  Cancellation cancel;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "workspace";
    config.state_root = temp.root / "invocation";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "gpt-6.1-sol";
    config.providers["openai"].api_key = "fixture";
  }
};
TEST_F(Modernization, RawProviderFailuresPreserveBodiesAndRetryRules) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  for (const auto &body :
       {std::string("{\"error\":{\"message\":\"raw\",\"detail\":42}}"),
        std::string("<html>failure\nsecond line</html>"), std::string("bad\xff\0", 5)}) {
    http.body = body;
    http.requests = 0;
    try {
      provider.complete(config, "", {}, Json::array(), cancel, {});
      FAIL();
    } catch (const ProviderError &e) {
      EXPECT_EQ(e.http_status, 400);
      EXPECT_EQ(e.body, body);
      const auto details = Json::parse(e.details().dump());
      if (safe_text(body) == body)
        EXPECT_EQ(details["body"], body);
      else
        EXPECT_TRUE(details.contains("body_base64"));
    }
    EXPECT_EQ(http.requests, 1);
  }
  http.status = 503;
  http.requests = 0;
  EXPECT_THROW(provider.complete(config, "", {}, Json::array(), cancel, {}), ProviderError);
  EXPECT_EQ(http.requests, 4);
  EXPECT_EQ(clock.waits, 3);
  http.status = 200;
  const auto frame =
      std::string("{\"type\":\"error\",\"error\":{\"message\":\"raw\",\"extra\":[1,2]}}");
  // Also exercise a final SSE frame without a terminating blank line.
  http.stream = "event: error\ndata: " + frame;
  try {
    provider.complete(config, "", {}, Json::array(), cancel, {});
    FAIL();
  } catch (const ProviderError &e) {
    EXPECT_EQ(e.body, frame);
  }
}
TEST_F(Modernization, PublicResponsesSharesReplayWithoutPrivateHeaders) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message user;
  user.role = "user";
  user.text = "inspect";
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"call", "read_file", {{"path", "x"}}}};
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "call";
  result.text = Json({{"content", "visible result"}, {"is_error", false}}).dump();
  const Json schemas = Json::array(
      {{{"name", "read_file"}, {"description", "read"}, {"input_schema", {{"type", "object"}}}}});
  const auto req = provider.request(config, "instructions", {user, assistant, result}, schemas);
  EXPECT_EQ(req.url, "https://api.openai.com/v1/responses");
  EXPECT_EQ(req.headers.count("originator"), 0u);
  EXPECT_EQ(req.headers.count("ChatGPT-Account-Id"), 0u);
  auto body = Json::parse(req.body);
  EXPECT_EQ(body["reasoning"]["effort"], "medium");
  EXPECT_THAT(body["input"].dump(), HasSubstr("function_call_output"));
  EXPECT_EQ(body["tools"][0]["type"], "function");
  auto imported = assistant;
  imported.replay_provider = "codex";
  imported.calls = {{"call", "apply_patch", {{"patch", "*** Begin Patch\n*** End Patch"}}}};
  imported.replay = Json::array({{{"type", "custom_tool_call"},
                                  {"call_id", "call"},
                                  {"name", "apply_patch"},
                                  {"input", "*** Begin Patch\n*** End Patch"}}});
  const auto migrated =
      Json::parse(provider.request(config, "", {user, imported, result}, schemas).body);
  EXPECT_THAT(migrated["input"].dump(), Not(HasSubstr("custom_tool_call")));
  Config subscription = config;
  subscription.provider = "codex";
  imported.replay_provider = "openai";
  const auto private_body = Json::parse(provider
                                            .request(subscription, "", {user, imported, result},
                                                     schemas, {{"access_token", "fixture"}})
                                            .body);
  EXPECT_THAT(private_body["input"].dump(), HasSubstr("custom_tool_call_output"));
  config.providers["openai"].base_url = "http://127.0.0.1:1/v1";
  EXPECT_FALSE(uses_responses(config));
  EXPECT_THROW(provider.request(config, "", {}, schemas), Error);
  config.providers["openai"].wire_api = "responses";
  http.status = 200;
  http.stream =
      "data: {\"type\":\"response.output_text.delta\",\"output_index\":0,\"delta\":\"visible\"}\n\n"
      "data: "
      "{\"type\":\"response.completed\",\"response\":{\"output\":[{\"type\":\"reasoning\","
      "\"encrypted_content\":\"opaque\"},{\"type\":\"message\",\"role\":\"assistant\",\"content\":["
      "{\"type\":\"output_text\",\"text\":\"visible\"}]},{\"type\":\"function_call\",\"call_id\":"
      "\"call2\",\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":\\\"x\\\"}\"}],\"usage\":{"
      "\"input_tokens\":8,\"output_tokens\":4}}}\n\n";
  const auto answer = provider.complete(config, "", {user}, schemas, cancel, {});
  EXPECT_EQ(answer.message.text, "visible");
  ASSERT_EQ(answer.message.calls.size(), 1u);
  EXPECT_EQ(answer.message.calls[0].name, "read_file");
  EXPECT_THAT(Json(answer.message.replay).dump(), HasSubstr("opaque"));
}
TEST_F(Modernization, SignedPrefixesPersistAndEpochChangesDiscardSignatures) {
  config.provider = "anthropic";
  config.model = "claude-sonnet-5-5";
  config.providers["anthropic"].api_key = "fixture";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message user;
  user.role = "user";
  user.text = "start";
  user.images = {
      persist_image(config, files, {std::string(225000, 'p'), "image/png", 1, 1}, "high")};
  http.status = 200;
  http.stream = "event: content_block_start\ndata: "
                "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":"
                "\"thinking\",\"thinking\":\"\",\"signature\":\"signed-empty\"}}\n\n"
                "event: content_block_start\ndata: "
                "{\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"tool_"
                "use\",\"id\":\"tool\",\"name\":\"read_file\",\"input\":{\"path\":\"x\"}}}\n\n"
                "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n";
  const auto answer =
      provider.complete(config, "first instructions", {user}, Json::array(), cancel, {});
  ASSERT_EQ(answer.message.calls.size(), 1u);
  auto state = provider.conversation_state();
  StateStore store(config.state_root);
  auto session = store.create();
  session.messages = {user, answer.message};
  session.provider_state = state;
  store.append_messages(session, 0, true);
  auto restored = store.load_id(session.id, false);
  // Avoid recovery here: the fixture deliberately stops before reserving a tool effect.
  provider.restore_conversation(restored.provider_state);
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "tool";
  result.text = "completed";
  auto history = restored.messages;
  history.push_back(result);
  auto body =
      Json::parse(provider.request(config, "changed instructions", history, Json::array()).body);
  EXPECT_EQ(body["system"][0]["text"], "first instructions");
  EXPECT_EQ(body["messages"][1]["content"][0]["signature"], "signed-empty");
  EXPECT_EQ(body["messages"][2]["content"][0]["type"], "tool_result");
  EXPECT_EQ(body["messages"].back()["role"], "system");
  EXPECT_EQ(body["output_config"]["effort"], "medium");
  EXPECT_THAT(body["messages"][0]["content"].dump(), HasSubstr("image/png"));
  config.providers["anthropic"].api_key = "another-account";
  body = Json::parse(provider.request(config, "new epoch", history, Json::array()).body);
  EXPECT_THAT(body.dump(), Not(HasSubstr("signed-empty")));
  EXPECT_THAT(body.dump(), HasSubstr("completed"));
  EXPECT_THROW(provider.request(config, "", {user, answer.message}, Json::array()), Error);
  provider.restore_conversation(state);
  config.model = "claude-sonnet-4-5";
  body = Json::parse(provider.request(config, "changed model", history, Json::array()).body);
  EXPECT_THAT(body.dump(), Not(HasSubstr("signed-empty")));
  EXPECT_THAT(body.dump(), HasSubstr("completed"));
  provider.restore_conversation(state);
  config.model = "claude-sonnet-5-5";
  config.providers["anthropic"].api_key = "fixture";
  config.capture_root = temp.root / "relocated-captures";
  body = Json::parse(provider.request(config, "moved references", history, Json::array()).body);
  EXPECT_THAT(body.dump(), Not(HasSubstr("signed-empty")));
}
TEST_F(Modernization, SemanticProjectionOmitsReplayAndKeepsRecoveryAndReferences) {
  codet::Message m;
  m.role = "assistant";
  m.text = "visible";
  m.message_id = "storage-only";
  m.replay =
      Json::array({{{"encrypted_content", "secret-opaque"}, {"signature", "secret-signature"}}});
  m.result_file = "results/session/message.json";
  m.calls = {{"call", "read_file", {{"path", "file"}}}};
  codet::Message tool;
  tool.role = "tool";
  tool.tool_call_id = "call";
  tool.text = Json({{"content", "outcome"},
                    {"is_error", true},
                    {"metadata", {{"action_completed", true}, {"recovery", "uncertain"}}}})
                  .dump();
  const auto projection = semantic_transcript({m, tool});
  for (const auto *s :
       {"visible", "outcome", "read_file", "uncertain", "results/session/message.json"})
    EXPECT_THAT(projection, HasSubstr(s));
  for (const auto *s : {"secret-opaque", "secret-signature", "storage-only"})
    EXPECT_THAT(projection, Not(HasSubstr(s)));
}
TEST_F(Modernization, HookFailuresStopButWarnNeverWeakensVetoes) {
  for (const auto *event : {"SessionStart", "UserPromptSubmit", "PreToolUse", "PermissionRequest",
                            "PostToolUse", "PreCompact", "Stop"}) {
    config.hooks[event] = Json::array({{{"command", "fixture"}}});
    Tools tools(config, files, processes, http, clock);
    processes.result = {1, ""};
    EXPECT_THROW(tools.run_hooks(event, Json::object(), cancel), HookFailure);
    config.hooks[event][0]["on_error"] = "warn";
    Tools warned(config, files, processes, http, clock);
    EXPECT_FALSE(warned.run_hooks(event, Json::object(), cancel).blocked);
    processes.result = {2, ""};
    auto veto = warned.run_hooks(event, Json::object(), cancel);
    EXPECT_TRUE(veto.blocked);
    EXPECT_FALSE(veto.reason.empty());
  }
  config.hooks = Json::object();
  config.hooks["PreToolUse"] = Json::array({{{"command", "fixture"}}});
  Tools tools(config, files, processes, http, clock);
  processes.result = {0, "{"};
  EXPECT_THROW(tools.run_hooks("PreToolUse", Json::object(), cancel), HookFailure);
  processes.result = {0, "{\"decision\":\"block\"}"};
  EXPECT_TRUE(tools.run_hooks("PreToolUse", Json::object(), cancel).blocked);
  processes.result.stdout_truncated = true;
  EXPECT_THROW(tools.run_hooks("PreToolUse", Json::object(), cancel), HookFailure);
  config.hooks["PreToolUse"][0]["on_error"] = "invalid";
  EXPECT_THROW(load_config(config.home, config.cwd, "", {"hooks=" + config.hooks.dump()}, files),
               Error);
}
TEST_F(Modernization, CorruptPlanResumesAndQuarantinesBeforeReplacement) {
  const auto path = config.state_root / "plans/session.json";
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "{broken", true);
  Tools tools(config, files, processes, http, clock);
  EXPECT_NO_THROW(tools.use_session("session"));
  EXPECT_EQ(files.read(path), "{broken");
  auto result =
      tools.execute({"plan",
                     "update_plan",
                     {{"plan", Json::array({{{"step", "repair"}, {"status", "pending"}}})}}},
                    cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_TRUE(Json::parse(files.read(path)).is_array());
  size_t recovered = 0;
  for (const auto &entry : fs::directory_iterator(path.parent_path()))
    if (entry.path() != path) {
      ++recovered;
      EXPECT_EQ(files.read(entry.path()), "{broken");
      struct stat st {};
      ASSERT_EQ(::stat(entry.path().c_str(), &st), 0);
      EXPECT_EQ(st.st_mode & 0777, 0600);
    }
  EXPECT_EQ(recovered, 1u);
  EXPECT_FALSE(fs::exists(config.home / "plans"));
}
TEST_F(Modernization, IncrementalWritesAndCompletionRollbackInBothDurabilityModes) {
  for (const auto *mode : {"durable", "fast"}) {
    const auto root = config.state_root / mode;
    StateStore store(root, {}, mode);
    auto session = store.create();
    codet::Message first;
    first.role = "user";
    first.text = std::string(100000, 'x');
    session.messages.push_back(first);
    store.append_messages(session, 0);
    const auto baseline = store.serialized_bytes();
    codet::Message next;
    next.role = "assistant";
    next.calls = {{"call", "read_file", {{"path", "x"}}}};
    session.messages.push_back(next);
    store.append_messages(session, 1);
    EXPECT_LT(store.serialized_bytes() - baseline, 2000u);
    store.tool_started(session.id, next.calls[0]);
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open((root / "state.sqlite3").c_str(), &db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(db,
                           "CREATE TRIGGER fail_result BEFORE INSERT ON session_messages WHEN "
                           "NEW.position=2 BEGIN SELECT RAISE(FAIL,'completion rollback'); END",
                           nullptr, nullptr, nullptr),
              SQLITE_OK);
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = "call";
    result.text = "done";
    EXPECT_THROW(store.complete_tool(session, next.calls[0], result), Error);
    EXPECT_EQ(session.messages.size(), 2u);
    ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER fail_result", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(db);
    store.complete_tool(session, next.calls[0], result);
    store.sync();
    auto loaded = store.load_id(session.id, false);
    ASSERT_EQ(loaded.messages.size(), 3u);
    EXPECT_EQ(loaded.messages.back().text, "done");
  }
}
TEST_F(Modernization, CaptureCatalogRebuildsAndFlushesWithoutWholeStoreScans) {
  const auto dir = config.state_root / "captures";
  {
    auto store = std::make_shared<CaptureStore>(dir);
    for (int i = 0; i < 30; ++i) {
      OutputSpool old(store);
      old.append("retained");
      old.close();
    }
    OutputSpool spool(store);
    spool.append(std::string(65536, 'x'));
    spool.flush();
    const auto scans = store->metrics.scans;
    for (int i = 0; i < 40; ++i) {
      spool.append(std::string(65536, 'y'));
      spool.flush();
    }
    EXPECT_EQ(store->metrics.scans, scans);
    spool.close();
  }
  files.atomic_write(dir / "catalog.sqlite3", "damaged catalog", true);
  auto rebuilt = std::make_shared<CaptureStore>(dir);
  OutputSpool spool(rebuilt);
  spool.append("after rebuild");
  spool.close();
  EXPECT_TRUE(spool.error().empty()) << spool.error();
  EXPECT_GE(rebuilt->metrics.scans, 1u);
}
TEST_F(Modernization, AbandonedCaptureReservationsAreReconciledUnderQuotaPressure) {
  const auto dir = config.state_root / "reservations";
  auto store = std::make_shared<CaptureStore>(dir, 128, 128, 64);
  {
    OutputSpool old(store);
    old.append("retained");
    old.close();
    ASSERT_TRUE(old.error().empty());
  }
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((dir / "catalog.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "UPDATE captures SET bytes=128,active=1", nullptr, nullptr, nullptr),
            SQLITE_OK);
  sqlite3_close(db);
  const auto scans = store->metrics.scans;
  OutputSpool next(store);
  next.append(std::string(64, 'x'));
  next.close();
  EXPECT_TRUE(next.error().empty()) << next.error();
  EXPECT_EQ(next.bytes(), 64u);
  EXPECT_GT(store->metrics.scans, scans);
}
TEST_F(Modernization, HookFailureJournalsTheAffectedCallBeforeCancellingRemainingCalls) {
  struct Batch : Provider {
    ProviderResult complete(const Config &, const std::string &,
                            const std::vector<codet::Message> &, const Json &, const Cancellation &,
                            const ChunkSink &, const ItemSink &) override {
      ProviderResult out;
      out.message.role = "assistant";
      for (const auto *id : {"first", "remaining"})
        out.message.calls.push_back(
            {id, "update_plan", {{"plan", Json::array({{{"step", id}, {"status", "pending"}}})}}});
      return out;
    }
  } provider;
  for (const auto *event : {"PreToolUse", "PostToolUse"}) {
    config.hooks = Json::object();
    config.hooks[event] = Json::array({{{"command", "fixture"}}});
    processes.result = {1, ""};
    StateStore store(config.state_root);
    auto session = store.create();
    Tools tools(config, files, processes, http, clock);
    Agent agent(config, provider, tools, store, {});
    EXPECT_THROW(agent.run(session, "run", cancel), HookFailure);
    auto recovered = store.load_id(session.id, true);
    ASSERT_GE(recovered.messages.size(), 4u);
    std::vector<Json> results;
    for (const auto &m : recovered.messages)
      if (m.role == "tool")
        results.push_back(Json::parse(m.text));
    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0]["metadata"]["hook_failure"].get<bool>());
    EXPECT_EQ(results[0]["metadata"]["action_completed"], std::string(event) == "PostToolUse");
    EXPECT_EQ(results[1]["metadata"]["recovery"], "not_started");
  }
}
TEST_F(Modernization, LegacyMigrationBacksUpAndPreservesMalformedBytesIndependently) {
  fs::create_directories(config.state_root / "session-locks");
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(
          db,
          "CREATE TABLE sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent TEXT,updated "
          "INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle',provider "
          "TEXT NOT NULL DEFAULT '',model TEXT NOT NULL DEFAULT '',reasoning_effort TEXT);"
          "CREATE TABLE tools(session TEXT,call_id TEXT,name TEXT,args TEXT,result TEXT,status "
          "TEXT,PRIMARY KEY(session,call_id));"
          "CREATE TABLE session_references(session TEXT,kind TEXT,path TEXT,PRIMARY "
          "KEY(session,kind,path));"
          "INSERT INTO sessions(id,name,updated,messages) "
          "VALUES('good','good',1,'[{\"role\":\"user\",\"text\":\"preserved\"}]'),('bad','bad',1,'["
          "{'); PRAGMA user_version=5;",
          nullptr, nullptr, nullptr),
      SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "INSERT INTO sessions(id,name,updated,messages) "
                         "VALUES('nul','nul',1,CAST(X'5b5d006f7061717565' AS TEXT))",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  sqlite3_close(db);
  const int fd =
      ::open((config.state_root / "session-locks/good.lock").c_str(), O_CREAT | O_RDWR, 0600);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(::flock(fd, LOCK_EX | LOCK_NB), 0);
  EXPECT_THROW(StateStore busy(config.state_root), Error);
  ::close(fd);
  StateStore store(config.state_root);
  EXPECT_EQ(store.load_id("good", false).messages.front().text, "preserved");
  EXPECT_THROW(store.load_id("bad", false), Error);
  EXPECT_THROW(store.load_id("nul", false), Error);
  size_t backups = 0;
  for (const auto &entry : fs::directory_iterator(config.state_root))
    if (entry.path().filename().string().rfind("state.sqlite3.pre-v6-", 0) == 0) {
      ++backups;
      struct stat st {};
      ASSERT_EQ(::stat(entry.path().c_str(), &st), 0);
      EXPECT_EQ(st.st_mode & 0777, 0600);
    }
  EXPECT_EQ(backups, 1u);
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &db), SQLITE_OK);
  sqlite3_stmt *q = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT messages,message_format FROM sessions WHERE id='bad'",
                               -1, &q, nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char *>(sqlite3_column_text(q, 0)), "[{");
  EXPECT_EQ(sqlite3_column_int(q, 1), 0);
  sqlite3_finalize(q);
  ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT messages,message_format FROM sessions WHERE id='nul'",
                               -1, &q, nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_bytes(q, 0), 9);
  EXPECT_EQ(sqlite3_column_int(q, 1), 0);
  sqlite3_finalize(q);
  sqlite3_close(db);
}
TEST_F(Modernization, AbruptExitNeverReexecutesIncompleteToolsInEitherMode) {
  for (const auto *mode : {"durable", "fast"})
    for (bool complete : {false, true}) {
      const auto root =
          config.state_root / (std::string(mode) + (complete ? "-complete" : "-intent"));
      std::string id;
      {
        StateStore initial(root, {}, mode);
        id = initial.create().id;
      }
      const auto child = ::fork();
      ASSERT_GE(child, 0);
      if (child == 0) {
        try {
          StateStore store(root, {}, mode);
          auto session = store.load_id(id, false);
          codet::Message assistant;
          assistant.role = "assistant";
          assistant.calls = {{"effect", "read_file", {{"path", "x"}}}};
          session.messages.push_back(assistant);
          store.append_messages(session, 0);
          store.tool_started(id, assistant.calls[0]);
          if (complete) {
            codet::Message result;
            result.role = "tool";
            result.tool_call_id = "effect";
            result.text = "completed once";
            store.complete_tool(session, assistant.calls[0], result);
          }
          ::_exit(0);
        } catch (...) {
          ::_exit(1);
        }
      }
      int status = 0;
      pid_t waited = 0;
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
      while ((waited = ::waitpid(child, &status, WNOHANG)) == 0 &&
             std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (waited == 0) {
        ::kill(child, SIGKILL);
        ::waitpid(child, &status, 0);
        FAIL() << "Crash fixture timed out";
      }
      ASSERT_EQ(waited, child);
      ASSERT_TRUE(WIFEXITED(status));
      ASSERT_EQ(WEXITSTATUS(status), 0);
      StateStore store(root, {}, mode);
      auto recovered = store.load_id(id, true);
      ASSERT_EQ(recovered.messages.size(), 2u);
      if (complete)
        EXPECT_EQ(recovered.messages.back().text, "completed once");
      else
        EXPECT_EQ(Json::parse(recovered.messages.back().text)["metadata"]["recovery"], "uncertain");
    }
}
} // namespace
