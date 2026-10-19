#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "process_helper.hpp"
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
  for (const auto &raw :
       {std::string("<html>stream failure</html>"), std::string("bad\xff\0", 5), std::string{}}) {
    http.requests = 0;
    http.stream = "event: error\ndata: " + raw + "\n\n";
    try {
      provider.complete(config, "", {}, Json::array(), cancel, {});
      FAIL();
    } catch (const ProviderError &e) {
      EXPECT_EQ(e.body, raw);
      EXPECT_EQ(e.http_status, 200);
    }
    EXPECT_EQ(http.requests, 1);
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
TEST_F(Modernization, PublicResponsesReasoningDefaultsAreModelAware) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  for (const auto *model : {"gpt-6.1-sol", "gpt-6-astra", "gpt-4o-mini", "unknown-model"}) {
    config.model = model;
    for (const auto *effort : {"", "high"}) {
      config.effort = effort;
      const auto body = Json::parse(provider.request(config, "hello", {}, Json::array()).body);
      const bool expected =
          !config.effort.empty() || config.model == "gpt-6.1-sol" || config.model == "gpt-6-astra";
      EXPECT_EQ(body.contains("reasoning"), expected) << model << ":" << effort;
      EXPECT_EQ(body["include"], Json::array({"reasoning.encrypted_content"}));
      if (expected) {
        EXPECT_EQ(body["reasoning"]["effort"], config.effort.empty() ? "medium" : effort);
        EXPECT_EQ(body["include"], Json::array({"reasoning.encrypted_content"}));
      }
    }
  }
  config.provider = "codex";
  config.effort.clear();
  auto body = Json::parse(
      provider.request(config, "hello", {}, Json::array(), {{"access_token", "fixture"}}).body);
  EXPECT_FALSE(body.contains("reasoning"));
  EXPECT_TRUE(body.contains("include"));
  config.effort = "high";
  body = Json::parse(
      provider.request(config, "hello", {}, Json::array(), {{"access_token", "fixture"}}).body);
  EXPECT_EQ(body["reasoning"]["effort"], "high");
  EXPECT_TRUE(body.contains("include"));
  config.provider = "openai";
  config.providers["openai"].base_url = "http://127.0.0.1:1/v1";
  const auto custom = provider.request(config, "hello", {}, Json::array());
  EXPECT_THAT(custom.url, HasSubstr("/chat/completions"));
  body = Json::parse(custom.body);
  EXPECT_FALSE(body.contains("reasoning"));
  EXPECT_FALSE(body.contains("include"));
  EXPECT_EQ(body["reasoning_effort"], "high");
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
  provider.restore_conversation(state);
  config.capture_root.clear();
  auto with_new_images = history;
  codet::Message new_images;
  new_images.role = "user";
  new_images.text = "more images";
  new_images.images = std::vector<ImageArtifact>(4, user.images.front());
  with_new_images.push_back(new_images);
  body = Json::parse(provider.request(config, "more images", with_new_images, Json::array()).body);
  EXPECT_THAT(body.dump(), Not(HasSubstr("signed-empty")));
  size_t delivered = 0;
  for (const auto &message : body["messages"])
    for (const auto &block : message["content"])
      if (block.value("type", Json()) == "image")
        ++delivered;
  EXPECT_EQ(delivered, 4u);
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
TEST_F(Modernization, HookStopsSurviveVetoesInEitherOrder) {
  struct Sequence : HookProcesses {
    std::vector<std::string> outputs;
    size_t index = 0;
    ProcessResult run(const ProcessSpec &, const Cancellation &) override {
      return {0, outputs.at(index++)};
    }
  } sequence;
  config.hooks["PostToolUse"] = Json::array({{{"command", "first"}}, {{"command", "second"}}});
  for (bool stop_first : {true, false}) {
    sequence.outputs = {"{\"continue\":false}", "{\"decision\":\"block\"}"};
    if (!stop_first)
      std::reverse(sequence.outputs.begin(), sequence.outputs.end());
    sequence.index = 0;
    Tools tools(config, files, sequence, http, clock);
    const auto result = tools.run_hooks("PostToolUse", Json::object(), cancel);
    EXPECT_TRUE(result.blocked);
    EXPECT_TRUE(result.stopped);
  }
}
TEST_F(Modernization, PostToolBlockingFeedbackAllowsTheModelToContinue) {
  struct FeedbackProvider : Provider {
    int requests = 0;
    ProviderResult complete(const Config &, const std::string &,
                            const std::vector<codet::Message> &, const Json &, const Cancellation &,
                            const ChunkSink &, const ItemSink &) override {
      ProviderResult out;
      out.message.role = "assistant";
      if (requests++ == 0) {
        for (const auto *id : {"first", "second"})
          out.message.calls.push_back(
              {id,
               "update_plan",
               {{"plan", Json::array({{{"step", id}, {"status", "pending"}}})}}});
      } else
        out.message.text = "Handled formatting feedback";
      return out;
    }
  } provider;
  config.hooks["PostToolUse"] = Json::array({{{"command", "fixture"}}});
  processes.result = {0, "{\"decision\":\"block\",\"reason\":\"fix formatting\"}"};
  StateStore store(config.state_root);
  auto session = store.create();
  Tools tools(config, files, processes, http, clock);
  Agent agent(config, provider, tools, store, {});
  EXPECT_NO_THROW(agent.run(session, "run", cancel));
  EXPECT_EQ(provider.requests, 2);
  const auto saved = store.load_id(session.id, false);
  int outcomes = 0;
  for (const auto &message : saved.messages) {
    if (message.role != "tool")
      continue;
    ++outcomes;
    const auto result = Json::parse(message.text);
    EXPECT_TRUE(result["metadata"].value("action_completed", false));
    EXPECT_FALSE(result["metadata"].contains("hook_stop"));
    EXPECT_FALSE(result["metadata"].contains("hook_failure"));
    EXPECT_THAT(result.value("content", ""), HasSubstr("fix formatting"));
  }
  EXPECT_EQ(outcomes, 2);
}
TEST_F(Modernization, ExplicitPostToolStopStillReconcilesRemainingCalls) {
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
  config.hooks["PostToolUse"] = Json::array({{{"command", "fixture"}}});
  processes.result = {0, "{\"continue\":false,\"stopReason\":\"stop now\"}"};
  StateStore store(config.state_root);
  auto session = store.create();
  Tools tools(config, files, processes, http, clock);
  Agent agent(config, provider, tools, store, {});
  EXPECT_THROW(agent.run(session, "run", cancel), HookFailure);
  std::vector<Json> results;
  for (const auto &message : store.load_id(session.id, true).messages)
    if (message.role == "tool")
      results.push_back(Json::parse(message.text));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_TRUE(results[0]["metadata"].value("action_completed", false));
  EXPECT_TRUE(results[0]["metadata"].contains("hook_stop"));
  EXPECT_FALSE(results[0]["metadata"].contains("hook_failure"));
  EXPECT_EQ(results[1]["metadata"]["recovery"], "not_started");
}
TEST_F(Modernization, CorruptPlanResumesAndQuarantinesBeforeReplacement) {
  for (const auto &damaged : {std::string("{broken"), std::string("[]\0opaque", 9)}) {
    const auto id = unique_id();
    const auto path = config.state_root / "plans" / (id + ".json");
    fs::create_directories(path.parent_path());
    files.atomic_write(path, damaged, true);
    Tools tools(config, files, processes, http, clock);
    EXPECT_NO_THROW(tools.use_session(id));
    EXPECT_EQ(files.read(path), damaged);
    const auto result =
        tools.execute({"plan",
                       "update_plan",
                       {{"plan", Json::array({{{"step", "repair"}, {"status", "pending"}}})}}},
                      cancel);
    ASSERT_FALSE(result.error) << result.content;
    EXPECT_TRUE(Json::parse(files.read(path)).is_array());
    size_t recovered = 0;
    for (const auto &entry : fs::directory_iterator(path.parent_path()))
      if (entry.path().filename().string().rfind(id + ".corrupt-", 0) == 0) {
        ++recovered;
        EXPECT_EQ(files.read(entry.path()), damaged);
        struct stat st{};
        ASSERT_EQ(::stat(entry.path().c_str(), &st), 0);
        EXPECT_EQ(st.st_mode & 0777, 0600);
      }
    EXPECT_EQ(recovered, 1u);
    EXPECT_FALSE(fs::exists(config.home / "plans"));
  }
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
  OutputSpool next(store);
  next.append(std::string(64, 'x'));
  next.flush();
  const auto scans = store->metrics.scans;
  next.close();
  EXPECT_TRUE(next.error().empty()) << next.error();
  EXPECT_EQ(next.bytes(), 64u);
  EXPECT_EQ(store->metrics.scans, scans);
  const auto records_dir = config.state_root / "abandoned-records";
  auto limited = std::make_shared<CaptureStore>(records_dir, 128, 4096, 2);
  for (int n = 0; n < 2; ++n) {
    OutputSpool old(limited);
    old.append("completed");
    old.close();
  }
  ASSERT_EQ(sqlite3_open((records_dir / "catalog.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "UPDATE captures SET active=1", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  {
    OutputSpool next(limited);
    next.append("new");
    next.close();
    ASSERT_TRUE(next.error().empty());
  }
  size_t completed = 0;
  for (const auto &entry : fs::directory_iterator(records_dir))
    if (entry.path().extension() == ".txt")
      ++completed;
  EXPECT_LE(completed, 2u);
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
    if (entry.path().filename().string().rfind("state.sqlite3.pre-v7-", 0) == 0) {
      ++backups;
      struct stat st{};
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
      const auto child = codet::test::start_helper(
          {"--codet-crash-helper", root.string(), id, mode, complete ? "complete" : "intent"});
      ASSERT_GE(child, 0);
      int status = 0;
      ASSERT_TRUE(codet::test::wait_helper(child, status));
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

TEST_F(Modernization, EmptyAnthropicRepliesKeepSourcePositionsWithoutEmptyWireMessages) {
  config.provider = "anthropic";
  config.model = "claude-sonnet-5-5";
  config.providers["anthropic"].api_key = "fixture";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message user;
  user.role = "user";
  user.text = "start";
  std::vector<codet::Message> history{user};
  http.status = 200;
  http.stream = "event: message_delta\ndata: "
                "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}\n\n"
                "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n";
  for (int i = 0; i < 3; ++i) {
    auto reply = provider.complete(config, "instructions", history, Json::array(), cancel, {});
    ASSERT_TRUE(reply.message.replay.empty());
    history.push_back(reply.message);
    user.text = "continue " + std::to_string(i);
    history.push_back(user);
    const auto body =
        Json::parse(provider.request(config, "instructions", history, Json::array()).body);
    for (const auto &message : body["messages"])
      EXPECT_FALSE(message["role"] == "assistant" && message["content"].empty());
    EXPECT_EQ(provider.conversation_state()["messages"].size(), history.size());
  }
}
TEST_F(Modernization, AnthropicModelTransitionResetsOnceThenPreservesNewToolPairs) {
  config.provider = "anthropic";
  config.model = "claude-sonnet-5-5";
  config.providers["anthropic"].api_key = "fixture";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message user;
  user.role = "user";
  user.text = "start";
  std::vector<codet::Message> history{user};
  (void)provider.request(config, "signed instructions", history, Json::array());
  config.model = "claude-sonnet-4-5";
  (void)provider.request(config, "transition", history, Json::array());
  codet::Message call, result;
  call.role = "assistant";
  call.calls = {{"new-call", "read_file", {{"path", "x"}}}};
  result.role = "tool";
  result.tool_call_id = "new-call";
  result.text = "completed";
  history.push_back(call);
  history.push_back(result);
  for (int i = 0; i < 4; ++i) {
    auto prompt = "plan update " + std::to_string(i);
    const auto body = Json::parse(provider.request(config, prompt, history, Json::array()).body);
    EXPECT_EQ(body["system"][0]["text"], prompt);
    EXPECT_EQ(body["messages"][1]["content"][0]["type"], "tool_use");
    EXPECT_EQ(body["messages"][2]["content"][0]["type"], "tool_result");
    EXPECT_EQ(provider.conversation_state()["messages"].size(), 3u);
  }
}
TEST_F(Modernization, IncompleteResponsesPreserveSpecificDiagnosticAndRawBody) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  http.status = 200;
  const std::string body =
      R"({"type":"response.incomplete","response":{"incomplete_details":{"reason":"max_output_tokens"}}})";
  for (const auto &frame :
       {std::make_pair(std::string("event: response.incomplete\n"), body),
        std::make_pair(std::string{}, body),
        std::make_pair(std::string("event: response.incomplete\n"), std::string("{invalid"))}) {
    http.requests = 0;
    http.stream = frame.first + "data: " + frame.second + "\n\n";
    try {
      provider.complete(config, "", {}, Json::array(), cancel, {});
      FAIL();
    } catch (const ProviderError &e) {
      EXPECT_THAT(e.what(), HasSubstr("output limit or interruption"));
      EXPECT_EQ(e.body, frame.second);
    }
    EXPECT_EQ(http.requests, 1);
  }
}

Json conversation_fixture(const std::string &text) {
  return {
      {"provider", "anthropic"},
      {"messages",
       Json::array(
           {{{"source", "source"},
             {"wire",
              Json::array({{{"role", "user"},
                            {"content", Json::array({{{"type", "text"}, {"text", text}}})}}})}}})}};
}
TEST_F(Modernization, MalformedProviderCachesFailClearlyWithoutRepairingStoredBytes) {
  StateStore state(config.state_root);
  auto session = state.create();
  codet::Message user;
  user.role = "user";
  user.text = "valid transcript";
  session.messages = {user};
  session.provider_state = conversation_fixture("valid");
  state.append_messages(session, 0, true);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "UPDATE provider_parts SET payload='{truncated' WHERE position=1",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  try {
    (void)state.load_id(session.id, false);
    FAIL();
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("Corrupt provider conversation cache; transcript preserved"));
  }
  sqlite3_stmt *q = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT payload FROM provider_parts WHERE position=1", -1, &q,
                               nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  EXPECT_STREQ(reinterpret_cast<const char *>(sqlite3_column_text(q, 0)), "{truncated");
  sqlite3_finalize(q);
  ASSERT_EQ(sqlite3_exec(db, "UPDATE provider_parts SET position=2,payload='{}' WHERE position=1",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_THROW(state.load_id(session.id, false), Error);
  sqlite3_close(db);
}
TEST_F(Modernization, ProviderCacheIsBoundedAndUncachedSavesWriteOnlyChangedParts) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture(std::string(9 * 1024 * 1024, 'x'));
  state.save_provider_state(session);
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  const auto baseline = state.serialized_bytes();
  session.provider_state["messages"].push_back(conversation_fixture("new")["messages"][0]);
  state.save_provider_state(session);
  EXPECT_LT(state.serialized_bytes() - baseline, 1000u);
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  auto small = state.create();
  small.provider_state = conversation_fixture("small");
  state.save_provider_state(small);
  EXPECT_GT(state.provider_cache_bytes(), 0u);
  EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  EXPECT_EQ(state.load_id(small.id, false).provider_state, small.provider_state);
  EXPECT_LT(state.provider_cache_bytes(), 1000u);
}
TEST_F(Modernization, InvalidProviderWritesAndFailedCommitsDoNotPublishCachedState) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture("valid");
  state.save_provider_state(session);
  const auto valid = session.provider_state;
  for (const auto &invalid :
       {Json::array(), Json{{"messages", 4}}, Json{{"provider", 4}}, Json{{"execution_digest", 4}},
        Json{{"messages", Json::array({{{"wire", "bad"}}})}},
        Json{{"messages",
              Json::array({{{"wire", Json::array({{{"role", "assistant"},
                                                   {"content", Json::array()}}})}}})}}}) {
    session.provider_state = invalid;
    EXPECT_THROW(state.save_provider_state(session), Error);
    EXPECT_EQ(state.load_id(session.id, false).provider_state, valid);
  }
  session.provider_state = valid;
  session.provider_state["messages"].push_back(conversation_fixture("next")["messages"][0]);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TRIGGER fail_provider BEFORE INSERT ON provider_parts WHEN "
                         "NEW.position=2 BEGIN SELECT RAISE(FAIL,'provider rollback'); END",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_THROW(state.save_provider_state(session), Error);
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER fail_provider", nullptr, nullptr, nullptr), SQLITE_OK);
  state.save_provider_state(session);
  EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
  sqlite3_close(db);
}
TEST_F(Modernization, ProviderSnapshotsFollowOwningAndNestedCommitOutcomes) {
  for (const auto *mode : {"durable", "fast"})
    for (const auto *operation : {"provider", "append", "replace", "reconcile"})
      for (bool fail_commit : {false, true}) {
        SCOPED_TRACE(std::string(mode) + "/" + operation + (fail_commit ? "/rollback" : "/commit"));
        const auto root = config.state_root / mode / operation / (fail_commit ? "fail" : "ok");
        StateStore state(root, {}, mode);
        auto session = state.create();
        codet::Message assistant;
        assistant.role = "assistant";
        assistant.calls = {{"pending", "read_file", {{"path", "x"}}}};
        session.messages = {assistant};
        session.provider_state = conversation_fixture("old");
        state.save(session);
        sqlite3 *connection = nullptr;
        ASSERT_EQ(sqlite3_open((root / "state.sqlite3").c_str(), &connection), SQLITE_OK);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(connection, sqlite3_close);
        if (fail_commit)
          ASSERT_EQ(sqlite3_exec(db.get(),
                                 "CREATE TABLE parent(id INTEGER PRIMARY KEY);"
                                 "CREATE TABLE poison(id INTEGER REFERENCES parent(id) "
                                 "DEFERRABLE INITIALLY DEFERRED);"
                                 "CREATE TRIGGER fail_outer AFTER INSERT ON provider_parts "
                                 "WHEN NEW.position=1 BEGIN INSERT INTO poison VALUES(1); END;",
                                 nullptr, nullptr, nullptr),
                    SQLITE_OK);
        session.provider_state = conversation_fixture("new");
        const auto write = [&] {
          const std::string action = operation;
          if (action == "provider")
            state.save_provider_state(session);
          else if (action == "append") {
            codet::Message user;
            user.role = "user";
            user.text = "appended";
            session.messages.push_back(user);
            state.append_messages(session, 1, true);
          } else if (action == "replace")
            state.replace_history(session);
          else
            state.reconcile(session, true);
        };
        if (fail_commit) {
          try {
            write();
            FAIL() << "Expected the owning COMMIT to reject its deferred foreign key";
          } catch (const Error &error) {
            EXPECT_THAT(error.what(), HasSubstr("FOREIGN KEY constraint failed"));
          }
          ASSERT_EQ(sqlite3_exec(db.get(), "DROP TRIGGER fail_outer", nullptr, nullptr, nullptr),
                    SQLITE_OK);
        } else
          EXPECT_NO_THROW(write());
        // A reload would repopulate the cache and hide the regression. First save a suffix
        // using a prefix different from the rows that a stale snapshot would describe.
        session.provider_state = conversation_fixture(fail_commit ? "new" : "old");
        session.provider_state["messages"].push_back(conversation_fixture("tail")["messages"][0]);
        state.save_provider_state(session);
        EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
        StateStore reopened(root, {}, mode);
        EXPECT_EQ(reopened.load_id(session.id, false).provider_state, session.provider_state);
      }
}
TEST_F(Modernization, DurableCompletionAvoidsBarrierWhileFastCompletionKeepsIt) {
  for (const auto *mode : {"durable", "fast"}) {
    StateStore state(config.state_root / mode, {}, mode);
    auto session = state.create();
    state.begin_turn(session.id);
    state.finish_turn(session.id, "complete");
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open((config.state_root / mode / "state.sqlite3").c_str(), &db), SQLITE_OK);
    sqlite3_stmt *q = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(db, "SELECT count(*) FROM sqlite_master WHERE name='durability_barrier'",
                           -1, &q, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(q, 0), std::string(mode) == "fast" ? 1 : 0);
    sqlite3_finalize(q);
    sqlite3_close(db);
  }
}
TEST_F(Modernization, CurrentSchemaReadersDoNotNeedTheWriterLock) {
  StateStore original(config.state_root);
  original.create();
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr), SQLITE_OK);
  EXPECT_NO_THROW({
    StateStore reader(config.state_root);
    EXPECT_EQ(reader.list().size(), 1u);
  });
  ASSERT_EQ(sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
}

TEST_F(Modernization, LegacyEmptyAnthropicPartsNormalizeReadOnlyThenRewriteOnSave) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture("before");
  state.save_provider_state(session);
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &raw), SQLITE_OK);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
  ASSERT_EQ(sqlite3_exec(raw,
                         "UPDATE provider_parts SET "
                         "payload='{\"source\":\"source\",\"wire\":[{\"role\":\"assistant\","
                         "\"content\":[]}]}' WHERE position=1",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  session = state.load_id(session.id, false);
  EXPECT_TRUE(session.provider_state["messages"][0]["wire"].empty());
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  EXPECT_EQ(state.provider_revision(session.id), 0u);
  sqlite3_stmt *row = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(raw, "SELECT payload FROM provider_parts WHERE position=1", -1, &row,
                               nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(row), SQLITE_ROW);
  EXPECT_THAT(reinterpret_cast<const char *>(sqlite3_column_text(row, 0)), HasSubstr("assistant"));
  sqlite3_finalize(row);
  ASSERT_EQ(
      sqlite3_exec(
          raw,
          "CREATE TABLE parent(id INTEGER PRIMARY KEY);"
          "CREATE TABLE poison(id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED);"
          "CREATE TRIGGER fail_outer AFTER INSERT ON provider_parts WHEN NEW.position=1 "
          "BEGIN INSERT INTO poison VALUES(1); END;",
          nullptr, nullptr, nullptr),
      SQLITE_OK);
  EXPECT_THROW(state.save_provider_state(session), Error);
  ASSERT_EQ(sqlite3_exec(raw, "DROP TRIGGER fail_outer", nullptr, nullptr, nullptr), SQLITE_OK);
  state.save_provider_state(session); // Deliberately no reload before the retry.
  EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
  ASSERT_EQ(sqlite3_prepare_v2(raw, "SELECT payload FROM provider_parts WHERE position=1", -1, &row,
                               nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(row), SQLITE_ROW);
  EXPECT_THAT(reinterpret_cast<const char *>(sqlite3_column_text(row, 0)),
              Not(HasSubstr("assistant")));
  sqlite3_finalize(row);
}
TEST_F(Modernization, LargeProviderHintsAvoidPrefixReadsAndRejectExternalChanges) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture(std::string(9 * 1024 * 1024, 'x'));
  state.save_provider_state(session);
  const auto before = state.serialized_bytes(), reads = state.provider_rows_read();
  for (int i = 0; i < 4; ++i) {
    const ProviderStateUpdate hint{state.provider_revision(session.id),
                                   session.provider_state["messages"].size()};
    session.provider_state["messages"].push_back(conversation_fixture("new")["messages"][0]);
    state.append_messages(session, 0, true, hint);
  }
  EXPECT_EQ(state.provider_rows_read(), reads);
  EXPECT_LT(state.serialized_bytes() - before, 4000u);
  EXPECT_EQ(state.provider_cache_bytes(), 0u);
  const ProviderStateUpdate stale{state.provider_revision(session.id),
                                  session.provider_state["messages"].size()};
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &raw), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(raw, "UPDATE provider_parts SET payload='{broken' WHERE position=1",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_EQ(state.provider_revision(session.id), 0u);
  session.provider_state["messages"].push_back(conversation_fixture("last")["messages"][0]);
  EXPECT_THROW(state.save_provider_state(session, stale), Error);
  EXPECT_GT(state.provider_rows_read(), reads);
  sqlite3_close(raw);
}
TEST_F(Modernization, ProviderHintRollbackInvalidatesTicketBeforeNextSave) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture("old");
  state.save_provider_state(session);
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &raw), SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(
          raw,
          "CREATE TABLE parent(id INTEGER PRIMARY KEY);"
          "CREATE TABLE poison(id INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED);"
          "CREATE TRIGGER fail_outer AFTER INSERT ON provider_parts WHEN NEW.position=2 "
          "BEGIN INSERT INTO poison VALUES(1); END;",
          nullptr, nullptr, nullptr),
      SQLITE_OK);
  session = state.load_id(session.id, false);
  const ProviderStateUpdate hint{state.provider_revision(session.id), 1};
  session.provider_state["messages"].push_back(conversation_fixture("new")["messages"][0]);
  EXPECT_THROW(state.append_messages(session, 0, true, hint), Error);
  EXPECT_EQ(state.provider_revision(session.id), 0u);
  ASSERT_EQ(sqlite3_exec(raw, "DROP TRIGGER fail_outer", nullptr, nullptr, nullptr), SQLITE_OK);
  state.save_provider_state(session, hint);
  EXPECT_EQ(state.load_id(session.id, false).provider_state, session.provider_state);
  sqlite3_close(raw);
}
TEST_F(Modernization, StatelessReasoningIncludeDoesNotChangeEffortDefaults) {
  config.model = "gpt-5.5";
  config.effort.clear();
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  const auto body = Json::parse(provider.request(config, "", {}, Json::array()).body);
  EXPECT_EQ(body["include"], Json::array({"reasoning.encrypted_content"}));
  EXPECT_FALSE(body.contains("reasoning"));
  http.status = 200;
  http.stream =
      "data: " +
      Json({{"type", "response.completed"},
            {"response",
             {{"status", "completed"},
              {"output",
               Json::array(
                   {{{"type", "reasoning"}, {"id", "reason"}, {"encrypted_content", "secret"}},
                    {{"type", "function_call"},
                     {"call_id", "call"},
                     {"name", "read_file"},
                     {"arguments", "{}"}}})}}}})
          .dump() +
      "\n\n";
  auto response = provider.complete(config, "", {}, Json::array(), cancel, {});
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "call";
  result.text = "done";
  const auto next =
      Json::parse(provider.request(config, "", {response.message, result}, Json::array()).body);
  EXPECT_EQ(next["input"][1]["encrypted_content"], "secret");
  EXPECT_EQ(next["input"][2]["call_id"], "call");
}
TEST_F(Modernization, ExecutionOnlyEpochUpdatesKeepSystemAndDeduplicateWireParts) {
  config.provider = "anthropic";
  config.model = "claude-sonnet-5-5";
  config.providers["anthropic"].api_key = "fixture";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message user, runtime;
  user.role = "user";
  user.text = "continue";
  runtime.role = "system";
  runtime.origin = "runtime";
  runtime.text = "running";
  const auto first =
      Json::parse(provider.request(config, "stable system", {user, runtime}, Json::array()).body);
  auto saved = provider.conversation_state();
  provider.restore_conversation(saved);
  (void)provider.request(config, "stable system", {user, runtime}, Json::array());
  EXPECT_EQ(provider.conversation_state(), saved);
  runtime.text = "no live handles";
  auto next =
      Json::parse(provider.request(config, "stable system", {user, runtime}, Json::array()).body);
  EXPECT_EQ(next["system"], first["system"]);
  EXPECT_EQ(provider.conversation_state()["messages"].size(), saved["messages"].size() + 1);
  EXPECT_FALSE(provider.conversation_state()["messages"].back().contains("source"));
  EXPECT_THAT(next["messages"].dump(), HasSubstr("no live handles"));
  EXPECT_EQ(next["messages"].dump().find("stable system"), std::string::npos);
}

TEST_F(Modernization, ExplicitRepairRollsBackAfterBackupAndNeverChangesJournal) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture("valid");
  state.save(session);
  state.tool_started(session.id, {"retired", "exec_command", {{"cmd", "old effect"}}});
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &raw), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(raw,
                         "UPDATE provider_parts SET payload='{broken' WHERE position=1;"
                         "CREATE TRIGGER refuse_repair BEFORE DELETE ON provider_parts BEGIN "
                         "SELECT RAISE(FAIL,'repair rollback'); END;",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  Tools tools(config, files, processes, http, clock);
  const auto journal = state.recovery_facts(session.id);
  EXPECT_THROW(state.repair(session.id, false, true, false, tools), Error);
  EXPECT_EQ(state.recovery_facts(session.id), journal);
  EXPECT_THROW(state.load_id(session.id, false), Error);
  EXPECT_EQ(std::distance(fs::directory_iterator(config.state_root / "repairs"),
                          fs::directory_iterator()),
            1);
  ASSERT_EQ(sqlite3_exec(raw, "DROP TRIGGER refuse_repair", nullptr, nullptr, nullptr), SQLITE_OK);
  const auto repaired = state.repair(session.id, false, true, false, tools);
  EXPECT_TRUE(repaired["changed"]);
  EXPECT_EQ(state.recovery_facts(session.id), journal);
  EXPECT_TRUE(state.load_id(session.id, false).provider_state.is_null());
  sqlite3_close(raw);
}

TEST_F(Modernization, LegacyEmptyPartsAllowResumeCompactionAndProviderSwitch) {
  struct Summary : Provider {
    ProviderResult complete(const Config &, const std::string &,
                            const std::vector<codet::Message> &, const Json &, const Cancellation &,
                            const ChunkSink &, const ItemSink &) override {
      ProviderResult result;
      result.message.role = "assistant";
      result.message.text = "done";
      return result;
    }
  } provider;
  for (const auto *action : {"resume", "compact", "switch"}) {
    SCOPED_TRACE(action);
    StateStore state(config.state_root / action);
    auto session = state.create();
    codet::Message old, user, empty;
    old.role = "assistant";
    old.text = "completed older work";
    user.role = "user";
    user.text = "continue";
    empty.role = "assistant";
    session.messages = {old, user, empty};
    session.provider = "anthropic";
    session.model = "claude-sonnet-5-5";
    session.provider_state = conversation_fixture("legacy");
    state.save(session);
    sqlite3 *raw = nullptr;
    ASSERT_EQ(sqlite3_open((config.state_root / action / "state.sqlite3").c_str(), &raw),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(raw,
                           "UPDATE provider_parts SET "
                           "payload='{\"source\":\"source\",\"wire\":[{\"role\":\"assistant\","
                           "\"content\":[]}]}' WHERE position=1",
                           nullptr, nullptr, nullptr),
              SQLITE_OK);
    sqlite3_close(raw);
    config.provider = std::string(action) == "switch" ? "openai" : "anthropic";
    config.model = config.provider == "openai" ? "gpt-6.1-sol" : "claude-sonnet-5-5";
    Tools tools(config, files, processes, http, clock);
    Agent agent(config, provider, tools, state, {});
    if (std::string(action) == "compact")
      EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
    else
      EXPECT_EQ(agent.run(session, "new turn", cancel).text, "done");
    EXPECT_FALSE(state.load_id(session.id, false).messages.empty());
  }
}
TEST_F(Modernization, RepairBackupRetainsMalformedUtf8Bytes) {
  StateStore state(config.state_root);
  auto session = state.create();
  session.provider_state = conversation_fixture("valid");
  state.save(session);
  sqlite3 *raw = nullptr;
  ASSERT_EQ(sqlite3_open((config.state_root / "state.sqlite3").c_str(), &raw), SQLITE_OK);
  sqlite3_stmt *update = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(raw, "UPDATE provider_parts SET payload=? WHERE position=1", -1,
                               &update, nullptr),
            SQLITE_OK);
  const std::string invalid("bad\xff\0suffix", 11);
  ASSERT_EQ(
      sqlite3_bind_text(update, 1, invalid.data(), static_cast<int>(invalid.size()), SQLITE_STATIC),
      SQLITE_OK);
  ASSERT_EQ(sqlite3_step(update), SQLITE_DONE);
  sqlite3_finalize(update);
  sqlite3_close(raw);
  Tools tools(config, files, processes, http, clock);
  const auto report = state.repair(session.id, false, true, false, tools);
  const auto backup = Json::parse(files.read(report["backup"].get<std::string>(), 100000));
  EXPECT_EQ(backup["provider_parts"][1]["payload"]["encoding"], "base64");
  EXPECT_EQ(backup["provider_parts"][1]["payload"]["bytes"], base64_encode(invalid));
  EXPECT_TRUE(state.load_id(session.id, false).provider_state.is_null());
}
} // namespace
