#include "codet/core.hpp"
#include "codet/process_control.hpp"
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>

using namespace codet;
using namespace testing;
namespace {
struct Remaining : Test {
  fs::path root;
  Config config;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    auto pattern = (fs::temp_directory_path() / "codet-remaining-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    root = mkdtemp(name.data());
    config.home = root / "state";
    config.cwd = root / "workspace";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.provider = "openai";
    config.model = "fixture";
    config.providers["openai"].api_key = "fake";
  }
  void TearDown() override {
    fs::remove_all(root);
  }
  ToolCall call(const std::string &name, const Json &args) {
    return {unique_id(), name, args};
  }
  void hook(const std::string &event, const std::string &command) {
    config.hooks = {{event, Json::array({{{"command", command}}})}};
  }
  ToolResult execute(const std::string &name, const Json &args) {
    Tools tools(config, files, processes, http, clock);
    return tools.execute(call(name, args), cancel);
  }
};
TEST_F(Remaining, RecoveryMovesResultsToOwningBatchAndIsIdempotentAcrossForks) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call("Bash", {{"command", "effect"}}), call("Write", {{"file_path", "a"}}),
                     call("Read", {{"file_path", "b"}})};
  codet::Message later;
  later.role = "user";
  later.text = "failed continuation";
  codet::Message completed;
  completed.role = "tool";
  completed.tool_call_id = assistant.calls[0].id;
  completed.text = "actual result";
  session.messages = {assistant, later, completed};
  state.save(session);
  state.tool_started(session.id, assistant.calls[0]);
  state.tool_finished(session.id, assistant.calls[0], Json(completed));
  state.tool_started(session.id, assistant.calls[1]);
  state.finish_turn(session.id, "failed");
  auto loaded = state.load(session.id);
  ASSERT_EQ(loaded.messages.size(), 5u);
  EXPECT_EQ(loaded.messages[1].text, "actual result");
  EXPECT_EQ(Json::parse(loaded.messages[2].text)["metadata"]["recovery"], "uncertain");
  EXPECT_EQ(Json::parse(loaded.messages[3].text)["metadata"]["recovery"], "not_started");
  EXPECT_EQ(loaded.messages[4].text, "failed continuation");
  const auto stable = Json(loaded.messages);
  state.reconcile(loaded);
  EXPECT_EQ(Json(loaded.messages), stable);
  EXPECT_EQ(Json(state.load(session.id, false).messages), stable);
  EXPECT_EQ(Json(state.load(session.id).messages), stable);
  auto branch = state.fork(loaded);
  EXPECT_EQ(Json(branch.messages), stable);
  EXPECT_EQ(Json(state.load(branch.id, false).messages), stable);
  state.reconcile(branch);
  EXPECT_EQ(Json(branch.messages), stable);
  EXPECT_EQ(Json(state.load(branch.id).messages), stable);
}
TEST_F(Remaining, RecoveryRejectsDuplicateIdsAndCorruptCompletedRecords) {
  StateStore state(config.home);
  auto s = state.create();
  codet::Message message;
  message.role = "assistant";
  auto tool = call("Read", {{"file_path", "a"}});
  message.calls = {tool, tool};
  s.messages = {message};
  EXPECT_THROW(state.reconcile(s), Error);
  message.calls = {tool};
  s.messages = {message};
  state.save(s);
  state.tool_started(s.id, tool);
  state.tool_finished(s.id, tool, {{"role", "user"}, {"tool_call_id", tool.id}});
  EXPECT_THROW(state.load(s.id), Error);
}
TEST_F(Remaining, FailedRecoveryTransactionLeavesBothTranscriptAndJournalUntouched) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call("Write", {{"file_path", "a"}})};
  session.messages = {assistant};
  state.save(session);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TRIGGER fail_recovery BEFORE INSERT ON tools BEGIN SELECT "
                         "RAISE(FAIL,'injected journal failure'); END;",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_THROW(state.reconcile(session), Error);
  EXPECT_EQ(session.messages.size(), 1u);
  sqlite3_stmt *query = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT count(*) FROM tools", -1, &query, nullptr), SQLITE_OK);
  ASSERT_EQ(sqlite3_step(query), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(query, 0), 0);
  sqlite3_finalize(query);
  ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER fail_recovery", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_EQ(state.load(session.id).messages.size(), 2u);
}
TEST_F(Remaining, AutomaticSqliteRollbackPreservesOriginalFailure) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call("Read", {{"file_path", "a"}})};
  session.messages = {assistant};
  state.save(session);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TRIGGER rollback_recovery BEFORE INSERT ON tools BEGIN SELECT "
                         "RAISE(ROLLBACK,'original rollback cause'); END;",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  try {
    state.reconcile(session);
    ADD_FAILURE();
  } catch (const Error &error) {
    EXPECT_THAT(error.what(), HasSubstr("original rollback cause"));
  }
  EXPECT_EQ(session.messages.size(), 1u);
  sqlite3_close(db);
}
TEST_F(Remaining, BoundedToolOutputRetainsItsSpillNoticeAndFullContent) {
  config.tool_output_limit = 256;
  files.atomic_write(config.cwd / "long", std::string(2000, 'x') + "\n");
  const auto result = execute("Read", {{"file_path", "long"}});
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_LE(result.content.size(), 256u);
  EXPECT_THAT(result.content, HasSubstr("Output truncated"));
  ASSERT_TRUE(result.metadata.contains("spill_path"));
  EXPECT_GT(files.read(result.metadata["spill_path"].get<std::string>()).size(), 2000u);
  EXPECT_NO_THROW(Json(result.content).dump());
}
TEST_F(Remaining, SnapshotLoadAndForkCannotModifyAnOwnedLiveBatch) {
  StateStore owner(config.home), observer(config.home);
  auto session = owner.create();
  auto lease = owner.acquire(session.id);
  EXPECT_NO_THROW(owner.acquire(session.id));
  EXPECT_THROW(observer.acquire(session.id), Error);
  EXPECT_THROW(observer.save(session), Error);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call("Bash", {{"command", "first"}}),
                     call("Write", {{"file_path", "second"}})};
  session.messages = {assistant};
  owner.save(session);
  owner.begin_turn(session.id);
  owner.tool_started(session.id, assistant.calls[0]);
  auto snapshot = observer.load(session.id);
  ASSERT_EQ(snapshot.messages.size(), 3u);
  const auto branch = observer.fork(snapshot);
  EXPECT_NE(branch.id, session.id);
  EXPECT_EQ(Json(branch.messages), Json(snapshot.messages));
  // No synthetic result or call reservation was written into the live owner's data.
  EXPECT_EQ(owner.load(session.id, false).messages.size(), 1u);
  codet::Message first;
  first.role = "tool";
  first.tool_call_id = assistant.calls[0].id;
  first.text = "completed";
  EXPECT_NO_THROW(owner.tool_finished(session.id, assistant.calls[0], Json(first)));
  EXPECT_NO_THROW(owner.tool_started(session.id, assistant.calls[1]));
  EXPECT_THROW(observer.reconcile(snapshot), Error);
  codet::Message second;
  second.role = "tool";
  second.tool_call_id = assistant.calls[1].id;
  second.text = "second completed";
  owner.tool_finished(session.id, assistant.calls[1], Json(second));
  codet::Message finished;
  finished.role = "assistant";
  finished.text = "owner finished";
  session.messages = {assistant, first, second, finished};
  owner.save(session);
  lease.reset();
  EXPECT_NO_THROW(observer.acquire(session.id));
  EXPECT_THROW(observer.reconcile(snapshot), Error);
  EXPECT_EQ(observer.load(session.id, false).messages.back().text, "owner finished");
}
TEST_F(Remaining, DuplicateToolReservationsFailClearlyWithoutOverwritingRecovery) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call("Read", {{"file_path", "a"}})};
  session.messages = {assistant};
  state.save(session);
  state.reconcile(session);
  try {
    state.tool_started(session.id, assistant.calls[0]);
    ADD_FAILURE();
  } catch (const Error &error) {
    EXPECT_THAT(error.what(), HasSubstr("Session modified by another process"));
  }
  EXPECT_EQ(Json::parse(state.load(session.id).messages[1].text)["metadata"]["recovery"],
            "not_started");
}
struct ScriptedProvider : Provider {
  std::function<ProviderResult(const std::vector<codet::Message> &)> next;
  ProviderResult complete(const Config &, const std::string &,
                          const std::vector<codet::Message> &messages, const Json &,
                          const Cancellation &, const ChunkSink &) override {
    return next(messages);
  }
};
TEST_F(Remaining, CancelledBatchCanContinueInSameSessionWithoutReplayingEffects) {
  StateStore state(config.home);
  auto session = state.create();
  Tools tools(config, files, processes, http, clock);
  ScriptedProvider provider;
  provider.next = [&](const auto &) {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.calls = {call("Bash", {{"command", "printf x >> effect"}}),
                            call("Sleep", {{"seconds", 1}}),
                            call("Write", {{"file_path", "never"}, {"content", "x"}})};
    return result;
  };
  Agent agent(config, provider, tools, state, {});
  std::thread interrupt([&] {
    for (int i = 0; i < 500 && !fs::exists(config.cwd / "effect"); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    cancel.requested = true;
  });
  try {
    agent.run(session, "first", cancel);
    ADD_FAILURE();
  } catch (const Error &e) {
    EXPECT_EQ(e.code, 130);
  }
  interrupt.join();
  cancel.requested = false;
  provider.next = [&](const auto &messages) {
    EXPECT_EQ(messages.size(), 6u);
    EXPECT_EQ(messages[1].role, "assistant");
    for (size_t i = 2; i <= 4; ++i)
      EXPECT_EQ(messages[i].role, "tool");
    EXPECT_EQ(messages[5].role, "user");
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = "continued";
    return result;
  };
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "continued");
  EXPECT_EQ(files.read(config.cwd / "effect"), "x");
  EXPECT_FALSE(fs::exists(config.cwd / "never"));
}
TEST_F(Remaining, UnexpectedProviderExceptionsBecomeRecoverableTurnErrors) {
  StateStore state(config.home);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  ScriptedProvider provider;
  provider.next = [](const auto &) -> ProviderResult {
    throw std::runtime_error(std::string("bad ") + char(0xff));
  };
  Agent agent(config, provider, tools, state,
              [](const Json &event) { EXPECT_NO_THROW(event.dump()); });
  EXPECT_THROW(agent.run(s, "first", cancel), Error);
  provider.next = [](const auto &) {
    ProviderResult r;
    r.message.role = "assistant";
    r.message.text = "ok";
    return r;
  };
  EXPECT_EQ(agent.run(s, "next", cancel).text, "ok");
}
TEST_F(Remaining, ExplicitPostStopIsJournaledBeforeHaltingTheBatch) {
  files.atomic_write(config.cwd / "a", "text");
  hook("PostToolUse", "printf '%s' '{\"continue\":false,\"stopReason\":\"halt\"}'");
  StateStore state(config.home);
  auto session = state.create();
  Tools tools(config, files, processes, http, clock);
  ScriptedProvider provider;
  provider.next = [&](const auto &) {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.calls = {call("Read", {{"file_path", "a"}}),
                            call("Write", {{"file_path", "never"}, {"content", "x"}})};
    return result;
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(session, "read then write", cancel), Error);
  EXPECT_FALSE(fs::exists(config.cwd / "never"));
  ASSERT_EQ(session.messages.size(), 4u);
  EXPECT_EQ(Json::parse(session.messages[2].text)["metadata"]["action_completed"], true);
  EXPECT_EQ(Json::parse(session.messages[3].text)["metadata"]["recovery"], "not_started");
  hook("Stop", "printf '%s' '{\"continue\":false,\"stopReason\":\"finished\"}'");
  int requests = 0;
  provider.next = [&](const auto &) {
    ++requests;
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = "done";
    return result;
  };
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
  EXPECT_EQ(requests, 1);
}
TEST_F(Remaining, PathDenialsCoverAliasesCanonicalTargetsAndWildcardPrefixes) {
  fs::create_directories(config.cwd / "secrets");
  files.atomic_write(config.cwd / "secrets/key", "secret");
  fs::create_directory_symlink(config.cwd / "secrets", config.cwd / "alias");
  for (const auto &rule :
       {"Read(secrets/key)", "Read(./secrets/key)", "Read(secrets/*)", "Read(alias/*)"}) {
    config.deny = {rule};
    for (const auto &path : {"secrets/key", "./secrets/key", "alias/key", "alias/../secrets/key"})
      EXPECT_TRUE(execute("Read", {{"file_path", path}}).error) << rule << " " << path;
  }
}
TEST_F(Remaining, DeniedPathsPreventHooksAndRewrittenInputsAreRechecked) {
  files.atomic_write(config.cwd / "public", "public");
  files.atomic_write(config.cwd / "secret", "secret");
  config.deny = {"Read(secret)"};
  hook("PreToolUse", "printf ran > hook-ran");
  EXPECT_TRUE(execute("Read", {{"file_path", "./secret"}}).error);
  EXPECT_FALSE(fs::exists(config.cwd / "hook-ran"));
  hook("PreToolUse", "printf '%s' "
                     "'{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\",\"updatedInput\":{"
                     "\"file_path\":\"./secret\"}}}'");
  EXPECT_TRUE(execute("Read", {{"file_path", "public"}}).error);
  config.deny = {"ApplyPatch(secret)", "BatchEdit(secret)"};
  EXPECT_TRUE(execute("ApplyPatch",
                      {{"patch", "*** Begin Patch\n*** Add File: ./secret\n+x\n*** End Patch\n"}})
                  .error);
  EXPECT_TRUE(execute("BatchEdit", {{"edits", Json::array({{{"file_path", "./secret"},
                                                            {"old_string", "secret"},
                                                            {"new_string", "x"}}})}})
                  .error);
}
TEST_F(Remaining, GlobFiltersItsOwnDeniedFilesAndSubtreesOnly) {
  fs::create_directories(config.cwd / "hidden");
  files.atomic_write(config.cwd / "hidden/key", "secret");
  files.atomic_write(config.cwd / "public", "public");
  config.deny = {"Glob(hidden)", "Read(public)"};
  auto result = execute("Glob", {{"pattern", "*"}});
  EXPECT_FALSE(result.error);
  EXPECT_THAT(result.content, HasSubstr("public"));
  EXPECT_THAT(result.content, Not(HasSubstr("hidden")));
  config.deny = {"Glob(hidden/*)"};
  result = execute("Glob", {{"pattern", "*"}});
  EXPECT_THAT(result.content, Not(HasSubstr("hidden/key")));
}
struct CountingFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t canonical_calls = 0;
  std::string read(const fs::path &p, size_t cap) const override {
    return native.read(p, cap);
  }
  std::string read_prefix(const fs::path &p, size_t cap, bool &cut) const override {
    return native.read_prefix(p, cap, cut);
  }
  void atomic_write(const fs::path &p, const std::string &text, bool privacy) override {
    native.atomic_write(p, text, privacy);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    ++canonical_calls;
    return native.canonical(p);
  }
};
TEST_F(Remaining, GlobAvoidsRepeatedCanonicalizationWithoutApplicableDenials) {
  config.permission_mode = "default";
  config.deny = {"Read(secret*)"};
  for (int i = 0; i < 100; ++i)
    files.atomic_write(config.cwd / ("file-" + std::to_string(i)), "x");
  CountingFiles counted;
  Tools tools(config, counted, processes, http, clock);
  const auto result = tools.execute(call("Glob", {{"pattern", "*"}}), cancel);
  ASSERT_FALSE(result.error);
  EXPECT_LT(counted.canonical_calls, 20u);
  EXPECT_THAT(result.content, HasSubstr("file-99"));
  config.deny = {"Glob(file-99)"};
  counted.canonical_calls = 0;
  const auto filtered = tools.execute(call("Glob", {{"pattern", "*"}}), cancel);
  EXPECT_FALSE(filtered.error);
  EXPECT_THAT(filtered.content, Not(HasSubstr("file-99")));
  EXPECT_GT(counted.canonical_calls, 20u);
}
TEST_F(Remaining, HookFailurePlaintextStderrAndExplicitDenialsFollowEventSemantics) {
  Tools tools(config, files, processes, http, clock);
  for (const auto &event : {"SessionStart", "UserPromptSubmit"}) {
    hook(event, "printf context; printf warning >&2");
    const auto result = tools.run_hooks(event, Json::object(), cancel);
    EXPECT_EQ(result.additional_context, "context");
    EXPECT_FALSE(result.blocked);
    EXPECT_FALSE(result.diagnostics.empty());
  }
  for (const auto &command :
       {"printf '{bad'", "exit 1", "exit 2", "printf ignored", "printf x >&2; exit 3"}) {
    hook("PreToolUse", command);
    const auto result = tools.run_hooks("PreToolUse", Json::object(), cancel);
    EXPECT_FALSE(result.blocked);
    EXPECT_FALSE(result.stopped);
  }
  hook("PreToolUse", "printf reason >&2; exit 2");
  EXPECT_TRUE(tools.run_hooks("PreToolUse", Json::object(), cancel).blocked);
  hook("SessionStart", "printf reason >&2; exit 2");
  EXPECT_FALSE(tools.run_hooks("SessionStart", Json::object(), cancel).blocked);
  hook("PreCompact", "printf '%s' '{\"continue\":false,\"stopReason\":\"stop\"}'");
  EXPECT_TRUE(tools.run_hooks("PreCompact", Json::object(), cancel).stopped);
  hook("SessionEnd", "printf '%s' '{\"continue\":false}'");
  EXPECT_FALSE(tools.run_hooks("SessionEnd", Json::object(), cancel).stopped);
}
TEST_F(Remaining, FailedApprovalHookDoesNotGrantAndPostBlockPreservesCompletedEffects) {
  config.permission_mode = "default";
  hook("PermissionRequest", "exit 1");
  EXPECT_TRUE(execute("Write", {{"file_path", "a"}, {"content", "x"}}).error);
  config.permission_mode = "bypass";
  hook("PostToolUse", "printf reason >&2; exit 2");
  const auto result = execute("Write", {{"file_path", "a"}, {"content", "x"}});
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.metadata["action_completed"], true);
  EXPECT_EQ(files.read(config.cwd / "a"), "x");
}
TEST_F(Remaining, InvalidHookErrorBytesRemainSerializableAndBounded) {
  files.atomic_write(config.cwd / "a", "text");
  hook("PreToolUse", "printf '\\377' >&2; exit 2");
  const auto result = execute("Read", {{"file_path", "a"}});
  EXPECT_TRUE(result.error);
  EXPECT_NO_THROW(Json(result.content).dump());
  EXPECT_THAT(result.content, HasSubstr("\xef\xbf\xbd"));
  EXPECT_LE(result.content.size(), config.tool_output_limit);
}
TEST_F(Remaining, InvalidPathMetadataIsSanitizedWithoutChangingFilesystemBytes) {
  const auto name = std::string("raw-") + char(0xff);
  const auto result = execute("Write", {{"file_path", name}, {"content", "x"}});
  EXPECT_NO_THROW(result.metadata.dump());
#ifdef __APPLE__
  // Darwin filesystems reject ill-formed UTF-8 path bytes; report the OS error safely.
  EXPECT_TRUE(result.error);
  EXPECT_NO_THROW(Json(result.content).dump());
#else
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_TRUE(fs::exists(config.cwd / name));
#endif
  EXPECT_FALSE(fs::exists(config.cwd / safe_text(name)));
}
TEST_F(Remaining, CompletedOutputHistoryHasAnAggregateByteBudget) {
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "dd if=/dev/zero bs=1024 count=512 2>/dev/null"};
  spec.output_limit = 524288;
  for (int i = 0; i < 10; ++i) {
    const auto id = processes.start(spec, cancel);
    for (int n = 0; n < 1000; ++n) {
      if (!processes.poll(id).running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
  size_t bytes = 0;
  for (const auto &job : processes.list()) {
    EXPECT_FALSE(job.running);
    bytes += processes.poll(job.id).output.size();
  }
  EXPECT_LE(bytes, 4u * 1024 * 1024);
  EXPECT_LE(processes.list().size(), 8u);
  const auto retained = processes.list().size();
  spec.argv = {"/bin/sh", "-c", "printf foreground"};
  EXPECT_EQ(processes.run(spec, cancel).output, "foreground");
  EXPECT_EQ(processes.list().size(), retained);
}
TEST_F(Remaining, OversizedInstructionsMemoryAndIncludesUseBoundedReads) {
  files.atomic_write(config.cwd / "AGENTS.md", "@include more.md\n" + std::string(80000, 'x'));
  files.atomic_write(config.cwd / "more.md", "included\n@include AGENTS.md\n");
  fs::create_directories(config.home / "memory");
  files.atomic_write(config.home / "memory/a.md", std::string(80000, 'a'));
  files.atomic_write(config.home / "memory/b.md", "later memory");
  Tools tools(config, files, processes, http, clock);
  auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr("included"));
  EXPECT_THAT(text, Not(HasSubstr("later memory")));
  EXPECT_LT(text.size(), config.project_doc_max_bytes + config.memory_context_max_bytes + 100);
  EXPECT_EQ(files.read(config.cwd / "AGENTS.md").size(), 80017u);
  config.project_doc_max_bytes = 0;
  config.memory_context_max_bytes = 0;
  text = tools.instructions();
  EXPECT_THAT(text, Not(HasSubstr("Instructions:")));
  EXPECT_THAT(text, Not(HasSubstr("Memory:")));
  bool truncated = false;
  files.atomic_write(config.cwd / "utf8", "a\xc3\xa9");
  EXPECT_EQ(files.read_prefix(config.cwd / "utf8", 2, truncated), "a");
  EXPECT_TRUE(truncated);
  EXPECT_THROW(files.read(config.cwd / "utf8", 2), Error);
}
TEST_F(Remaining, ParameterlessGoogleToolsOmitParametersAndTimeoutDefaultsRemainExplicit) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  config.provider = "google";
  config.providers["google"].api_key = "fake";
  Tools tools(config, files, processes, http, clock);
  auto schemas = tools.schemas();
  schemas.push_back({{"name", "StructuredOutput"},
                     {"description", "output"},
                     {"input_schema", {{"type", "object"}, {"properties", Json::object()}}}});
  const auto request = provider.request(config, "system", {}, schemas);
  EXPECT_EQ(request.timeout_seconds, 0);
  EXPECT_EQ(request.idle_timeout_seconds, 120);
  EXPECT_EQ(request.connect_timeout_seconds, 20);
  const auto body = Json::parse(request.body);
  for (const auto &tool : body["tools"][0]["functionDeclarations"]) {
    const auto name = tool["name"].get<std::string>();
    EXPECT_EQ(tool.contains("parameters"),
              name != "TaskList" && name != "EnterPlanMode" && name != "ExitPlanMode");
  }
  files.atomic_write(config.home / "config.toml",
                     "request_timeout_seconds=7\nstream_idle_timeout_seconds=9\nconnect_timeout_"
                     "seconds=3\nproject_doc_max_bytes=0\nmemory_context_max_bytes=0\n");
  const auto loaded = load_config(config.home, config.cwd, "", {}, files);
  EXPECT_EQ(loaded.request_timeout, 7);
  EXPECT_EQ(loaded.stream_idle_timeout, 9);
  EXPECT_EQ(loaded.connect_timeout, 3);
  EXPECT_THROW(load_config(config.home, config.cwd, "", {"project_doc_max_bytes=1048577"}, files),
               Error);
}
TEST_F(Remaining, PatchMatchesReferenceChunksAndPreservesContextEndings) {
  const std::vector<std::tuple<std::string, std::string, std::string>> cases = {
      {std::string(262144, 'x') + "  \n", "@@\n-" + std::string(262144, 'x') + "\n+short\n",
       "short\n"},
      {"one\n", "@@\n+two\n", "one\ntwo\n"},
      {"old\nold\n", "@@\n-old\n+first\n@@\n-old\n+second\n", "first\nsecond\n"},
      {"anchor\nold\nanchor\nold\n", "@@ anchor\n-old\n+new\n", "anchor\nnew\nanchor\nold\n"},
      {"old\nold\n", "@@\n-old\n+last\n*** End of File\n", "old\nlast\n"},
      {"old", "-old\n+new\n", "new\n"},
      {"keep  \r\nold\nrest\r", "@@\n keep\n-old\n+new\n", "keep  \r\nnew\r\nrest\r"},
      {"  fancy \xe2\x80\x94 text\n", "@@\n-fancy - text\n+plain\n", "plain\n"}};
  Tools tools(config, files, processes, http, clock);
  for (const auto &entry : cases) {
    files.atomic_write(config.cwd / "a", std::get<0>(entry));
    ASSERT_FALSE(tools.execute(call("Read", {{"file_path", "a"}}), cancel).error);
    const auto result =
        tools.execute(call("ApplyPatch", {{"patch", "*** Begin Patch\n*** Update File: a\n" +
                                                        std::get<1>(entry) + "*** End Patch\n"}}),
                      cancel);
    ASSERT_FALSE(result.error) << result.content;
    EXPECT_EQ(files.read(config.cwd / "a"), std::get<2>(entry));
  }
}
struct DelayedControl : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  mutable int offset = 0;
  bool delay_wait = false, delay_watch = false;
  Cancellation *cancel_wait = nullptr, *cancel_watch = nullptr;
  int wait_ready(int fd, int timeout) override {
    if (cancel_wait) {
      cancel_wait->requested = true;
      return 0;
    }
    if (delay_wait) {
      delay_wait = false;
      offset += 6000;
      return 0;
    }
    return native->wait_ready(fd, timeout);
  }
  std::chrono::steady_clock::time_point now() const override {
    return native->now() + std::chrono::milliseconds(offset);
  }
  int watch(pid_t pid) override {
    const auto fd = native->watch(pid);
    if (cancel_watch)
      cancel_watch->requested = true;
    if (delay_watch)
      offset += 6000;
    return fd;
  }
  bool exited(pid_t pid, int observer) override {
    return native->exited(pid, observer);
  }
  void signal(pid_t pid, int sig) override {
    native->signal(pid, sig);
  }
  int reap(pid_t pid) override {
    return native->reap(pid);
  }
};
TEST_F(Remaining, BackgroundStartupTimeoutHasStatusAndRunsFailureHookWithoutAHandle) {
  auto control = std::make_shared<DelayedControl>();
  control->delay_wait = true;
  NativeProcesses native(control);
  hook("PostToolUseFailure", "printf failure > failure-hook");
  Tools tools(config, files, native, http, clock);
  const auto result = tools.execute(
      call("TaskCreate", {{"command", "printf effect > never"}, {"timeout_ms", 30}}), cancel);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(result.metadata["exit_code"], 124);
  EXPECT_TRUE(native.list().empty());
  EXPECT_FALSE(fs::exists(config.cwd / "never"));
  EXPECT_EQ(files.read(config.cwd / "failure-hook"), "failure");
}
TEST_F(Remaining, StartupDeadlinePreventsExecutionBeforeAndAfterWatcherRegistration) {
  for (bool after_watch : {false, true}) {
    auto control = std::make_shared<DelayedControl>();
    control->delay_wait = !after_watch;
    control->delay_watch = after_watch;
    NativeProcesses native(control);
    ProcessSpec spec;
    spec.cwd = config.cwd;
    spec.timeout_ms = 30;
    spec.argv = {"/bin/sh", "-c", "printf side-effect > never"};
    EXPECT_EQ(native.run(spec, cancel).exit_code, 124);
    EXPECT_FALSE(fs::exists(config.cwd / "never"));
    control->offset = 0;
    control->delay_wait = !after_watch;
    EXPECT_THROW(native.start(spec, cancel), ProcessTimeout);
    EXPECT_FALSE(fs::exists(config.cwd / "never"));
  }
  auto control = std::make_shared<DelayedControl>();
  control->delay_wait = true;
  NativeProcesses native(control);
  ProcessSpec spec;
  spec.timeout_ms = 10000;
  spec.argv = {"/bin/sh", "-c", "true"};
  try {
    native.start(spec, cancel);
    ADD_FAILURE();
  } catch (const ProcessTimeout &) {
    ADD_FAILURE();
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("setup timed out"));
  }
}
TEST_F(Remaining, StartupCancellationNeverReleasesTheCommand) {
  for (bool watched : {false, true}) {
    auto control = std::make_shared<DelayedControl>();
    cancel.requested = false;
    if (watched)
      control->cancel_watch = &cancel;
    else
      control->cancel_wait = &cancel;
    NativeProcesses native(control);
    ProcessSpec spec;
    spec.cwd = config.cwd;
    spec.argv = {"/bin/sh", "-c", "printf effect > never"};
    try {
      native.run(spec, cancel);
      ADD_FAILURE();
    } catch (const Error &error) {
      EXPECT_EQ(error.code, 130);
    }
    EXPECT_FALSE(fs::exists(config.cwd / "never"));
  }
}
TEST_F(Remaining, CompletedJobsDoNotExhaustActiveLimitAndHistoryIsBounded) {
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "printf done"};
  std::string oldest;
  for (int i = 0; i < 70; ++i) {
    const auto id = processes.start(spec, cancel);
    if (!i)
      oldest = id;
    ProcessResult result;
    for (int n = 0; n < 500; ++n) {
      result = processes.poll(id);
      if (!result.running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_FALSE(result.running);
    ASSERT_EQ(result.output, "done");
  }
  EXPECT_LE(processes.list().size(), 64u);
  EXPECT_THROW(processes.poll(oldest), Error);
  auto snapshot = execute("TaskList", Json::object());
  ASSERT_FALSE(snapshot.error);
  EXPECT_LE(Json::parse(snapshot.content).size(), 64u);
  EXPECT_THAT(execute("TaskOutput", {{"task_id", oldest}}).content, HasSubstr("expired"));
  EXPECT_EQ(processes.run(spec, cancel).output, "done");
  spec.separate_stderr = true;
  spec.argv = {"/bin/sh", "-c", "printf stdout; printf stderr >&2"};
  const auto result = processes.run(spec, cancel);
  EXPECT_EQ(result.output, "stdout");
  EXPECT_EQ(result.stderr_output, "stderr");
}
} // namespace
