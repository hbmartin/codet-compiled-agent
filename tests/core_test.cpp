#include "codet/core.hpp"
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace codet;
using namespace testing;
namespace {
struct Temp {
  fs::path root;
  Temp() {
    std::string pattern = (fs::temp_directory_path() / "codet-test-XXXXXX").string();
    std::vector<char> b(pattern.begin(), pattern.end());
    b.push_back(0);
    auto *p = mkdtemp(b.data());
    if (!p)
      throw std::runtime_error("mkdtemp");
    root = p;
  }
  ~Temp() {
    std::error_code e;
    fs::remove_all(root, e);
  }
};
struct FakeClock : Clock {
  int64_t now = 10000;
  int slept = 0;
  int64_t seconds() const override {
    return now;
  }
  void sleep_ms(int n, const Cancellation &c) override {
    c.check();
    slept += n;
  }
};
struct MockHttp : HttpTransport {
  MOCK_METHOD(HttpResponse, perform, (const HttpRequest &, const Cancellation &, const ChunkSink &),
              (override));
};
struct MockProcesses : Processes {
  MOCK_METHOD(ProcessResult, run, (const ProcessSpec &, const Cancellation &), (override));
  MOCK_METHOD(std::string, start, (const ProcessSpec &), (override));
  MOCK_METHOD(ProcessResult, poll, (const std::string &), (override));
  MOCK_METHOD(void, stop, (const std::string &), (override));
};
struct MockProvider : Provider {
  MOCK_METHOD(ProviderResult, complete,
              (const Config &, const std::string &, const std::vector<codet::Message> &,
               const Json &, const Cancellation &, const ChunkSink &),
              (override));
};
struct MockFiles : FileSystem {
  MOCK_METHOD(std::string, read, (const fs::path &, size_t), (const, override));
  MOCK_METHOD(void, atomic_write, (const fs::path &, const std::string &, bool), (override));
  MOCK_METHOD(void, remove, (const fs::path &), (override));
  MOCK_METHOD(fs::path, canonical, (const fs::path &), (const, override));
};
struct Fixture : Test {
  Temp temp;
  NativeFileSystem files;
  FakeClock clock;
  Cancellation cancel;
  NiceMock<MockHttp> http;
  NiceMock<MockProcesses> processes;
  Config config;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "workspace";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "fixture-model";
    config.providers["openai"].api_key = "fake-key";
  }
  void write(const std::string &p, const std::string &text) {
    files.atomic_write(config.cwd / p, text);
  }
  ToolCall call(std::string name, Json args) {
    return {unique_id(), std::move(name), std::move(args)};
  }
};
TEST(Sse, FragmentedCrLfMultilineAndComments) {
  std::vector<std::pair<std::string, std::string>> frames;
  SseDecoder decoder([&](const auto &e, const auto &d) { frames.emplace_back(e, d); });
  const std::string data =
      ": ping\r\nevent: delta\r\ndata: first\r\ndata: second\r\n\r\ndata: [DONE]\n\n";
  for (char ch : data)
    decoder.feed(std::string(1, ch));
  decoder.finish();
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[0], std::make_pair(std::string("delta"), std::string("first\nsecond")));
}
TEST_F(Fixture, AtomicWritesPreserveModeAndLeaveNoScratchFiles) {
  write("a", "before");
  chmod((config.cwd / "a").c_str(), 0755);
  write("a", "after");
  struct stat st {};
  ASSERT_EQ(stat((config.cwd / "a").c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0755);
  EXPECT_EQ(files.read(config.cwd / "a"), "after");
  EXPECT_EQ(std::distance(fs::directory_iterator(config.cwd), fs::directory_iterator()), 1);
}
TEST_F(Fixture, PermissionsDenyWinsOverAllowAndBypass) {
  config.permission_mode = "bypass";
  config.allow = {"*"};
  config.deny = {"Bash(rm*)"};
  Permissions p(config, files);
  EXPECT_EQ(p.decide({"Bash", "rm -rf x", std::nullopt, true}), Decision::deny);
  EXPECT_EQ(p.decide({"Read", "a", config.cwd / "a", false}), Decision::allow);
}
TEST_F(Fixture, PermissionRootsRejectSiblingAndSymlinkEscapes) {
  const auto other = temp.root / "workspace-other";
  fs::create_directories(other);
  fs::create_directory_symlink(other, config.cwd / "link");
  Permissions p(config, files);
  EXPECT_EQ(p.decide({"Read", "other", other / "a", false}), Decision::deny);
  EXPECT_EQ(p.decide({"Write", "link", config.cwd / "link/a", true}), Decision::deny);
}
TEST_F(Fixture, HeadlessMutationIsDeniedAndReadRemainsAvailable) {
  write("a", "hello");
  Tools t(config, files, processes, http, clock);
  EXPECT_FALSE(t.execute(call("Read", {{"file_path", "a"}}), cancel).error);
  auto result = t.execute(call("Write", {{"file_path", "a"}, {"content", "changed"}}), cancel);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(files.read(config.cwd / "a"), "hello");
}
TEST_F(Fixture, EditRejectsStaleAndAmbiguousContent) {
  config.permission_mode = "accept-edits";
  write("a", "same same");
  Tools t(config, files, processes, http, clock);
  ASSERT_FALSE(t.execute(call("Read", {{"file_path", "a"}}), cancel).error);
  EXPECT_TRUE(
      t.execute(call("Edit", {{"file_path", "a"}, {"old_string", "same"}, {"new_string", "new"}}),
                cancel)
          .error);
  write("a", "different");
  EXPECT_TRUE(
      t.execute(call("Write", {{"file_path", "a"}, {"content", "overwrite"}}), cancel).error);
  EXPECT_EQ(files.read(config.cwd / "a"), "different");
}
TEST_F(Fixture, BatchValidationPrecedesAnyMutation) {
  config.permission_mode = "accept-edits";
  write("a", "alpha");
  write("b", "beta");
  Tools t(config, files, processes, http, clock);
  for (auto *p : {"a", "b"})
    ASSERT_FALSE(t.execute(call("Read", {{"file_path", p}}), cancel).error);
  const Json edits =
      Json::array({{{"file_path", "a"}, {"old_string", "alpha"}, {"new_string", "new"}},
                   {{"file_path", "b"}, {"old_string", "missing"}, {"new_string", "new"}}});
  EXPECT_TRUE(t.execute(call("BatchEdit", {{"edits", edits}}), cancel).error);
  EXPECT_EQ(files.read(config.cwd / "a"), "alpha");
}
TEST_F(Fixture, PatchAddsUpdatesAndDeletesFiles) {
  config.permission_mode = "accept-edits";
  write("old", "one\ntwo\n");
  Tools t(config, files, processes, http, clock);
  ASSERT_FALSE(t.execute(call("Read", {{"file_path", "old"}}), cancel).error);
  auto result =
      t.execute(call("ApplyPatch",
                     {{"patch", "*** Begin Patch\n*** Update File: old\n@@\n "
                                "one\n-two\n+three\n*** Add File: new\n+hello\n*** End Patch\n"}}),
                cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(files.read(config.cwd / "old"), "one\nthree\n");
  EXPECT_EQ(files.read(config.cwd / "new"), "hello\n");
}
TEST_F(Fixture, GrepTreatsQuotesAsDataWithoutInvokingShell) {
  write("a", "a'b\nother\n");
  Tools t(config, files, processes, http, clock);
  EXPECT_CALL(processes, run(_, _)).Times(0);
  const auto result = t.execute(call("Grep", {{"pattern", "a'b"}}), cancel);
  ASSERT_FALSE(result.error);
  EXPECT_THAT(result.content, HasSubstr("a:1:a'b"));
}
TEST_F(Fixture, ConfigLayeringProfilesOverridesAndTypes) {
  files.atomic_write(config.home / "config.toml", "provider='openai'\nmodel='global'\n");
  files.atomic_write(config.home / "work.config.toml", "model='profile'\n");
  auto c = load_config(config.home, config.cwd, "work", {"model='override'"}, files);
  EXPECT_EQ(c.model, "override");
  EXPECT_THROW(load_config(config.home, config.cwd, "", {"max_turns=0"}, files), Error);
}
TEST_F(Fixture, CredentialsImportIsPrivateAndDoesNotModifySource) {
  write("tokens.json", Json({{"tokens",
                              {{"access_token", "fake-access"},
                               {"refresh_token", "fake-refresh"},
                               {"account_id", "account"}}}})
                           .dump());
  const auto before = files.read(config.cwd / "tokens.json");
  CredentialStore c(config.home, files, clock);
  c.import(config.cwd / "tokens.json");
  EXPECT_EQ(c.load()["codex"]["access_token"], "fake-access");
  EXPECT_EQ(files.read(config.cwd / "tokens.json"), before);
  struct stat st {};
  ASSERT_EQ(stat((config.home / "auth.json").c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600);
}
TEST_F(Fixture, CredentialRefreshRotatesAndPersistsTokens) {
  write("tokens.json",
        Json({{"access_token", "old"}, {"refresh_token", "refresh"}, {"expires_at", 1}}).dump());
  CredentialStore c(config.home, files, clock);
  c.import(config.cwd / "tokens.json");
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([](const HttpRequest &r, const Cancellation &, const ChunkSink &) {
        EXPECT_EQ(Json::parse(r.body)["grant_type"], "refresh_token");
        return HttpResponse{
            200, R"({"access_token":"new","refresh_token":"rotated","expires_in":3600})"};
      }));
  EXPECT_EQ(c.codex(http, cancel)["access_token"], "new");
  EXPECT_EQ(c.load()["codex"]["refresh_token"], "rotated");
}
TEST_F(Fixture, ExpiredCredentialsAndFailedRefreshLeaveStoredTokensIntact) {
  CredentialStore creds(config.home, files, clock);
  write("tokens.json", R"({"access_token":"expired","expires_at":1})");
  creds.import(config.cwd / "tokens.json");
  EXPECT_CALL(http, perform(_, _, _)).Times(0);
  EXPECT_THROW(creds.codex(http, cancel), Error);
  Mock::VerifyAndClearExpectations(&http);
  write("tokens.json",
        R"({"access_token":"expired","refresh_token":"original-refresh","expires_at":1})");
  creds.import(config.cwd / "tokens.json");
  const auto before = files.read(config.home / "auth.json");
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Return(HttpResponse{400, "sensitive provider error body"}));
  try {
    creds.codex(http, cancel);
    FAIL() << "Expected failed refresh";
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), Not(HasSubstr("sensitive")));
  }
  EXPECT_EQ(files.read(config.home / "auth.json"), before);
  EXPECT_CALL(http, perform(_, _, _)).WillOnce(Return(HttpResponse{200, R"({"wrong":true})"}));
  EXPECT_THROW(creds.codex(http, cancel), Error);
  EXPECT_EQ(files.read(config.home / "auth.json"), before);
}
TEST_F(Fixture, TransportFailuresHaveBoundedRetriesAndCancellationPropagates) {
  CredentialStore creds(config.home, files, clock);
  ApiProvider provider(http, creds, clock);
  EXPECT_CALL(http, perform(_, _, _))
      .Times(4)
      .WillRepeatedly(Throw(Error("TLS verification failed")));
  EXPECT_THROW(provider.complete(config, "", {}, Json::array(), cancel, {}), Error);
  EXPECT_EQ(clock.slept, 1750);
  EXPECT_CALL(http, perform(_, _, _)).WillOnce(Throw(Error("Interrupted", 130)));
  try {
    provider.complete(config, "", {}, Json::array(), cancel, {});
    FAIL() << "Expected interruption";
  } catch (const Error &e) {
    EXPECT_EQ(e.code, 130);
  }
  EXPECT_EQ(clock.slept, 1750);
}
TEST_F(Fixture, ReadRejectsBinaryAndOversizedFilesAndEditsUnicode) {
  config.permission_mode = "accept-edits";
  Tools tools(config, files, processes, http, clock);
  write("binary", std::string("a\0b", 3));
  EXPECT_TRUE(tools.execute(call("Read", {{"file_path", "binary"}}), cancel).error);
  write("large", "");
  ASSERT_EQ(::truncate((config.cwd / "large").c_str(), 17 * 1024 * 1024), 0);
  EXPECT_TRUE(tools.execute(call("Read", {{"file_path", "large"}}), cancel).error);
  write(u8"café.txt", u8"before: 日本語\n");
  const auto read = tools.execute(call("Read", {{"file_path", u8"café.txt"}}), cancel);
  ASSERT_FALSE(read.error);
  EXPECT_THAT(read.content, HasSubstr(u8"日本語"));
  const auto edit = tools.execute(
      call("Edit",
           {{"file_path", u8"café.txt"}, {"old_string", "before"}, {"new_string", "after"}}),
      cancel);
  EXPECT_FALSE(edit.error) << edit.content;
  EXPECT_EQ(files.read(config.cwd / u8"café.txt"), u8"after: 日本語\n");
}
TEST_F(Fixture, ProjectConfigurationRequiresExplicitTrustAndCliOverridesWin) {
  files.atomic_write(config.home / "config.toml", "model='global'\n");
  fs::create_directories(config.cwd / ".codet");
  files.atomic_write(config.cwd / ".codet/config.toml", "model='project'\n");
  auto untrusted = load_config(config.home, config.cwd, "", {}, files);
  EXPECT_EQ(untrusted.model, "global");
  EXPECT_FALSE(untrusted.trust_project);
  files.atomic_write(config.home / "config.toml", "model='global'\ntrusted_projects=[" +
                                                      Json(config.cwd.string()).dump() + "]\n");
  auto trusted = load_config(config.home, config.cwd, "", {}, files);
  EXPECT_EQ(trusted.model, "project");
  EXPECT_TRUE(trusted.trust_project);
  EXPECT_EQ(load_config(config.home, config.cwd, "", {"model='cli'"}, files).model, "cli");
}
TEST_F(Fixture, ApprovalHooksCannotOverridePlanModeOrExplicitDenials) {
  config.hooks = {{"PermissionRequest", Json::array({{{"command", "fixture"}}})}};
  EXPECT_CALL(processes, run(_, _))
      .WillOnce(Return(ProcessResult{0, R"({"hookSpecificOutput":{"permissionDecision":"allow"}})",
                                     false, false}));
  Tools allowed(config, files, processes, http, clock);
  ASSERT_FALSE(
      allowed.execute(call("Write", {{"file_path", "allowed"}, {"content", "yes"}}), cancel).error);
  Mock::VerifyAndClearExpectations(&processes);
  EXPECT_CALL(processes, run(_, _)).Times(0);
  config.permission_mode = "plan";
  Tools plan(config, files, processes, http, clock);
  EXPECT_TRUE(
      plan.execute(call("Write", {{"file_path", "plan"}, {"content", "no"}}), cancel).error);
  config.permission_mode = "bypass";
  config.deny = {"Write"};
  Tools denied(config, files, processes, http, clock);
  EXPECT_TRUE(
      denied.execute(call("Write", {{"file_path", "denied"}, {"content", "no"}}), cancel).error);
  EXPECT_FALSE(fs::exists(config.cwd / "plan"));
  EXPECT_FALSE(fs::exists(config.cwd / "denied"));
}
TEST_F(Fixture, OpenaiToolCallsAssembleAcrossArbitraryChunks) {
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  const std::string stream =
      "data: "
      "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"id\",\"function\":{\"name\":"
      "\"Read\",\"arguments\":\"{\\\"file_\"}}]}}]}\n\n"
      "data: "
      "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":"
      "\"path\\\":\\\"a\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\ndata: [DONE]\n\n";
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([&](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        for (char c : stream)
          sink(std::string(1, c));
        return HttpResponse{200, stream};
      }));
  auto result = p.complete(config, "system", {}, Json::array(), cancel, {});
  ASSERT_EQ(result.message.calls.size(), 1u);
  EXPECT_EQ(result.message.calls[0].arguments["file_path"], "a");
}
TEST_F(Fixture, ProviderRetriesTransientStatusAndRejectsTruncatedStreams) {
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Return(HttpResponse{429, ""}))
      .WillOnce(Invoke([](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        sink("data: "
             "{\"choices\":[{\"delta\":{\"content\":\"ok\"},\"finish_reason\":\"stop\"}]}\n\ndata: "
             "[DONE]\n\n");
        return HttpResponse{200, ""};
      }));
  EXPECT_EQ(p.complete(config, "", {}, Json::array(), cancel, {}).message.text, "ok");
  EXPECT_GT(clock.slept, 0);
  EXPECT_CALL(http, perform(_, _, _)).WillOnce(Return(HttpResponse{200, ""}));
  EXPECT_THROW(p.complete(config, "", {}, Json::array(), cancel, {}), Error);
}
TEST_F(Fixture, StatePersistenceForkIsolationAndNewerSchemaRejection) {
  StateStore state(config.home);
  auto a = state.create("named");
  a.messages.push_back({"user", "hello", "", {}, Json::array()});
  state.save(a);
  auto b = state.fork(a);
  b.messages[0].text = "branch";
  state.save(b);
  EXPECT_EQ(state.load(a.id).messages[0].text, "hello");
  EXPECT_EQ(state.load(b.id).messages[0].text, "branch");
  EXPECT_EQ(state.list().size(), 2u);
}
TEST_F(Fixture, CrashRecoveryRestoresResultsAndNeverReplaysUnknownEffects) {
  StateStore state(config.home);
  auto s = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  ToolCall done{"done", "Write", {{"file_path", "a"}, {"content", "x"}}},
      uncertain{"uncertain", "Bash", {{"command", "some effect"}}};
  assistant.calls = {done, uncertain};
  s.messages.push_back(assistant);
  state.save(s);
  state.begin_turn(s.id);
  state.tool_started(s.id, done);
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = done.id;
  result.text = "recorded result";
  state.tool_finished(s.id, done, Json(result));
  state.tool_started(s.id, uncertain);
  auto recovered = state.load(s.id);
  ASSERT_EQ(recovered.messages.size(), 4u);
  EXPECT_EQ(recovered.messages[1].text, "recorded result");
  EXPECT_THAT(recovered.messages[2].text, HasSubstr("not replayed"));
  EXPECT_EQ(recovered.messages[2].tool_call_id, "uncertain");
}
TEST_F(Fixture, StateRejectsFutureSchemaWithoutDowngrading) {
  { StateStore state(config.home); }
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "PRAGMA user_version=99", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_THROW(StateStore state(config.home), Error);
}
TEST_F(Fixture, StateMigratesV1AndRejectsTruncatedMessages) {
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TABLE sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent "
                         "TEXT,updated INTEGER NOT NULL,"
                         "messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle');"
                         "INSERT INTO sessions VALUES('old','named',NULL,1,'[]','complete');PRAGMA "
                         "user_version=1;",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  sqlite3_close(db);
  {
    StateStore state(config.home);
    auto session = state.load("old");
    EXPECT_EQ(session.name, "named");
    EXPECT_TRUE(session.messages.empty());
    session.provider = "ollama";
    session.model = "fixture";
    state.save(session);
    EXPECT_EQ(state.load("old").provider, "ollama");
  }
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "UPDATE sessions SET messages='[{';", nullptr, nullptr, nullptr),
            SQLITE_OK);
  sqlite3_close(db);
  StateStore state(config.home);
  EXPECT_THROW(state.load("old"), Error);
}
TEST_F(Fixture, ControlledInterruptionAlsoRepairsPendingToolResults) {
  StateStore state(config.home);
  auto s = state.create();
  codet::Message message;
  message.role = "assistant";
  message.calls = {{"pending", "Bash", {{"command", "side effect"}}}};
  s.messages.push_back(message);
  state.save(s);
  state.finish_turn(s.id, "interrupted");
  const auto resumed = state.load(s.id);
  ASSERT_EQ(resumed.messages.size(), 3u);
  EXPECT_EQ(resumed.messages[1].role, "tool");
  EXPECT_EQ(resumed.messages[1].tool_call_id, "pending");
  EXPECT_THAT(resumed.messages[1].text, HasSubstr("not replayed"));
}
TEST_F(Fixture, StructuredOutputIsValidatedBeforeAcceptance) {
  StateStore state(config.home);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  StrictMock<MockProvider> provider;
  ProviderResult invalid;
  invalid.message.role = "assistant";
  invalid.message.calls = {{"invalid", "StructuredOutput", {{"answer", "wrong type"}}}};
  ProviderResult valid;
  valid.message.role = "assistant";
  valid.message.calls = {{"valid", "StructuredOutput", {{"answer", 42}}}};
  ProviderResult final;
  final.message.role = "assistant";
  final.message.text = "answer";
  EXPECT_CALL(provider, complete(_, _, _, _, _, _))
      .WillOnce(Return(invalid))
      .WillOnce(Return(valid))
      .WillOnce(Return(final));
  Agent agent(config, provider, tools, state, {});
  Json schema = {{"type", "object"},
                 {"properties", {{"answer", {{"type", "integer"}}}}},
                 {"required", Json::array({"answer"})}};
  auto result = agent.run(s, "answer", cancel, schema);
  EXPECT_EQ(result.structured["answer"], 42);
  EXPECT_THAT(s.messages[2].text, HasSubstr("validation failed"));
}
TEST_F(Fixture, ExternalSchemasAreRejectedWithoutNetworking) {
  StateStore state(config.home);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  StrictMock<MockProvider> provider;
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(s, "test", cancel,
                         {{"type", "object"}, {"$ref", "https://example.invalid/schema"}}),
               Error);
}
TEST_F(Fixture, CodexRequestReplaysEncryptedReasoningAndCustomPatchOutputs) {
  config.provider = "codex";
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  Tools tools(config, files, processes, http, clock);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.replay =
      Json::array({{{"type", "reasoning"}, {"id", "r"}, {"encrypted_content", "encrypted"}},
                   {{"type", "custom_tool_call"},
                    {"call_id", "patch"},
                    {"name", "apply_patch"},
                    {"input", "patch text"}}});
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "patch";
  result.text = "applied";
  auto r = p.request(config, "system", {assistant, result}, tools.schemas(),
                     {{"access_token", "fake"}, {"account_id", "account"}});
  auto body = Json::parse(r.body);
  EXPECT_EQ(body["input"][1]["encrypted_content"], "encrypted");
  EXPECT_EQ(body["input"][3]["type"], "custom_tool_call_output");
  EXPECT_EQ(r.headers.at("ChatGPT-Account-Id"), "account");
  bool found = false;
  for (const auto &tool : body["tools"])
    if (tool["name"] == "apply_patch") {
      found = true;
      EXPECT_EQ(tool["type"], "custom");
    }
  EXPECT_TRUE(found);
}
TEST_F(Fixture, CodexCompletionParsesToolsUsageAndReasoningReplay) {
  config.provider = "codex";
  write("auth", R"({"access_token":"fake"})");
  CredentialStore creds(config.home, files, clock);
  creds.import(config.cwd / "auth");
  ApiProvider p(http, creds, clock);
  Json response = {
      {"type", "response.completed"},
      {"response",
       {{"status", "completed"},
        {"output",
         Json::array({{{"type", "reasoning"}, {"id", "r"}, {"encrypted_content", "secret-replay"}},
                      {{"type", "function_call"},
                       {"call_id", "call"},
                       {"name", "Read"},
                       {"arguments", R"({"file_path":"a"})"}}})},
        {"usage", {{"input_tokens", 10}, {"output_tokens", 3}}}}}};
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([&](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        sink("data: " + response.dump() + "\n\n");
        return HttpResponse{200, ""};
      }));
  auto result = p.complete(config, "", {}, Json::array(), cancel, {});
  ASSERT_EQ(result.message.calls.size(), 1u);
  EXPECT_EQ(result.message.calls[0].name, "Read");
  EXPECT_EQ(result.message.replay.size(), 2u);
  EXPECT_EQ(result.usage.input, 10);
}
TEST_F(Fixture, AnthropicStreamPreservesThinkingSignatureAndToolArguments) {
  config.provider = "anthropic";
  config.providers["anthropic"].api_key = "fake";
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  const std::vector<Json> frames = {
      {{"type", "message_start"}, {"message", {{"usage", {{"input_tokens", 12}}}}}},
      {{"type", "content_block_start"},
       {"index", 0},
       {"content_block", {{"type", "thinking"}, {"thinking", ""}, {"signature", ""}}}},
      {{"type", "content_block_delta"},
       {"index", 0},
       {"delta", {{"type", "thinking_delta"}, {"thinking", "thought"}}}},
      {{"type", "content_block_delta"},
       {"index", 0},
       {"delta", {{"type", "signature_delta"}, {"signature", "sig"}}}},
      {{"type", "content_block_start"},
       {"index", 1},
       {"content_block",
        {{"type", "tool_use"}, {"id", "call"}, {"name", "Read"}, {"input", Json::object()}}}},
      {{"type", "content_block_delta"},
       {"index", 1},
       {"delta", {{"type", "input_json_delta"}, {"partial_json", R"({"file_path":"a"})"}}}},
      {{"type", "message_stop"}}};
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([&](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        for (const auto &j : frames)
          sink("data: " + j.dump() + "\n\n");
        return HttpResponse{200, ""};
      }));
  auto result = p.complete(config, "", {}, Json::array(), cancel, {});
  EXPECT_EQ(result.message.replay[0]["signature"], "sig");
  ASSERT_EQ(result.message.calls.size(), 1u);
  EXPECT_EQ(result.message.calls[0].arguments["file_path"], "a");
}
TEST_F(Fixture, GoogleAndOllamaStreamsHaveRealProtocolFixtures) {
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  config.provider = "google";
  config.providers["google"].api_key = "fake";
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        sink("data: "
             "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"google\"}]},\"finishReason\":"
             "\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":7}}\n\n");
        return HttpResponse{200, ""};
      }));
  EXPECT_EQ(p.complete(config, "", {}, Json::array(), cancel, {}).message.text, "google");
  config.provider = "ollama";
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        sink("{\"message\":{\"content\":\"olla\"},\"done\":false}\n{\"message\":{\"content\":"
             "\"ma\"},\"done\":true,\"eval_count\":2}\n");
        return HttpResponse{200, ""};
      }));
  EXPECT_EQ(p.complete(config, "", {}, Json::array(), cancel, {}).message.text, "ollama");
}
TEST_F(Fixture, OllamaToolRoundTripUsesObjectArgumentsAndNamedResults) {
  config.provider = "ollama";
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        const std::string frame =
            R"({"message":{"content":"","tool_calls":[{"function":{"name":"Read","arguments":{"file_path":"a"}}}]},"done":true})";
        for (char ch : frame)
          sink(std::string(1, ch));
        sink("\n");
        return HttpResponse{200, ""};
      }));
  auto result = p.complete(config, "", {}, Json::array(), cancel, {});
  ASSERT_EQ(result.message.calls.size(), 1u);
  codet::Message output;
  output.role = "tool";
  output.tool_call_id = result.message.calls[0].id;
  output.text = "file contents";
  const auto request = p.request(config, "system", {result.message, output}, Json::array(), {});
  const auto body = Json::parse(request.body);
  EXPECT_EQ(body["messages"][1]["tool_calls"][0]["function"]["arguments"],
            Json({{"file_path", "a"}}));
  EXPECT_EQ(body["messages"][2]["tool_name"], "Read");
  EXPECT_FALSE(body["messages"][2].contains("tool_call_id"));
}
TEST_F(Fixture, GoogleToolRoundTripGroupsResultsAndPreservesThoughtSignatures) {
  config.provider = "google";
  config.providers["google"].api_key = "fake";
  CredentialStore creds(config.home, files, clock);
  ApiProvider p(http, creds, clock);
  const auto frame = Json::parse(R"({"candidates":[{
    "content":{"parts":[
      {"functionCall":{"name":"Read","args":{"file_path":"a"}},
       "thoughtSignature":"opaque-signature"},
      {"functionCall":{"name":"Glob","args":{"pattern":"*"}}}
    ]},"finishReason":"STOP"
  }]})");
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Invoke([&](const HttpRequest &, const Cancellation &, const ChunkSink &sink) {
        sink("data: " + frame.dump() + "\n\n");
        return HttpResponse{200, ""};
      }));
  auto result = p.complete(config, "", {}, Json::array(), cancel, {});
  ASSERT_EQ(result.message.calls.size(), 2u);
  std::vector<codet::Message> messages = {result.message};
  for (const auto &call : result.message.calls) {
    codet::Message output;
    output.role = "tool";
    output.tool_call_id = call.id;
    output.text = "result";
    messages.push_back(output);
  }
  const auto body = Json::parse(p.request(config, "", messages, Json::array(), {}).body);
  ASSERT_EQ(body["contents"].size(), 2u);
  EXPECT_EQ(body["contents"][0]["parts"][0]["thoughtSignature"], "opaque-signature");
  ASSERT_EQ(body["contents"][1]["parts"].size(), 2u);
  EXPECT_EQ(body["contents"][1]["parts"][0]["functionResponse"]["name"], "Read");
  EXPECT_EQ(body["contents"][1]["parts"][1]["functionResponse"]["name"], "Glob");
}
TEST_F(Fixture, HookInputIsJsonAndDenialPreventsMutation) {
  config.permission_mode = "bypass";
  config.hooks = {{"PreToolUse", Json::array({{{"command", "exit 2"}, {"matcher", "Write"}}})}};
  EXPECT_CALL(processes, run(_, _))
      .WillOnce(Invoke([&](const ProcessSpec &spec, const Cancellation &) {
        auto input = Json::parse(files.read(spec.argv.back()));
        EXPECT_EQ(input["tool_name"], "Write");
        return ProcessResult{2, "blocked", false, false};
      }));
  Tools tools(config, files, processes, http, clock);
  EXPECT_TRUE(tools.execute(call("Write", {{"file_path", "a"}, {"content", "x"}}), cancel).error);
  EXPECT_FALSE(fs::exists(config.cwd / "a"));
}
TEST_F(Fixture, InstructionsIncludesSkillsAndExplicitMemoryRemainBounded) {
  write("AGENTS.md", "root guide\n@include more.md\n");
  write("more.md", "included guide\n@include AGENTS.md\n");
  fs::create_directories(config.cwd / ".agents/skills/demo");
  files.atomic_write(config.cwd / ".agents/skills/demo/SKILL.md", "skill guide");
  Tools tools(config, files, processes, http, clock);
  auto instructions = tools.instructions();
  EXPECT_THAT(instructions, HasSubstr("included guide"));
  EXPECT_THAT(instructions, HasSubstr("demo"));
  EXPECT_LT(instructions.size(), 100000u);
  EXPECT_EQ(tools.execute(call("Skill", {{"name", "demo"}}), cancel).content, "skill guide");
  EXPECT_TRUE(
      tools
          .execute(call("Memory", {{"action", "write"}, {"name", "../bad.md"}, {"content", "x"}}),
                   cancel)
          .error);
}
TEST_F(Fixture, BatchCanApplySeveralEditsToOneFileWithOneApproval) {
  write("a", "alpha beta");
  unsigned approvals = 0;
  Tools tools(config, files, processes, http, clock, [&](const PermissionRequest &) {
    ++approvals;
    return true;
  });
  ASSERT_FALSE(tools.execute(call("Read", {{"file_path", "a"}}), cancel).error);
  auto result = tools.execute(
      call("BatchEdit",
           {{"edits",
             Json::array({{{"file_path", "a"}, {"old_string", "alpha"}, {"new_string", "new"}},
                          {{"file_path", "a"}, {"old_string", "beta"}, {"new_string", "text"}}})}}),
      cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(files.read(config.cwd / "a"), "new text");
  EXPECT_EQ(approvals, 1u);
}
TEST_F(Fixture, UniversalDenyRulesCoverUtilityTools) {
  config.deny = {"Sleep", "Memory", "EnterPlanMode"};
  Tools tools(config, files, processes, http, clock);
  EXPECT_TRUE(tools.execute(call("Sleep", {{"seconds", 0}}), cancel).error);
  EXPECT_TRUE(tools.execute(call("Memory", {{"action", "list"}}), cancel).error);
  EXPECT_TRUE(tools.execute(call("EnterPlanMode", Json::object()), cancel).error);
}
TEST_F(Fixture, QuestionsReturnTextAndSuggestedChoicesAndRejectHeadlessAnswers) {
  unsigned questions = 0;
  Tools interactive(config, files, processes, http, clock, {},
                    [&](const std::string &question, const std::vector<std::string> &options) {
                      ++questions;
                      EXPECT_EQ(question, "Which library is available?");
                      EXPECT_EQ(options, (std::vector<std::string>{"libcurl", "other"}));
                      return "libcurl with OpenSSL";
                    });
  const auto request = call("AskUserQuestion", {{"question", "Which library is available?"},
                                                {"options", Json::array({"libcurl", "other"})}});
  const auto result = interactive.execute(request, cancel);
  ASSERT_FALSE(result.error);
  EXPECT_EQ(result.content, "libcurl with OpenSSL");
  EXPECT_EQ(questions, 1u);
  config.permission_mode = "bypass";
  Tools headless(config, files, processes, http, clock);
  EXPECT_TRUE(headless.execute(request, cancel).error);
}
TEST_F(Fixture, WebFetchPreservesPlainTextAndSearchIncludesResultUrls) {
  Tools tools(config, files, processes, http, clock);
  EXPECT_CALL(http, perform(_, _, _)).WillOnce(Return(HttpResponse{200, "a < b and c > d"}));
  EXPECT_EQ(
      tools.execute(call("WebFetch", {{"url", "https://example.invalid/text"}}), cancel).content,
      "a < b and c > d");
  EXPECT_CALL(http, perform(_, _, _))
      .WillOnce(Return(HttpResponse{
          200,
          R"({"Abstract":"answer","AbstractSource":"source","AbstractURL":"https://example.invalid/result"})"}));
  auto result = tools.execute(call("WebSearch", {{"query", "fixture"}}), cancel);
  ASSERT_FALSE(result.error);
  EXPECT_THAT(result.content, HasSubstr("https://example.invalid/result"));
}
TEST_F(Fixture, MemoryAndPlansPersistWithoutBackgroundCalls) {
  config.permission_mode = "accept-edits";
  Tools tools(config, files, processes, http, clock);
  EXPECT_CALL(http, perform(_, _, _)).Times(0);
  tools.use_session("fixture");
  EXPECT_FALSE(
      tools
          .execute(call("Memory",
                        {{"action", "write"}, {"name", "project.md"}, {"content", "durable fact"}}),
                   cancel)
          .error);
  EXPECT_FALSE(
      tools
          .execute(call("TodoWrite",
                        {{"todos", Json::array({{{"content", "task"}, {"status", "pending"}}})}}),
                   cancel)
          .error);
  Tools resumed(config, files, processes, http, clock);
  resumed.use_session("fixture");
  const auto instructions = resumed.instructions();
  EXPECT_THAT(instructions, HasSubstr("durable fact"));
  EXPECT_THAT(instructions, HasSubstr("pending"));
}
TEST_F(Fixture, BatchRestoresEarlierFilesWhenLaterWriteFails) {
  config.permission_mode = "accept-edits";
  config.cwd = files.canonical(config.cwd);
  config.roots = {config.cwd};
  write("a", "alpha");
  write("b", "beta");
  chmod((config.cwd / "a").c_str(), 0755);
  NiceMock<MockFiles> failing;
  ON_CALL(failing, canonical(_)).WillByDefault(Invoke([&](const fs::path &p) {
    return files.canonical(p);
  }));
  ON_CALL(failing, read(_, _)).WillByDefault(Invoke([&](const fs::path &p, size_t limit) {
    return files.read(p, limit);
  }));
  ON_CALL(failing, atomic_write(_, _, _))
      .WillByDefault(Invoke([&](const fs::path &p, const std::string &text, bool privacy) {
        files.atomic_write(p, text, privacy);
      }));
  EXPECT_CALL(failing, atomic_write(config.cwd / "a", _, false)).Times(2);
  EXPECT_CALL(failing, atomic_write(config.cwd / "b", _, false))
      .WillOnce(Throw(Error("disk full")));
  Tools tools(config, failing, processes, http, clock);
  for (auto *p : {"a", "b"})
    ASSERT_FALSE(tools.execute(call("Read", {{"file_path", p}}), cancel).error);
  auto result = tools.execute(
      call("BatchEdit",
           {{"edits",
             Json::array(
                 {{{"file_path", "a"}, {"old_string", "alpha"}, {"new_string", "changed"}},
                  {{"file_path", "b"}, {"old_string", "beta"}, {"new_string", "changed"}}})}}),
      cancel);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(files.read(config.cwd / "a"), "alpha");
  EXPECT_EQ(files.read(config.cwd / "b"), "beta");
  struct stat st {};
  ASSERT_EQ(stat((config.cwd / "a").c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0755);
}
TEST_F(Fixture, ContextCompactionUsesSameProviderAndRetainsCurrentRequest) {
  config.context_tokens = 512;
  StateStore state(config.home);
  auto s = state.create();
  for (int i = 0; i < 3; ++i) {
    codet::Message m;
    m.role = i % 2 ? "assistant" : "user";
    m.text = std::string(2000, 'x');
    s.messages.push_back(m);
  }
  state.save(s);
  Tools tools(config, files, processes, http, clock);
  StrictMock<MockProvider> provider;
  ProviderResult summary;
  summary.message.role = "assistant";
  summary.message.text = "preserved summary";
  ProviderResult final;
  final.message.role = "assistant";
  final.message.text = "done";
  EXPECT_CALL(provider, complete(_, _, _, _, _, _))
      .WillOnce(Return(summary))
      .WillOnce(Invoke([&](const Config &, const std::string &,
                           const std::vector<codet::Message> &messages, const Json &,
                           const Cancellation &, const ChunkSink &) {
        EXPECT_EQ(messages[0].role, "system");
        EXPECT_EQ(messages[1].text, "current objective");
        return final;
      }));
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(s, "current objective", cancel).text, "done");
  EXPECT_EQ(s.messages.size(), 3u);
}
TEST_F(Fixture, RequestLimitStopsWithoutAnExtraProviderCall) {
  config.max_turns = 1;
  write("a", "text");
  StateStore state(config.home);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  StrictMock<MockProvider> provider;
  ProviderResult result;
  result.message.role = "assistant";
  result.message.calls = {{"read", "Read", {{"file_path", "a"}}}};
  EXPECT_CALL(provider, complete(_, _, _, _, _, _)).WillOnce(Return(result));
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(s, "test", cancel), Error);
  const auto resumed = state.load(s.id);
  EXPECT_EQ(std::count_if(resumed.messages.begin(), resumed.messages.end(),
                          [](const codet::Message &m) { return m.role == "tool"; }),
            1);
}
TEST_F(Fixture, ReadVersionTracksReturnedBytesDespiteConcurrentWriter) {
  config.permission_mode = "accept-edits";
  write("a", "read value");
  NiceMock<MockFiles> racing;
  bool first = true;
  ON_CALL(racing, canonical(_)).WillByDefault(Invoke([&](const fs::path &p) {
    return files.canonical(p);
  }));
  ON_CALL(racing, read(_, _)).WillByDefault(Invoke([&](const fs::path &p, size_t limit) {
    const auto value = files.read(p, limit);
    if (first) {
      first = false;
      files.atomic_write(p, "concurrent value");
    }
    return value;
  }));
  EXPECT_CALL(racing, atomic_write(_, _, _)).Times(0);
  Tools tools(config, racing, processes, http, clock);
  ASSERT_FALSE(tools.execute(call("Read", {{"file_path", "a"}}), cancel).error);
  EXPECT_TRUE(
      tools.execute(call("Write", {{"file_path", "a"}, {"content", "overwrite"}}), cancel).error);
  EXPECT_EQ(files.read(config.cwd / "a"), "concurrent value");
}
TEST(ProcessIntegration, InteractivePtyInputAndBoundedFloodOutput) {
  Temp temp;
  NativeProcesses processes;
  Cancellation cancel;
  ProcessSpec spec;
  spec.cwd = temp.root;
  spec.argv = {"/bin/sh", "-c", "read value; printf '%s' \"$value\""};
  spec.pty = true;
  spec.timeout_ms = 2000;
  const auto id = processes.start(spec);
  processes.write(id, "hello\n");
  ProcessResult result;
  for (int i = 0; i < 200; ++i) {
    result = processes.poll(id);
    if (!result.running)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_FALSE(result.running);
  EXPECT_THAT(result.output, HasSubstr("hello"));
  processes.stop(id);
  spec.pty = false;
  spec.argv = {"/bin/sh", "-c", "while :; do printf flood; done"};
  spec.timeout_ms = 30;
  spec.output_limit = 256;
  result = processes.run(spec, cancel);
  EXPECT_EQ(result.exit_code, 124);
  EXPECT_TRUE(result.truncated);
  EXPECT_LE(result.output.size(), 256u);
}
TEST(ProcessIntegration, CancellationTerminatesAWaitingShell) {
  Temp temp;
  NativeProcesses processes;
  Cancellation cancel;
  ProcessSpec spec;
  spec.cwd = temp.root;
  spec.argv = {"/bin/sh", "-c", "sleep 10"};
  std::thread interrupt([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    cancel.requested = true;
  });
  try {
    processes.run(spec, cancel);
    ADD_FAILURE() << "Expected cancellation";
  } catch (const Error &e) {
    EXPECT_EQ(e.code, 130);
  }
  interrupt.join();
}
TEST(ProcessIntegration, StoppingJobAlsoTerminatesTermIgnoringDescendant) {
  Temp temp;
  NativeProcesses processes;
  ProcessSpec spec;
  spec.cwd = temp.root;
  spec.timeout_ms = 10000;
  spec.argv = {
      "/bin/sh", "-c",
      "trap '' TERM; (trap '' TERM; printf ready > ready; sleep 1; printf leaked > leaked) & wait"};
  const auto id = processes.start(spec);
  for (int i = 0; i < 500 && !fs::exists(temp.root / "ready"); ++i) {
    processes.poll(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(fs::exists(temp.root / "ready"));
  processes.stop(id);
  EXPECT_FALSE(processes.poll(id).running);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  EXPECT_FALSE(fs::exists(temp.root / "leaked"));
}
TEST(NetworkOptIn, TlsHandshake) {
  const auto url = env("CODET_TEST_TLS_URL");
  if (url.empty())
    GTEST_SKIP() << "Set CODET_TEST_TLS_URL explicitly for target TLS acceptance";
  ASSERT_EQ(url.rfind("https://", 0), 0u);
  CurlTransport http;
  Cancellation cancel;
  HttpRequest request;
  request.method = "GET";
  request.url = url;
  request.timeout_seconds = 30;
  request.max_bytes = 2 * 1024 * 1024;
  const auto result = http.perform(request, cancel, {});
  EXPECT_GE(result.status, 200);
  EXPECT_LT(result.status, 400);
  EXPECT_FALSE(result.body.empty());
}
TEST_F(Fixture, AgentExecutesToolAndPersistsOnlyCompletedTurn) {
  write("a", "fixture");
  StateStore state(config.home);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  StrictMock<MockProvider> provider;
  std::vector<Json> events;
  ProviderResult first;
  first.message.role = "assistant";
  first.message.calls = {{"call-1", "Read", {{"file_path", "a"}}}};
  ProviderResult last;
  last.message.role = "assistant";
  last.message.text = "done";
  EXPECT_CALL(provider, complete(_, _, _, _, _, _)).WillOnce(Return(first)).WillOnce(Return(last));
  Agent agent(config, provider, tools, state, [&](const Json &e) { events.push_back(e); });
  EXPECT_EQ(agent.run(s, "read a", cancel).text, "done");
  EXPECT_EQ(state.load(s.id).messages.size(), 4u);
  EXPECT_EQ(state.load(s.id).provider, config.provider);
  EXPECT_EQ(state.load(s.id).model, config.model);
  EXPECT_EQ(events.back()["type"], "turn.completed");
}
TEST(ProcessIntegration, OutputQuotingTimeoutAndPty) {
  Temp temp;
  NativeProcesses processes;
  Cancellation cancel;
  ProcessSpec spec;
  spec.cwd = temp.root;
  spec.argv = {"/bin/sh", "-c", "printf '%s' \"a'b\""};
  EXPECT_EQ(processes.run(spec, cancel).output, "a'b");
  spec.argv = {"/bin/sh", "-c", "sleep 10"};
  spec.timeout_ms = 30;
  EXPECT_EQ(processes.run(spec, cancel).exit_code, 124);
  spec.argv = {"/bin/sh", "-c", "test -t 1 && printf pty"};
  spec.pty = true;
  spec.timeout_ms = 2000;
  const auto result = processes.run(spec, cancel);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_THAT(result.output, HasSubstr("pty"));
}
} // namespace
