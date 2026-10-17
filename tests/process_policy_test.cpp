#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/process_control.hpp"
#include "codet/prompt.hpp"
#include "core_tools.hpp"
#include "temp.hpp"
#include <cerrno>
#include <csignal>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sstream>
#include <sys/wait.h>
#include <thread>
using namespace codet;
using namespace testing;
namespace {
struct Policies : Test {
  TempDirectory temp;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  Config config;
  void SetUp() override {
    config.cwd = files.canonical(temp.root / "work[1]");
    config.home = temp.root / "home";
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.tool_output_limit = 512;
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
  }
  ProcessSpec command(const std::string &text) {
    ProcessSpec s;
    s.argv = {"/bin/sh", "-c", text};
    s.cwd = config.cwd;
    s.timeout_ms = 120000;
    return s;
  }
};
struct BrokenObservation : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  enum class Failure { none, normal, allocation, resource, interrupt };
  Failure failure = Failure::none;
  pid_t selected = -1;
  bool reap_failure = false;
  size_t selected_reaps = 0;
  std::vector<std::pair<pid_t, int>> signals;
  int watch(pid_t pid) override {
    if (selected < 0)
      selected = pid;
    return native->watch(pid);
  }
  bool exited(pid_t pid, int fd) override {
    if (pid == selected) {
      if (failure == Failure::normal)
        throw std::runtime_error("ordinary observer failure");
      if (failure == Failure::allocation)
        throw std::bad_alloc();
      if (failure == Failure::resource)
        throw ProcessResourceError("descriptor exhaustion");
      if (failure == Failure::interrupt)
        throw Error("Interrupted", 130);
    }
    return native->exited(pid, fd);
  }
  void signal(pid_t pid, int sig) override {
    signals.emplace_back(pid, sig);
    native->signal(pid, sig);
  }
  int reap(pid_t pid) override {
    if (pid == selected)
      ++selected_reaps;
    if (pid == selected && reap_failure)
      throw std::runtime_error("ordinary reap failure");
    return native->reap(pid);
  }
  std::optional<int> try_reap(pid_t pid) override {
    if (pid == selected)
      ++selected_reaps;
    if (pid == selected && reap_failure)
      throw std::runtime_error("ordinary reap failure");
    return native->try_reap(pid);
  }
};
TEST_F(Policies, CleanupReapsWithoutAnExitObserverAfterTransientReapFailure) {
  auto control = std::make_shared<BrokenObservation>();
  NativeProcesses owned(control);
  control->failure = BrokenObservation::Failure::normal;
  control->reap_failure = true;
  EXPECT_THROW(owned.run(command("sleep 30"), cancel), std::runtime_error);
  const auto before = control->selected_reaps;
  control->reap_failure = false;
  for (int i = 0; i < 100 && control->selected_reaps == before; ++i)
    EXPECT_TRUE(owned.list().empty());
  EXPECT_GT(control->selected_reaps, before);
  // Explicit cleanup must also remain safe after a background fallback collected the status.
  control->failure = BrokenObservation::Failure::none;
  owned.shutdown();
  int status = 0;
  EXPECT_EQ(::waitpid(control->selected, &status, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
}
TEST_F(Policies, StopAndDestructionTerminateEvenWithPersistentObservationFailure) {
  for (bool destructor : {false, true}) {
    auto control = std::make_shared<BrokenObservation>();
    pid_t pid = -1;
    {
      NativeProcesses owned(control);
      const auto id = owned.start(command("sleep 30"), cancel);
      pid = control->selected;
      control->failure = BrokenObservation::Failure::normal;
      if (!destructor) {
        EXPECT_NO_THROW(owned.stop(id));
        EXPECT_FALSE(owned.poll(id).running);
      }
    }
    EXPECT_THAT(control->signals, Contains(std::make_pair(-pid, SIGTERM)));
    EXPECT_THAT(control->signals, Contains(std::make_pair(-pid, SIGKILL)));
    int status = 0;
    EXPECT_EQ(::waitpid(pid, &status, WNOHANG), -1);
    EXPECT_EQ(errno, ECHILD);
  }
}
TEST_F(Policies, StandardJobFailuresAreIsolatedAndResourceFailuresStillPropagate) {
  auto control = std::make_shared<BrokenObservation>();
  NativeProcesses owned(control);
  const auto id = owned.start(command("sleep 30"), cancel);
  control->failure = BrokenObservation::Failure::normal;
  EXPECT_EQ(owned.run(command("printf healthy"), cancel).output, "healthy");
  const auto states = owned.list();
  ASSERT_EQ(states.size(), 1u);
  EXPECT_THAT(states[0].error, HasSubstr("ordinary observer failure"));
  for (auto failure : {BrokenObservation::Failure::allocation, BrokenObservation::Failure::resource,
                       BrokenObservation::Failure::interrupt}) {
    control->failure = failure;
    if (failure == BrokenObservation::Failure::allocation)
      EXPECT_THROW(owned.start(command("true"), cancel), std::bad_alloc);
    if (failure == BrokenObservation::Failure::resource)
      EXPECT_THROW(owned.start(command("true"), cancel), ProcessResourceError);
    if (failure == BrokenObservation::Failure::interrupt) {
      try {
        owned.start(command("true"), cancel);
        FAIL();
      } catch (const Error &e) {
        EXPECT_EQ(e.code, 130);
      }
    }
  }
  control->failure = BrokenObservation::Failure::none;
  owned.stop(id);
}
TEST_F(Policies, StopCleansOwnedChildBeforePropagatingInterruptionOrResourceFailure) {
  for (auto failure : {BrokenObservation::Failure::allocation, BrokenObservation::Failure::resource,
                       BrokenObservation::Failure::interrupt}) {
    auto control = std::make_shared<BrokenObservation>();
    NativeProcesses owned(control);
    const auto id = owned.start(command("sleep 30"), cancel);
    const auto pid = control->selected;
    control->failure = failure;
    if (failure == BrokenObservation::Failure::allocation)
      EXPECT_THROW(owned.stop(id), std::bad_alloc);
    if (failure == BrokenObservation::Failure::resource)
      EXPECT_THROW(owned.stop(id), ProcessResourceError);
    if (failure == BrokenObservation::Failure::interrupt) {
      try {
        owned.stop(id);
        FAIL();
      } catch (const Error &e) {
        EXPECT_EQ(e.code, 130);
      }
    }
    int status = 0;
    EXPECT_EQ(::waitpid(pid, &status, WNOHANG), -1);
    EXPECT_EQ(errno, ECHILD);
    EXPECT_FALSE(owned.poll(id).running);
  }
}
TEST_F(Policies, FailedInternalCleanupIsPrivateAndConsumesCapacityUntilReaped) {
  auto control = std::make_shared<BrokenObservation>();
  NativeProcesses owned(control);
  control->reap_failure = true;
  EXPECT_THROW(owned.run(command("exit 0"), cancel), std::runtime_error);
  EXPECT_TRUE(owned.list().empty());
  EXPECT_THROW(owned.poll("job-1"), Error);
  EXPECT_THROW(owned.stop("job-1"), Error);
  EXPECT_THROW(owned.write("job-1", "x"), Error);
  std::vector<std::string> public_jobs;
  for (int i = 0; i < 63; ++i)
    public_jobs.push_back(owned.start(command("sleep 30"), cancel));
  EXPECT_EQ(owned.list().size(), 63u);
  EXPECT_THROW(owned.start(command("true"), cancel), Error);
  control->reap_failure = false;
  EXPECT_EQ(owned.run(command("printf recovered"), cancel).output, "recovered");
  for (const auto &id : public_jobs)
    owned.stop(id);
  EXPECT_EQ(owned.list().size(), 63u);
}
TEST_F(Policies, RoutineRefreshRetriesFailedCleanupWithoutAStopGraceLoop) {
  auto control = std::make_shared<BrokenObservation>();
  NativeProcesses owned(control);
  control->reap_failure = true;
  EXPECT_THROW(owned.run(command("exit 0"), cancel), std::runtime_error);
  auto before = control->selected_reaps;
  EXPECT_TRUE(owned.list().empty());
  EXPECT_LE(control->selected_reaps - before, 2u);
  before = control->selected_reaps;
  const auto healthy = owned.start(command("sleep 30"), cancel);
  EXPECT_LE(control->selected_reaps - before, 2u);
  control->reap_failure = false;
  owned.stop(healthy);
}
TEST_F(Policies, ForegroundFailureRetainsOriginalStandardException) {
  auto control = std::make_shared<BrokenObservation>();
  NativeProcesses owned(control);
  control->failure = BrokenObservation::Failure::normal;
  try {
    owned.run(command("sleep 30"), cancel);
    FAIL();
  } catch (const std::runtime_error &e) {
    EXPECT_EQ(std::string(e.what()), "ordinary observer failure");
  }
  EXPECT_TRUE(owned.list().empty());
  EXPECT_FALSE(control->signals.empty());
}
TEST_F(Policies, CanonicalLoadedWorkspaceAndEquivalentAliasDenyLiteralMetacharacters) {
  const auto alias = temp.root / "alias";
  fs::create_directory_symlink(config.cwd, alias);
  auto loaded = load_config(config.home, config.cwd, "", {}, files);
  loaded.permission_mode = "bypass";
  loaded.set_deny({"read_file(" + (alias / "secret").string() + ")"});
  files.atomic_write(config.cwd / "secret", "private");
  Tools tools(loaded, files, processes, http, clock);
  EXPECT_TRUE(
      execute_core(tools, loaded, {"read", "read_file", {{"path", "secret"}}}, cancel).error);
#ifdef __APPLE__
  TempDirectory unused;
  char name[] = "/tmp/codet-alias-XXXXXX";
  const auto *created = ::mkdtemp(name);
  ASSERT_NE(created, nullptr);
  const fs::path temporary = created;
  fs::create_directory(temporary / "w[1]");
  auto cfg = load_config(config.home, files.canonical(temporary / "w[1]"), "", {}, files);
  cfg.permission_mode = "bypass";
  cfg.set_deny({"read_file(" + (temporary / "w[1]/secret").string() + ")"});
  files.atomic_write(cfg.cwd / "secret", "private");
  Tools mac(cfg, files, processes, http, clock);
  EXPECT_TRUE(execute_core(mac, cfg, {"mac", "read_file", {{"path", "secret"}}}, cancel).error);
  fs::remove_all(temporary);
#endif
}
TEST_F(Policies, WildcardParentsNormalizeWithoutExpandingGlobsAndSnapshotChangesInvalidateRules) {
  config.permission_mode = "plan";
  files.atomic_write(config.cwd / "secret", "private");
  Permissions permissions(config, files);
  for (const auto &pattern : {"*/../secret", "**/../secret", "a/*/../../secret", "./secret"}) {
    config.set_deny({std::string("read_file(") + pattern + ")"});
    EXPECT_EQ(permissions.decide({"read_file", "secret", config.cwd / "secret", false}),
              Decision::deny)
        << pattern;
  }
  auto old = config.rule_snapshot();
  config.set_deny({});
  EXPECT_NE(old, config.rule_snapshot());
  EXPECT_FALSE(old->deny.empty());
  EXPECT_EQ(permissions.decide({"read_file", "secret", config.cwd / "secret", false}),
            Decision::allow);
}
TEST_F(Policies, DeniedCrLfPatchNeverRunsAnInputRewritingHook) {
  config.set_deny({"apply_patch(secret)"});
  Json rewrite = {
      {"hookSpecificOutput",
       {{"permissionDecision", "allow"},
        {"updatedInput",
         {{"patch", "*** Begin Patch\n*** Add File: public\n+allowed\n*** End Patch\n"}}}}}};
  config.hooks["PreToolUse"] =
      Json::array({{{"command", "printf ran > hook-ran; printf '%s' '" + rewrite.dump() + "'"}}});
  Tools tools(config, files, processes, http, clock);
  for (const auto &directive :
       {"*** Add File: secret", "*** Update File: secret", "*** Delete File: secret",
        "*** Update File: public\r\n*** Move to: secret"}) {
    const auto r = execute_core(tools, config,
                                {unique_id(),
                                 "apply_patch",
                                 {{"patch", std::string("*** Begin Patch\r\n") + directive +
                                                "\r\n+x\r\n*** End Patch\r\n"}}},
                                cancel);
    EXPECT_TRUE(r.error);
    EXPECT_FALSE(fs::exists(config.cwd / "hook-ran"));
    EXPECT_FALSE(fs::exists(config.cwd / "public"));
  }
}
TEST_F(Policies, ValidLargeDenyUsesFullChannelBudgetAndUnusedStdoutIsIgnored) {
  config.tool_output_limit = 65536;
  files.atomic_write(config.cwd / "decision",
                     Json({{"decision", "block"}, {"reason", std::string(40000, 'x')}}).dump());
  config.hooks["PreToolUse"] = Json::array({{{"command", "cat decision"}}});
  std::vector<std::string> diagnostics;
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { diagnostics.push_back(s); });
  auto h = tools.run_hooks("PreToolUse", {{"tool_name", "Bash"}}, cancel);
  EXPECT_TRUE(h.blocked);
  EXPECT_EQ(h.reason.size(), 40000u);
  files.atomic_write(config.cwd / "decision",
                     Json({{"decision", "block"}, {"reason", std::string(70000, 'x')}}).dump());
  EXPECT_FALSE(tools.run_hooks("PreToolUse", Json::object(), cancel).blocked);
  EXPECT_THAT(diagnostics, Contains(HasSubstr("truncated JSON")));
  config.tool_output_limit = 512;
  diagnostics.clear();
  for (const auto &event : {"SessionEnd", "PostToolUseFailure"}) {
    config.hooks[event] = Json::array({{{"command", "printf '%01000d' 0"}}});
    tools.run_hooks(event, Json::object(), cancel);
    EXPECT_TRUE(diagnostics.empty());
  }
  config.hooks["SessionEnd"] = Json::array({{{"command", "printf warning >&2"}}});
  tools.run_hooks("SessionEnd", Json::object(), cancel);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_THAT(diagnostics[0], HasSubstr("hook warning"));
  EXPECT_THAT(diagnostics[0], Not(HasSubstr("cleanup failed")));
}
TEST(DiagnosticPolicies, SharedBufferBudgetsFormattedMessagesAndReportsOmissionsOnce) {
  DiagnosticBuffer buffer(256);
  for (int i = 0; i < 20; ++i)
    buffer.add("SessionEnd hook warning: " + std::string(40, 'x') + "雪");
  std::ostringstream output;
  buffer.flush(output);
  const auto text = output.str();
  EXPECT_LE(text.size(), 256u);
  EXPECT_THAT(text, EndsWith(DiagnosticBuffer::notice() + "\n"));
  EXPECT_NO_THROW(Json(text).dump());
  std::ostringstream again;
  buffer.flush(again);
  EXPECT_TRUE(again.str().empty());
}
struct ReadyProvider : Provider {
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink & = {}) override {
    ProviderResult r;
    r.message.role = "assistant";
    r.message.text = "done";
    return r;
  }
};
TEST_F(Policies, FailedPromptSaveRollsBackContextBeforeRecoveryAndNextTurnSucceeds) {
  StateStore state(config.home);
  auto session = state.create();
  config.hooks["UserPromptSubmit"] = Json::array({{{"command", "printf accepted-context"}}});
  Tools tools(config, files, processes, http, clock);
  ReadyProvider provider;
  std::vector<std::string> diagnostics;
  Agent agent(config, provider, tools, state, {}, [&](const auto &s) { diagnostics.push_back(s); });
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TRIGGER fail_save BEFORE INSERT ON sessions BEGIN SELECT "
                         "RAISE(ABORT,'injected save failure'); END",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_THROW(agent.run(session, "prompt", cancel), Error);
  EXPECT_TRUE(session.messages.empty());
  EXPECT_TRUE(state.load_id(session.id).messages.empty());
  EXPECT_TRUE(diagnostics.empty());
  ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER fail_save", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_EQ(agent.run(session, "prompt", cancel).text, "done");
  ASSERT_GE(session.messages.size(), 2u);
  EXPECT_THAT(session.messages[0].text, HasSubstr("accepted-context"));
}
TEST(PromptPolicies, AllocationFailureAfterFirstAppendRestoresExistingHistory) {
  Session session;
  codet::Message existing;
  existing.role = "user";
  existing.text = "original";
  session.messages = {existing};
  codet::Message context;
  context.role = "system";
  context.text = "new context";
  codet::Message user;
  user.role = "user";
  user.text = "new prompt";
  for (size_t fail : {0u, 1u}) {
    EXPECT_THROW((PromptAppend(session, {context, user},
                               [&](size_t at) {
                                 if (at == fail)
                                   throw std::bad_alloc();
                               })),
                 std::bad_alloc);
    ASSERT_EQ(session.messages.size(), 1u);
    EXPECT_EQ(session.messages[0].text, "original");
  }
  {
    PromptAppend pending(session, {context, user});
    EXPECT_EQ(session.messages.size(), 3u);
  }
  EXPECT_EQ(session.messages.size(), 1u);
}

} // namespace
