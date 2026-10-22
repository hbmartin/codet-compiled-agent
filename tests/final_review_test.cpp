#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/patch.hpp"
#include "codet/process_control.hpp"
#include "core_tools.hpp"
#include "temp.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <random>
#include <sqlite3.h>
#include <thread>
using namespace codet;
using namespace testing;
namespace {
struct FinalReview : Test {
  TempDirectory temp;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  Config config;
  void SetUp() override {
    config.cwd = temp.root / "workspace";
    config.home = temp.root / "state";
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.tool_output_limit = 512;
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
  }
  ProcessSpec shell(const std::string &command) {
    ProcessSpec spec;
    spec.cwd = config.cwd;
    spec.argv = {"/bin/sh", "-c", command};
    spec.timeout_ms = 10000;
    return spec;
  }
};
TEST_F(FinalReview, LiteralRootsAndAuthoredGlobsRemainDistinctAcrossAliases) {
  for (const auto &name : {"work[1]", "work*", "work?", "work\\dir"}) {
    config.cwd = temp.root / name;
    config.roots = {config.cwd};
    config.permission_mode = "default";
    fs::create_directories(config.cwd / "secrets");
    files.atomic_write(config.cwd / "secrets/key.pem", "private");
    config.set_deny({"read_file(secrets/key.pem)"});
    Tools tools(config, files, processes, http, clock);
    for (const auto &alias : std::vector<std::string>{"secrets/key.pem", "./secrets/key.pem",
                                                      "secrets/../secrets/key.pem",
                                                      (config.cwd / "secrets/key.pem").string()}) {
      auto result =
          execute_core(tools, config, {unique_id(), "read_file", {{"path", alias}}}, cancel);
      EXPECT_TRUE(result.error) << name << " " << alias;
    }
    config.set_deny({"read_file(" + (config.cwd / "secrets/*").string() + ")"});
    EXPECT_TRUE(execute_core(tools, config,
                             {unique_id(), "read_file", {{"path", "./secrets/key.pem"}}}, cancel)
                    .error);
    config.permission_mode = "bypass";
    config.set_deny({"read_file(*key.pem)"});
    EXPECT_TRUE(execute_core(tools, config,
                             {unique_id(), "read_file", {{"path", "./secrets/key.pem"}}}, cancel)
                    .error);
    config.set_deny({});
    EXPECT_FALSE(execute_core(tools, config,
                              {unique_id(), "read_file", {{"path", "./secrets/key.pem"}}}, cancel)
                     .error);
  }
}
TEST_F(FinalReview, CachedRulesRecheckSymlinkPrefixesAndConfiguration) {
  fs::create_directories(config.cwd / "one");
  fs::create_directories(config.cwd / "two");
  files.atomic_write(config.cwd / "one/key", "one");
  files.atomic_write(config.cwd / "two/key", "two");
  fs::create_directory_symlink(config.cwd / "one", config.cwd / "alias");
  config.set_deny({"read_file(alias/*)"});
  Tools tools(config, files, processes, http, clock);
  EXPECT_TRUE(execute_core(tools, config, {"1", "read_file", {{"path", "one/key"}}}, cancel).error);
  fs::remove(config.cwd / "alias");
  fs::create_directory_symlink(config.cwd / "two", config.cwd / "alias");
  EXPECT_TRUE(execute_core(tools, config, {"2", "read_file", {{"path", "two/key"}}}, cancel).error);
  EXPECT_FALSE(
      execute_core(tools, config, {"3", "read_file", {{"path", "one/key"}}}, cancel).error);
  config.set_deny({"read_file(one/key)"});
  EXPECT_TRUE(execute_core(tools, config, {"4", "read_file", {{"path", "one/key"}}}, cancel).error);
}
TEST_F(FinalReview, SeparateHookChannelsCannotStarveControlOutput) {
  std::vector<std::string> diagnostics;
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { diagnostics.push_back(s); });
  config.hooks["PreToolUse"] =
      Json::array({{{"command", "printf '%01000d' 0; printf blocked >&2; exit 2"}}});
  const auto blocked = tools.run_hooks("PreToolUse", {{"tool_name", "read_file"}}, cancel);
  EXPECT_TRUE(blocked.blocked);
  EXPECT_EQ(blocked.reason, "blocked");
  EXPECT_THAT(diagnostics, Contains(HasSubstr("stdout capture truncated")));
  config.hooks["PreToolUse"] = Json::array(
      {{{"command",
         "printf '%01000d' 0 >&2; printf '%s' '{\"decision\":\"block\",\"reason\":\"denied\"}'"}}});
  const auto json = tools.run_hooks("PreToolUse", {{"tool_name", "read_file"}}, cancel);
  EXPECT_TRUE(json.blocked);
  EXPECT_EQ(json.reason, "denied");
  EXPECT_THAT(diagnostics, Contains(HasSubstr("stderr capture truncated")));
  auto spec = shell("printf '%01000d' 0; printf '%01000d' 0 >&2");
  spec.separate_stderr = true;
  spec.output_limit = 257;
  const auto both = processes.run(spec, cancel);
  EXPECT_EQ(both.output.size(), 257u);
  EXPECT_EQ(both.stderr_output.size(), 257u);
  EXPECT_TRUE(both.stdout_truncated);
  EXPECT_TRUE(both.stderr_truncated);
  config.hooks["PreToolUse"] = Json::array(
      {{{"command",
         "printf '{\"decision\":\"block\",\"reason\":\"'; printf '%01000d' 0; printf '\"}'"}}});
  EXPECT_THROW(tools.run_hooks("PreToolUse", Json::object(), cancel), HookFailure);
  config.hooks["PreToolUse"][0]["on_error"] = "warn";
  EXPECT_FALSE(tools.run_hooks("PreToolUse", Json::object(), cancel).blocked);
  EXPECT_THAT(diagnostics, Contains(HasSubstr("truncated JSON")));
}
TEST_F(FinalReview, HookSourceMatchersPreserveInvocationContextOrder) {
  config.hooks["SessionStart"] =
      Json::array({{{"matcher", "startup"}, {"command", "printf startup"}},
                   {{"matcher", "resume"}, {"command", "printf resume"}},
                   {{"matcher", "fork"}, {"command", "printf fork"}},
                   {{"matcher", ""}, {"command", "printf always"}}});
  Tools tools(config, files, processes, http, clock);
  for (const auto &source : {"startup", "resume", "fork"})
    EXPECT_EQ(tools.run_hooks("SessionStart", {{"source", source}}, cancel).additional_context,
              std::string(source) + "\nalways");
}
TEST_F(FinalReview, CrLfProtocolNeverChangesFileNamesAndPreservesContentWhitespace) {
  Tools tools(config, files, processes, http, clock);
  auto patch = [&](const std::string &body) {
    return execute_core(tools, config,
                        {unique_id(),
                         "apply_patch",
                         {{"patch", "*** Begin Patch\r\n" + body + "*** End Patch\r\n"}}},
                        cancel);
  };
  ASSERT_FALSE(patch("*** Add File: a\r\n+old  \r\n").error);
  EXPECT_TRUE(fs::exists(config.cwd / "a"));
  EXPECT_FALSE(fs::exists(config.cwd / "a\r"));
  EXPECT_EQ(files.read(config.cwd / "a"), "old  \n");
  ASSERT_FALSE(execute_core(tools, config, {"read", "read_file", {{"path", "a"}}}, cancel).error);
  ASSERT_FALSE(patch("*** Update File: a\r\n*** Move to: b\r\n@@\r\n-old  \r\n+new  \r\n").error);
  EXPECT_EQ(files.read(config.cwd / "b"), "new  \n");
  ASSERT_FALSE(execute_core(tools, config, {"read2", "read_file", {{"path", "b"}}}, cancel).error);
  ASSERT_FALSE(patch("*** Delete File: b\r\n").error);
  EXPECT_FALSE(fs::exists(config.cwd / "b"));
}
TEST_F(FinalReview, LoneCarriageReturnsAreContentAndUntouchedEofStaysUnterminated) {
  size_t index = 0;
  EXPECT_EQ(patch_update("one\rtwo\nold\n", {"@@", "-old", "+new", "*** End Patch"}, index, cancel),
            "one\rtwo\nnew\n");
  index = 0;
  EXPECT_EQ(patch_update("keep\rinside\r\nold\r\nrest\r", {"@@", "-old", "+new", "*** End Patch"},
                         index, cancel),
            "keep\rinside\r\nnew\r\nrest\r");
  index = 0;
  EXPECT_EQ(patch_update("old", {"@@", "+new", "*** End Patch"}, index, cancel), "old\nnew\n");
}
TEST_F(FinalReview, ExactIdsWinAndLockedReloadsNeverResolveNames) {
  StateStore state(config.home);
  const auto a = state.create();
  const auto b = state.create(a.id);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  const auto update = "UPDATE sessions SET updated=2000000000 WHERE id='" + a.id +
                      "'; UPDATE sessions SET updated=2000000001 WHERE id='" + b.id + "'";
  ASSERT_EQ(sqlite3_exec(db, update.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_EQ(state.load(a.id).id, a.id);
  EXPECT_EQ(state.load_id(a.id).id, a.id);
  EXPECT_EQ(state.last().id, b.id);
  auto named = state.create("only-name");
  EXPECT_EQ(state.load("only-name").id, named.id);
  EXPECT_THROW(state.load_id("only-name"), Error);
}
struct ProviderStub : Provider {
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink & = {}) override {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = "okay";
    return result;
  }
};
TEST_F(FinalReview, AgentOwnershipReloadUsesExactIdWhenAnotherSessionHasThatName) {
  StateStore state(config.home);
  auto session = state.create();
  auto other = state.create(session.id);
  other.messages.push_back({"user", "other session"});
  state.save(other);
  Tools tools(config, files, processes, http, clock);
  ProviderStub provider;
  Agent agent(config, provider, tools, state, {});
  const auto original_id = session.id;
  EXPECT_EQ(agent.run(session, "simple", cancel).text, "okay");
  EXPECT_EQ(session.id, original_id);
  const auto unchanged = state.load_id(other.id);
  ASSERT_EQ(unchanged.messages.size(), 1u);
  EXPECT_EQ(unchanged.messages.front().text, "other session");
}
TEST_F(FinalReview, FailedExpansionLeavesNoUnpersistedContextOrFalseRecoveryWarning) {
  config.hooks["UserPromptSubmit"] = Json::array({{{"command", "printf context"}}});
  std::vector<std::string> diagnostics;
  DiagnosticSink sink = [&](const auto &s) { diagnostics.push_back(s); };
  Tools tools(config, files, processes, http, clock, {}, {}, sink);
  StateStore state(config.home);
  auto session = state.create();
  ProviderStub provider;
  Agent agent(config, provider, tools, state, {}, sink);
  EXPECT_THROW(agent.run(session, "/unknown", cancel), Error);
  EXPECT_TRUE(session.messages.empty());
  EXPECT_TRUE(state.load_id(session.id).messages.empty());
  EXPECT_TRUE(diagnostics.empty());
  EXPECT_EQ(agent.run(session, "simple", cancel).text, "okay");
}
TEST_F(FinalReview, BoundedReadsRetainStrictSizeAndRegularFilePolicies) {
  files.atomic_write(config.cwd / "a", "abcdef");
  bool truncated = false;
  EXPECT_THROW(files.read(config.cwd / "a", 5), Error);
  EXPECT_EQ(files.read(config.cwd / "a", 6), "abcdef");
  EXPECT_EQ(files.read_prefix(config.cwd / "a", 3, truncated), "abc");
  EXPECT_TRUE(truncated);
  EXPECT_EQ(files.read_prefix(config.cwd / "a", 0, truncated), "");
  EXPECT_TRUE(truncated);
  files.atomic_write(config.cwd / "utf8", "\xe9\x9b\xaa-tail");
  EXPECT_EQ(files.read_prefix(config.cwd / "utf8", 2, truncated), "");
  EXPECT_TRUE(truncated);
  fs::create_symlink(config.cwd / "a", config.cwd / "alias");
  EXPECT_THROW(files.read(config.cwd / "alias"), Error);
  EXPECT_THROW(files.read_prefix(config.cwd / "alias", 10, truncated), Error);
  EXPECT_THROW(files.read_prefix(config.cwd, 10, truncated), Error);
}
TEST(Sanitization, MatchesPinnedJsonReplacementForEveryBytePairAndRandomSequences) {
  auto reference = [](const std::string &s) {
    return Json::parse(Json(s).dump(-1, ' ', false, Json::error_handler_t::replace))
        .get<std::string>();
  };
  for (unsigned i = 0; i < 65536; ++i) {
    const std::string s{static_cast<char>(i >> 8), static_cast<char>(i & 255)};
    ASSERT_EQ(safe_text(s), reference(s)) << i;
  }
  std::mt19937 generator(17);
  for (int i = 0; i < 10000; ++i) {
    std::string s;
    for (unsigned j = 0; j < generator() % 40; ++j)
      s += static_cast<char>(generator() & 255);
    ASSERT_EQ(safe_text(s), reference(s));
  }
  for (const auto &s :
       {std::string("\xf0\x90\x80"), std::string("\xed\xa0\x80"), std::string("雪🌲")})
    EXPECT_EQ(safe_text(s), reference(s));
}
TEST(Sanitization, PreservesJsonRepresentationAndInvalidKeyCollisionBehavior) {
  Json value = {{"valid", Json::array({"雪", std::string("bad\xff"), 7, -4, true, nullptr})}};
  value[std::string(1, '\xff')] = "last";
  value[std::string(1, '\xfe')] = "earlier";
  value["nan"] = std::numeric_limits<double>::quiet_NaN();
  value["binary"] = Json::binary({0, 255});
  const auto reference = Json::parse(value.dump(-1, ' ', false, Json::error_handler_t::replace));
  EXPECT_EQ(safe_json(value), reference);
  EXPECT_EQ(safe_json(value).dump(), reference.dump());
}
TEST(FileDiagnostics, FailedNoticesAreHeldUntilRestorationAndEscapedWithoutTruncation) {
  FileDiagnosticQueue queue;
  const std::string path = std::string(2048, 'p') + "\n\x1b\\\xff";
  queue.submit(std::make_unique<FileNotice>(path, FileNoticeKind::repair_backup));
  auto confirmed = std::make_unique<FileNotice>("confirmed-path", FileNoticeKind::tool_result);
  confirmed->confirmed = true;
  queue.submit(std::move(confirmed));
  int attempts = 0;
  testing::internal::CaptureStderr();
  queue.deliver([&](const auto &) {
    ++attempts;
    if (attempts == 2)
      std::cerr << "partial renderer output\n";
    throw std::bad_alloc();
  });
  const auto before_restoration = testing::internal::GetCapturedStderr();
  EXPECT_EQ(before_restoration, "partial renderer output\n");
  queue.deliver([&](const auto &) { ++attempts; });
  EXPECT_EQ(attempts, 2);
  testing::internal::CaptureStderr();
  queue.flush();
  queue.flush();
  const auto emergency = testing::internal::GetCapturedStderr();
  EXPECT_EQ(emergency,
            "Session repair backup candidate (publication unconfirmed): " + std::string(2048, 'p') +
                "\\x0A\\x1B\\x5C\\xFF\n" + "Uncommitted tool result preserved at confirmed-path\n");
}
TEST(FileDiagnostics, WorkerTransfersNoticesAndSuccessfulRenderingNeedsNoEmergencyOutput) {
  FileDiagnosticQueue queue;
  std::thread worker([&] {
    for (int i = 0; i < 8; ++i)
      queue.submit(std::make_unique<FileNotice>(std::to_string(i), FileNoticeKind::repair_backup));
  });
  worker.join();
  std::vector<std::string> delivered;
  testing::internal::CaptureStderr();
  queue.deliver([&](const auto &text) { delivered.push_back(text); });
  queue.flush();
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
  ASSERT_EQ(delivered.size(), 8u);
  for (size_t i = 0; i < delivered.size(); ++i)
    EXPECT_THAT(delivered[i], EndsWith(std::to_string(i)));
}
struct SelectiveControl : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  pid_t bad = -1;
  int offset = 0;
  bool observe_failure = false, reap_failure = false, allocation_failure = false, interrupt = false;
  bool expire_on_term = false, exit_before_expiry = false;
  std::map<pid_t, bool> observed;
  std::map<pid_t, int> observers;
  int watch(pid_t pid) override {
    if (bad < 0)
      bad = pid;
    return observers[pid] = native->watch(pid);
  }
  bool exited(pid_t pid, int fd) override {
    if (pid == bad && allocation_failure)
      throw std::bad_alloc();
    if (pid == bad && observe_failure)
      throw Error("injected observation failure", interrupt ? 130 : 1);
    return observed[pid] = observed[pid] || native->exited(pid, fd);
  }
  void signal(pid_t pid, int sig) override {
    native->signal(pid, sig);
    if (expire_on_term && sig == SIGTERM) {
      if (exit_before_expiry)
        for (int i = 0; i < 500 && !exited(-pid, observers.at(-pid)); ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
      offset += 20000;
    }
  }
  int reap(pid_t pid) override {
    if (pid == bad && reap_failure)
      throw Error("injected reap failure");
    return native->reap(pid);
  }
  std::chrono::steady_clock::time_point now() const override {
    return native->now() + std::chrono::milliseconds(offset);
  }
};
TEST_F(FinalReview, ExpiredStopAndGraceExpiryHaveTimeoutStatusAndFinishedJobsKeepStatus) {
  for (bool grace : {false, true}) {
    auto control = std::make_shared<SelectiveControl>();
    NativeProcesses native(control);
    auto spec = shell("trap '' TERM; printf ready > ready; sleep 20");
    const auto id = native.start(spec, cancel);
    for (int i = 0; i < 500 && !fs::exists(config.cwd / "ready"); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(fs::exists(config.cwd / "ready"));
    fs::remove(config.cwd / "ready");
    if (grace)
      control->expire_on_term = true;
    else
      control->offset += 20000;
    native.stop(id);
    EXPECT_EQ(native.poll(id).exit_code, 124);
    native.stop(id);
  }
  auto control = std::make_shared<SelectiveControl>();
  NativeProcesses native(control);
  const auto id = native.start(shell("exit 7"), cancel);
  // Observe without reaping, latching Darwin's one-shot event in the test seam.
  for (int i = 0; i < 500 && !control->exited(control->bad, control->observers.at(control->bad));
       ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  control->offset += 20000;
  native.stop(id);
  EXPECT_EQ(native.poll(id).exit_code, 7);
}
TEST_F(FinalReview, StopObservesCompletionBeforeForcingTerminationAfterGrace) {
  auto control = std::make_shared<SelectiveControl>();
  NativeProcesses native(control);
  const auto id = native.start(shell("sleep 20"), cancel);
  control->expire_on_term = true;
  control->exit_before_expiry = true;
  native.stop(id);
  // SIGTERM has already completed the command before the fake deadline expires.
  EXPECT_EQ(native.poll(id).exit_code, 128 + SIGTERM);
}
TEST_F(FinalReview, TimeoutReasonSurvivesFailedReapingAndRetry) {
  auto control = std::make_shared<SelectiveControl>();
  NativeProcesses native(control);
  const auto id = native.start(shell("sleep 20"), cancel);
  control->offset += 20000;
  control->reap_failure = true;
  EXPECT_THROW(native.stop(id), Error);
  const auto statuses = native.list();
  EXPECT_TRUE(statuses.front().running);
  control->reap_failure = false;
  EXPECT_EQ(native.poll(id).exit_code, 124);
  EXPECT_FALSE(native.poll(id).running);
}
TEST_F(FinalReview, BrokenBackgroundJobDoesNotBlockOtherJobsOrHooksAndRemainsOwned) {
  auto control = std::make_shared<SelectiveControl>();
  std::vector<std::string> diagnostics;
  NativeProcesses native(control, [&](const auto &s) { diagnostics.push_back(s); });
  const auto bad = native.start(shell("sleep 20"), cancel);
  control->observe_failure = true;
  EXPECT_EQ(native.run(shell("printf healthy"), cancel).output, "healthy");
  const auto statuses = native.list();
  ASSERT_EQ(statuses.size(), 1u);
  EXPECT_EQ(statuses[0].id, bad);
  EXPECT_TRUE(statuses[0].running);
  EXPECT_THAT(statuses[0].error, HasSubstr(bad));
  EXPECT_EQ(diagnostics.size(), 1u);
  config.hooks["SessionStart"] = Json::array({{{"command", "printf hook-healthy"}}});
  Tools tools(config, files, native, http, clock);
  EXPECT_EQ(tools.run_hooks("SessionStart", Json::object(), cancel).additional_context,
            "hook-healthy");
  EXPECT_THROW(native.poll(bad), Error);
  control->allocation_failure = true;
  EXPECT_THROW(native.start(shell("true"), cancel), std::bad_alloc);
  control->allocation_failure = false;
  control->interrupt = true;
  try {
    native.start(shell("true"), cancel);
    ADD_FAILURE();
  } catch (const Error &e) {
    EXPECT_EQ(e.code, 130);
  }
  control->interrupt = false;
  control->observe_failure = false;
  native.stop(bad);
  EXPECT_FALSE(native.poll(bad).running);
}

TEST_F(FinalReview, LiteralWorkspaceAllowRuleDoesNotGrantMatchingSibling) {
  config.cwd = temp.root / "work[1]";
  config.roots = {config.cwd};
  config.permission_mode = "default";
  fs::create_directories(config.cwd);
  fs::create_directories(temp.root / "work1");
  config.roots.push_back(temp.root / "work1");
  config.set_allow({"apply_patch(" + (config.cwd / "file").string() + ")"});
  Permissions permissions(config, files);
  EXPECT_EQ(permissions.decide(
                {"apply_patch", (config.cwd / "file").string(), config.cwd / "file", true}),
            Decision::allow);
  const auto sibling = temp.root / "work1/file";
  EXPECT_EQ(permissions.decide({"apply_patch", sibling.string(), sibling, true}), Decision::ask);
}
TEST_F(FinalReview, GlobFiltersLiteralWorkspaceAndRewrittenReadRemainsDenied) {
  config.cwd = temp.root / "work[*]?\\dir";
  config.roots = {config.cwd};
  fs::create_directories(config.cwd / "hidden");
  files.atomic_write(config.cwd / "hidden/secret", "secret");
  files.atomic_write(config.cwd / "public", "public");
  config.set_deny({"read_file(hidden)", "read_file(hidden/secret)"});
  Tools tools(config, files, processes, http, clock);
  const auto found = execute_core(tools, config, {"glob", "read_file", {{"path", "."}}}, cancel);
  EXPECT_FALSE(found.error);
  EXPECT_THAT(found.content, HasSubstr("public"));
  EXPECT_THAT(found.content, Not(HasSubstr("secret")));
  config.hooks["PreToolUse"] =
      Json::array({{{"matcher", "read_file"},
                    {"command", "printf '%s' "
                                "'{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\","
                                "\"updatedInput\":{\"path\":\"./hidden/secret\"}}}'"}}});
  EXPECT_TRUE(
      execute_core(tools, config, {"read", "read_file", {{"path", "public"}}}, cancel).error);
}
TEST_F(FinalReview, FailedForegroundCleanupRetainsOwnershipAndRetriesWithoutBlockingOtherWork) {
  auto control = std::make_shared<SelectiveControl>();
  NativeProcesses native(control);
  control->reap_failure = true;
  EXPECT_THROW(native.run(shell("exit 0"), cancel), Error);
  EXPECT_TRUE(native.list().empty());
  EXPECT_EQ(native.run(shell("printf healthy"), cancel).output, "healthy");
  EXPECT_TRUE(native.list().empty());
  EXPECT_THROW(native.poll("job-1"), Error);
  EXPECT_THROW(native.stop("job-1"), Error);
  control->reap_failure = false;
  EXPECT_TRUE(native.list().empty());
  EXPECT_EQ(native.run(shell("printf recovered"), cancel).output, "recovered");
}
TEST_F(FinalReview, FailedCleanupJobsContinueToCountAgainstActiveLimit) {
  auto control = std::make_shared<SelectiveControl>();
  NativeProcesses native(control);
  auto spec = shell("sleep 120");
  spec.timeout_ms = 120000;
  const auto failed = native.start(spec, cancel);
  control->observe_failure = true;
  std::vector<std::string> ids;
  for (int i = 0; i < 63; ++i)
    ids.push_back(native.start(spec, cancel));
  EXPECT_THROW(native.start(spec, cancel), Error);
  auto statuses = native.list();
  EXPECT_EQ(statuses.size(), 64u);
  native.stop(ids.back());
  EXPECT_NO_THROW(native.start(spec, cancel));
  control->observe_failure = false;
  native.stop(failed);
}
} // namespace
