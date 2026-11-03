#include "codet/core.hpp"
#include "codet/process_control.hpp"
#include "temp.hpp"
#include <csignal>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace codet;
using namespace testing;
namespace {
struct ReviewFixture : Test {
  TempDirectory temp;
  fs::path root;
  NativeFileSystem files;
  SystemClock clock;
  Cancellation cancel;
  CurlTransport http;
  NativeProcesses processes;
  Config config;
  void SetUp() override {
    root = temp.root;
    config.home = root / "state";
    config.cwd = root / "workspace";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "fixture";
    config.providers["openai"].api_key = "fake";
    config.providers["openai"].wire_api = "chat_completions";
  }

  ToolResult shell(const std::string &command, int timeout = 5000) {
    (void)timeout;
    Tools tools(config, files, processes, http, clock);
    auto result = tools.execute(
        {"shell", "exec_command", {{"cmd", command}, {"login", false}, {"shell", "/bin/sh"}}},
        cancel);
    if (!result.error) {
      const auto data = Json::parse(result.content);
      result.content = data.at("output");
      result.metadata = data;
      result.error = data.value("exit_code", 0) != 0;
    }
    return result;
  }
};
TEST_F(ReviewFixture, ScopedShellAllowsRequireLiteralSimpleCommands) {
  config.set_allow({"exec_command(git *)", "exec_command(git *)"});
  Permissions permissions(config, files);
  for (const auto &tool : {"exec_command", "exec_command"}) {
    EXPECT_EQ(permissions.decide({tool, "git status --short", {}, true}), Decision::allow);
    for (const auto &suffix :
         {"; printf x", " && true", " | cat", " $(true)", " `true`", " > file", "\ntrue", "\targ",
          " 'literal'", " \"literal\"", " \\x", " *", " ?", " [x]", " {a,b}", " ~", " # comment"})
      EXPECT_EQ(permissions.decide({tool, std::string("git status") + suffix, {}, true}),
                Decision::ask)
          << suffix;
  }
  config.set_allow({"exec_command(*)"});
  for (const auto &command : {"if true", "while true", "VAR=value true", "! true", "time true"})
    EXPECT_EQ(permissions.decide({"exec_command", command, {}, true}), Decision::ask);
}
TEST_F(ReviewFixture, ExplicitGrantsBypassDenialsAndPlanKeepTheirMeanings) {
  Permissions permissions(config, files);
  config.set_allow({"exec_command"});
  EXPECT_EQ(permissions.decide({"exec_command", "printf a; printf b", {}, true}), Decision::allow);
  config.set_allow({});
  config.permission_mode = "bypass";
  EXPECT_EQ(permissions.decide({"exec_command", "printf a; printf b", {}, true}), Decision::allow);
  config.set_deny({"exec_command(printf*)"});
  EXPECT_EQ(permissions.decide({"exec_command", "printf a; printf b", {}, true}), Decision::deny);
  config.permission_mode = "plan";
  EXPECT_EQ(permissions.decide({"exec_command", "true", {}, true}), Decision::deny);
  config.permission_mode = "default";
  config.approval = "never";
  const auto denied = shell("true");
  EXPECT_TRUE(denied.error);
  EXPECT_THAT(denied.content, HasSubstr("requires approval"));
}
TEST_F(ReviewFixture, GrepIsRetiredAndCompletedHistoricalResultsRemainReadable) {
  Tools tools(config, files, processes, http, clock);
  for (const auto &schema : tools.schemas())
    EXPECT_NE(schema["name"], "Grep");
  const ToolCall call{"old-search", "Grep", {{"pattern", "needle"}}};
  auto result = tools.execute(call, cancel);
  EXPECT_TRUE(result.error);
  EXPECT_THAT(result.content, HasSubstr("use exec_command"));
  StateStore state(config.home);
  auto session = state.create();
  codet::Message request;
  request.role = "assistant";
  request.calls = {call};
  session.messages = {request};
  state.save(session);
  state.tool_started(session.id, call);
  codet::Message historical;
  historical.role = "tool";
  historical.tool_call_id = call.id;
  historical.text = "historical match";
  state.tool_finished(session.id, call, Json(historical));
  state.begin_turn(session.id);
  auto recovered = state.load(session.id);
  EXPECT_THAT(Json(recovered.messages).dump(), HasSubstr("historical match"));
}
TEST_F(ReviewFixture, ShellGrepQuotesPathsOptionsAndExitStatuses) {
  config.permission_mode = "bypass";
  files.atomic_write(config.cwd / "résumé -file", "a'b\n-dash\n");
  auto result = shell("/usr/bin/grep -n -e \"a'b\" -- './résumé -file'");
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_THAT(result.content, HasSubstr("1:a'b"));
  result = shell("/usr/bin/grep -n -e '-dash' -- './résumé -file'");
  EXPECT_FALSE(result.error);
  EXPECT_THAT(result.content, HasSubstr("2:-dash"));
  result = shell("/usr/bin/grep -e missing -- './résumé -file'");
  EXPECT_EQ(result.metadata["exit_code"], 1);
  result = shell("/usr/bin/grep -e '[' -- './résumé -file'");
  EXPECT_EQ(result.metadata["exit_code"], 2);
}
TEST_F(ReviewFixture, ShellGrepHandlesLongLinesAndBoundsOutput) {
  config.permission_mode = "bypass";
  config.tool_output_limit = 512;
  files.atomic_write(config.cwd / "long", std::string(524288, 'x') + "needle\n");
  const auto result = shell("/usr/bin/grep -E -e '.*needle.*' -- long");
  EXPECT_FALSE(result.error) << result.content;
  EXPECT_TRUE(result.metadata.contains("original_token_count"));
  EXPECT_LE(result.content.size(), 512u);
}

TEST_F(ReviewFixture, EmptyOptionalToolDeclarationsAreOmitted) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  const Json tools = Json::array(
      {{{"name", "read_file"}, {"description", "read"}, {"input_schema", {{"type", "object"}}}}});
  for (const auto &name : {"openai", "ollama", "google"}) {
    config.provider = name;
    config.providers[name].api_key = "fake";
    EXPECT_FALSE(
        Json::parse(provider.request(config, "system", {}, Json::array()).body).contains("tools"));
    EXPECT_FALSE(Json::parse(provider.request(config, "system", {}, tools).body)["tools"].empty());
  }
}
TEST_F(ReviewFixture, EndHookCleanupHonorsTotalBudget) {
  config.hooks = {{"SessionEnd", Json::array({{{"command", "sleep 2"}, {"timeout_ms", 120000}}})}};
  Tools tools(config, files, processes, http, clock);
  const auto began = std::chrono::steady_clock::now();
  EXPECT_FALSE(tools.run_hooks("SessionEnd", Json::object(), cancel, 40).diagnostics.empty());
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                  began)
                .count(),
            1500);
}
TEST_F(ReviewFixture, SelectionTracksOnlyExplicitProfileAndCliFields) {
  files.atomic_write(config.home / "config.toml", "provider='openai'\nmodel='base'\n");
  files.atomic_write(config.home / "limits.config.toml", "max_turns=10\n");
  auto loaded = load_config(config.home, config.cwd, "limits", {"max_turns=20"}, files);
  EXPECT_FALSE(loaded.selected_provider);
  EXPECT_FALSE(loaded.selected_model);
  files.atomic_write(config.home / "choose.config.toml",
                     "model_provider='ollama'\nmodel='profile'\n");
  loaded = load_config(config.home, config.cwd, "choose", {"model='cli'"}, files);
  EXPECT_EQ(loaded.selected_provider, "ollama");
  EXPECT_EQ(loaded.selected_model, "cli");
}
struct RecordingControl : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  std::vector<std::string> events;
  bool fail_watch = false, reaped = false, exit_before_release = false;
  int watch(pid_t pid) override {
    events.push_back("watch");
    if (fail_watch)
      throw Error("fixture observer failure");
    const int observer = native->watch(pid);
    if (exit_before_release) {
      native->signal(pid, SIGKILL);
      bool done = false;
      for (int i = 0; i < 500 && !(done = native->exited(pid, observer)); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      EXPECT_TRUE(done);
    }
    return observer;
  }
  bool exited(pid_t pid, int observer) override {
    const bool exit = native->exited(pid, observer);
    if (exit)
      events.push_back("exit");
    return exit;
  }
  void signal(pid_t pid, int sig) override {
    EXPECT_FALSE(reaped) << "Never signal a recycled PID";
    events.push_back("signal");
    native->signal(pid, sig);
  }
  int reap(pid_t pid) override {
    events.push_back("reap");
    const int status = native->reap(pid);
    reaped = true;
    return status;
  }
};
TEST(ProcessOwnership, NormalCompletionSignalsBeforeReapAndLaterStopsDoNothing) {
  auto control = std::make_shared<RecordingControl>();
  size_t count = 0;
  {
    NativeProcesses processes(control);
    ProcessSpec spec;
    spec.argv = {"/bin/sh", "-c", "printf done"};
    const auto id = processes.start(spec);
    ProcessResult result;
    for (int i = 0; i < 500; ++i) {
      result = processes.poll(id);
      if (!result.running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_FALSE(result.running);
    EXPECT_EQ(result.output, "done");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_THAT(control->events, ElementsAre("watch", "exit", "signal", "reap"));
    count = control->events.size();
    processes.stop(id);
    processes.stop(id);
    EXPECT_EQ(control->events.size(), count);
  }
  EXPECT_EQ(control->events.size(), count);
}
TEST(ProcessOwnership, FailedWatcherCleansUpWithoutExecutingCommand) {
  auto control = std::make_shared<RecordingControl>();
  control->fail_watch = true;
  NativeProcesses processes(control);
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "exit 42"};
  EXPECT_THROW(processes.start(spec), Error);
  EXPECT_THAT(control->events, ElementsAre("watch", "signal", "reap"));
}
TEST(ProcessOwnership, ChildExitBeforeReleaseDoesNotDeliverSigpipeToParent) {
  auto control = std::make_shared<RecordingControl>();
  control->exit_before_release = true;
  NativeProcesses processes(control);
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "exit 42"};
  EXPECT_THROW(processes.start(spec), Error);
  EXPECT_THAT(control->events, ElementsAre("watch", "signal", "reap"));
}
TEST(ProcessOwnership, ImmediateExitsCannotRaceWatcherRegistration) {
  NativeProcesses processes;
  Cancellation cancel;
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "exit 7"};
  for (int i = 0; i < 20; ++i)
    EXPECT_EQ(processes.run(spec, cancel).exit_code, 7);
}
TEST_F(ReviewFixture, NormalExitCleansUpTermIgnoringDescendants) {
  ProcessSpec spec;
  spec.cwd = config.cwd;
  spec.argv = {"/bin/sh", "-c", "(trap '' TERM; sleep 1; printf leaked > leaked) & exit 0"};
  EXPECT_EQ(processes.run(spec, cancel).exit_code, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  EXPECT_FALSE(fs::exists(config.cwd / "leaked"));
}
TEST(ProcessOwnership, ChildResetsIgnoredSigpipeAndBlockedSignalMask) {
  struct Restore {
    struct sigaction action {};
    sigset_t mask;
    Restore() {
      sigaction(SIGPIPE, nullptr, &action);
      sigprocmask(SIG_SETMASK, nullptr, &mask);
    }
    ~Restore() {
      sigaction(SIGPIPE, &action, nullptr);
      sigprocmask(SIG_SETMASK, &mask, nullptr);
    }
  } restore;
  signal(SIGPIPE, SIG_IGN);
  sigset_t blocked;
  sigemptyset(&blocked);
  sigaddset(&blocked, SIGUSR1);
  sigprocmask(SIG_BLOCK, &blocked, nullptr);
  NativeProcesses processes;
  Cancellation cancel;
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "kill -PIPE $$; printf survived"};
  EXPECT_EQ(processes.run(spec, cancel).exit_code, 128 + SIGPIPE);
  spec.argv = {"/bin/sh", "-c", "kill -USR1 $$; printf survived"};
  EXPECT_EQ(processes.run(spec, cancel).exit_code, 128 + SIGUSR1);
}
} // namespace
