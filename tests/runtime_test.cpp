#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/interaction.hpp"
#include "codet/output.hpp"
#include "codet/process_control.hpp"
#include "codet/tool_registry.hpp"
#include "temp.hpp"
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json-schema.hpp>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
struct Runtime : Test {
  TempDirectory temp;
  Config config;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    config.cwd = files.canonical(temp.root / "work[1]");
    config.home = temp.root / "home";
    config.roots = {config.cwd};
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
  }
  ToolCall call(std::string name, Json args) {
    return {unique_id(), std::move(name), std::move(args)};
  }
};
struct Recording : Processes {
  std::vector<ProcessSpec> starts;
  std::vector<std::string> writes;
  std::function<ProcessResult(const ProcessSpec &)> hook;
  bool running = false, poll_error = false;
  std::string output = "done", capture_error;
  size_t closes = 0;
  ProcessResult run(const ProcessSpec &spec, const Cancellation &) override {
    return hook ? hook(spec) : ProcessResult{};
  }
  std::string start(const ProcessSpec &spec) override {
    starts.push_back(spec);
    return "owned";
  }
  ProcessResult poll(const std::string &) override {
    if (poll_error)
      throw Error("injected output collection failure");
    ProcessResult result{0, output, false, running};
    result.capture_error = capture_error;
    result.total_output_bytes = output.size();
    return result;
  }
  void stop(const std::string &) override {
    running = false;
  }
  void write(const std::string &, const std::string &text) override {
    writes.push_back(text);
  }
  void close_stdin(const std::string &) override {
    ++closes;
  }
};
TEST_F(Runtime, AbsoluteDeniesSurviveSlashRootsAliasesAndBypass) {
  const auto secret = temp.root / "secret";
  files.atomic_write(secret, "secret");
  fs::create_symlink(secret, temp.root / "alias");
  for (const auto &mode : {"default", "plan", "accept-edits", "bypass"})
    for (bool root_cwd : {false, true})
      for (const auto &pattern :
           {secret.string(), (secret.parent_path() / "sec*").string(), std::string("/*secret")}) {
        config.permission_mode = mode;
        config.cwd = root_cwd ? fs::path("/") : secret.parent_path();
        config.roots = {"/"};
        config.set_allow({"read_file"});
        config.set_deny({"read_file(" + pattern + ")"});
        Permissions permission(config, files);
        EXPECT_EQ(permission.decide({"read_file", secret.string(), secret, false}), Decision::deny)
            << mode << pattern;
        const auto alias = secret.parent_path() / "alias";
        EXPECT_EQ(permission.decide({"read_file", alias.string(), alias, false}), Decision::deny);
      }
}
TEST_F(Runtime, ExecRulesBindCwdAndDefaultShellAndExposeFullApprovalContext) {
  Recording process;
  config.set_allow({"exec_command(printf safe)"});
  std::vector<PermissionRequest> approvals;
  Tools tools(config, files, process, http, clock, [&](const auto &r) {
    approvals.push_back(r);
    return true;
  });
  EXPECT_FALSE(tools.execute(call("exec_command", {{"cmd", "printf safe"}}), cancel).error);
  EXPECT_TRUE(approvals.empty());
  fs::create_directories(temp.root / "outside");
  fs::create_directory_symlink(temp.root / "outside", config.cwd / "escape");
  for (const auto &cwd : {(temp.root / "outside").string(), std::string("escape")})
    EXPECT_TRUE(
        tools.execute(call("exec_command", {{"cmd", "printf safe"}, {"workdir", cwd}}), cancel)
            .error);
  auto custom = call(
      "exec_command",
      {{"cmd", "printf safe"}, {"shell", "/custom/shell"}, {"workdir", "."}, {"login", false}});
  EXPECT_FALSE(tools.execute(custom, cancel).error);
  ASSERT_EQ(approvals.size(), 1u);
  EXPECT_EQ(approvals[0].context.at("shell"), "/custom/shell");
  EXPECT_EQ(approvals[0].context.at("cwd"), files.canonical(config.cwd).string());
  EXPECT_EQ(process.starts.back().argv.front(), "/custom/shell");
  config.set_allow({"exec_command"});
  EXPECT_FALSE(tools.execute(custom, cancel).error);
  EXPECT_EQ(approvals.size(), 1u);
  config.permission_mode = "bypass";
  EXPECT_FALSE(tools
                   .execute(call("exec_command", {{"cmd", "printf safe"},
                                                  {"workdir", (temp.root / "outside").string()}}),
                            cancel)
                   .error);
  config.set_deny({"exec_command(printf safe)"});
  EXPECT_TRUE(tools.execute(custom, cancel).error);
}
TEST_F(Runtime, RewritesAndFailureHooksUseEffectiveExecutionContext) {
  config.permission_mode = "bypass";
  config.hooks = {
      {"PreToolUse", Json::array({{{"command", "rewrite"}, {"matcher", "Bash"}}})},
      {"PostToolUseFailure", Json::array({{{"command", "failure"}, {"matcher", "Bash"}}})}};
  Recording process;
  Json failure;
  process.hook = [&](const ProcessSpec &spec) {
    if (spec.argv[4] == "rewrite")
      return ProcessResult{
          0,
          R"({"hookSpecificOutput":{"permissionDecision":"allow","updatedInput":{"command":"effective","cwd":".","shell":"/effective/shell"}}})"};
    failure = Json::parse(files.read(spec.argv.back()));
    return ProcessResult{};
  };
  Tools tools(config, files, process, http, clock);
  const auto result = tools.execute(
      call("exec_command", {{"cmd", "original"}, {"sandbox_permissions", "require_escalated"}}),
      cancel);
  EXPECT_TRUE(result.error);
  EXPECT_TRUE(process.starts.empty());
  ASSERT_TRUE(failure.contains("tool_input"));
  EXPECT_EQ(failure["tool_input"]["command"], "effective");
  EXPECT_EQ(failure["tool_input"]["shell"], "/effective/shell");
  EXPECT_EQ(failure["cwd"], files.canonical(config.cwd).string());
  config.set_deny({"exec_command(effective)"});
  EXPECT_TRUE(tools.execute(call("exec_command", {{"cmd", "original"}}), cancel).error);
  EXPECT_TRUE(process.starts.empty());
}
TEST_F(Runtime, ApprovalHooksUseBashIdentityAndRejectInvalidRewriteContextAccurately) {
  Recording process;
  Json seen;
  config.hooks = {
      {"PermissionRequest", Json::array({{{"matcher", "Bash"}, {"command", "approve"}}})}};
  process.hook = [&](const ProcessSpec &spec) {
    seen = Json::parse(files.read(spec.argv.back()));
    return ProcessResult{0, R"({"hookSpecificOutput":{"decision":{"behavior":"allow"}}})"};
  };
  Tools tools(config, files, process, http, clock);
  EXPECT_FALSE(tools.execute(call("exec_command", {{"cmd", "accepted"}}), cancel).error);
  EXPECT_EQ(seen.at("tool_name"), "Bash");
  EXPECT_EQ(seen.at("tool_input").at("command"), "accepted");
  config.permission_mode = "bypass";
  config.hooks = {{"PreToolUse", Json::array({{{"command", "rewrite"}}})},
                  {"PostToolUseFailure", Json::array({{{"command", "failure"}}})}};
  process.hook = [&](const ProcessSpec &spec) {
    if (spec.argv[4] == "rewrite")
      return ProcessResult{
          0,
          R"({"hookSpecificOutput":{"permissionDecision":"allow","updatedInput":{"command":"rewritten","cwd":"missing-cwd"}}})"};
    seen = Json::parse(files.read(spec.argv.back()));
    return ProcessResult{};
  };
  EXPECT_TRUE(tools.execute(call("exec_command", {{"cmd", "original"}}), cancel).error);
  EXPECT_EQ(seen.at("tool_input").at("command"), "rewritten");
  EXPECT_EQ(seen.at("tool_input").at("cwd"), "missing-cwd");
}
TEST_F(Runtime, StdinRulesInspectInputAndKeepOriginAndPlanBlocksEof) {
  Recording process;
  process.running = true;
  config.permission_mode = "bypass";
  std::vector<PermissionRequest> approvals;
  Tools tools(config, files, process, http, clock, [&](const auto &r) {
    approvals.push_back(r);
    return true;
  });
  const auto start =
      tools.execute(call("exec_command", {{"cmd", "cat"}, {"yield_time_ms", 250}}), cancel);
  config.permission_mode = "default";
  config.set_deny({"write_stdin(*secret*)"});
  auto input = call(
      "write_stdin",
      {{"session_id", start.metadata.at("session_id")}, {"chars", "secret"}, {"yield_time_ms", 1}});
  EXPECT_TRUE(tools.execute(input, cancel).error);
  EXPECT_TRUE(process.writes.empty());
  input.arguments["chars"] = std::string("safe\0secret", 11);
  EXPECT_TRUE(tools.execute(input, cancel).error);
  input.arguments["chars"] = "safe";
  EXPECT_FALSE(tools.execute(input, cancel).error);
  ASSERT_EQ(approvals.size(), 1u);
  EXPECT_EQ(approvals[0].target, "safe");
  EXPECT_EQ(approvals[0].context.at("command"), "cat");
  EXPECT_EQ(approvals[0].context.at("cwd"), files.canonical(config.cwd).string());
  config.permission_mode = "plan";
  input.arguments["chars"] = "";
  input.arguments["close_stdin"] = true;
  EXPECT_TRUE(tools.execute(input, cancel).error);
  EXPECT_EQ(process.closes, 0u);
  input.arguments["close_stdin"] = false;
  process.running = false;
  EXPECT_FALSE(tools.execute(input, cancel).error);
  EXPECT_EQ(approvals.size(), 1u);
}
TEST_F(Runtime, PipeEofPublishesFinalOutputAndPtyRejectsBeforeWriting) {
  config.permission_mode = "bypass";
  Tools tools(config, files, processes, http, clock);
  const auto start = tools.execute(
      call("exec_command",
           {{"cmd", "cat"}, {"tty", false}, {"login", false}, {"yield_time_ms", 250}}),
      cancel);
  ASSERT_FALSE(start.error) << start.content;
  const auto end =
      tools.execute(call("write_stdin", {{"session_id", start.metadata.at("session_id")},
                                         {"chars", "hello"},
                                         {"close_stdin", true},
                                         {"yield_time_ms", 1000}}),
                    cancel);
  ASSERT_FALSE(end.error) << end.content;
  EXPECT_EQ(end.metadata.at("exit_code"), 0);
  EXPECT_THAT(end.metadata.at("output").get<std::string>(), HasSubstr("hello"));
  const auto repeated = tools.execute(
      call("write_stdin", {{"session_id", start.metadata.at("session_id")}, {"close_stdin", true}}),
      cancel);
  ASSERT_FALSE(repeated.error) << repeated.content;
  EXPECT_EQ(repeated.metadata.at("exit_code"), 0);
  EXPECT_TRUE(repeated.metadata.at("already_completed"));
  Recording recording;
  recording.running = true;
  Tools fake(config, files, recording, http, clock);
  const auto tty = fake.execute(
      call("exec_command", {{"cmd", "cat"}, {"tty", true}, {"yield_time_ms", 250}}), cancel);
  EXPECT_TRUE(fake.execute(call("write_stdin", {{"session_id", tty.metadata.at("session_id")},
                                                {"chars", "must not send"},
                                                {"close_stdin", true}}),
                           cancel)
                  .error);
  EXPECT_TRUE(recording.writes.empty());
}
TEST_F(Runtime, EofRetryDoesNotInventExitStatusAfterFailedCollection) {
  Recording process;
  process.running = true;
  config.permission_mode = "bypass";
  Tools tools(config, files, process, http, clock);
  const auto start =
      tools.execute(call("exec_command", {{"cmd", "cat"}, {"yield_time_ms", 250}}), cancel);
  ASSERT_FALSE(start.error);
  const auto id = start.metadata.at("session_id");
  process.poll_error = true;
  EXPECT_TRUE(
      tools
          .execute(call("write_stdin", {{"session_id", id}, {"chars", "x"}, {"yield_time_ms", 1}}),
                   cancel)
          .error);
  process.poll_error = false;
  const auto retry =
      tools.execute(call("write_stdin", {{"session_id", id}, {"close_stdin", true}}), cancel);
  EXPECT_TRUE(retry.error);
  EXPECT_FALSE(retry.metadata.contains("exit_code"));
}
struct OneCall : Provider {
  ToolCall tool;
  size_t requests = 0;
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink & = {}) override {
    ++requests;
    codet::Message message;
    message.role = "assistant";
    message.calls = {tool};
    return {message, {}, "tool_calls"};
  }
};
TEST_F(Runtime, CompletedPatchIsJournaledBeforeFatalPostHookWithoutReplay) {
  config.permission_mode = "accept-edits";
  config.hooks = {{"PostToolUse", Json::array({{{"command", "fatal"}}})}};
  Recording process;
  process.hook = [](const ProcessSpec &) -> ProcessResult {
    throw ProcessResourceError("exhausted");
  };
  Tools tools(config, files, process, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  OneCall provider;
  provider.tool =
      call("apply_patch", {{"patch", "*** Begin Patch\n*** Add File: once\n+kept\n*** End Patch"}});
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(session, "apply once", cancel), Error);
  EXPECT_EQ(provider.requests, 1u);
  EXPECT_EQ(files.read(config.cwd / "once"), "kept\n");
  const auto recovered = state.load_id(session.id);
  ASSERT_EQ(recovered.messages.back().role, "tool");
  const auto result = Json::parse(recovered.messages.back().text);
  EXPECT_TRUE(result.at("metadata").at("action_completed"));
  EXPECT_THAT(result.at("content").get<std::string>(), HasSubstr("Applied 1"));
  EXPECT_THAT(result.dump(), Not(HasSubstr("Tool failed")));
}
TEST_F(Runtime, CompletedExecRetainsItsOutcomeWhenPostHookExhaustsResources) {
  config.permission_mode = "bypass";
  config.hooks = {{"PostToolUse", Json::array({{{"command", "fatal"}}})}};
  Recording process;
  process.capture_error = std::string("invalid-") + char(0xff);
  process.output = std::string(1000, 'x');
  process.hook = [](const ProcessSpec &) -> ProcessResult {
    throw ProcessResourceError("exhausted");
  };
  Tools tools(config, files, process, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  OneCall provider;
  provider.tool = call("exec_command", {{"cmd", "completed command"}, {"max_output_tokens", 32}});
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(session, "run once", cancel), Error);
  ASSERT_EQ(process.starts.size(), 1u);
  const auto recovered = state.load_id(session.id);
  const auto result = Json::parse(recovered.messages.back().text);
  EXPECT_TRUE(result.at("metadata").at("action_completed"));
  EXPECT_THAT(result.at("content").get<std::string>(), HasSubstr("Tool completed"));
  EXPECT_THAT(result.dump(), Not(HasSubstr("Tool failed")));
  const auto envelope = Json::parse(result.at("content").get<std::string>());
  EXPECT_LE(envelope.at("output").get<std::string>().size(), 128u);
  EXPECT_EQ(envelope.at("original_token_count"), 250);
}
TEST_F(Runtime, StdinHookAllowRequiresExactNestedOperationScopeIncludingEof) {
  Recording process;
  process.running = true;
  config.permission_mode = "bypass";
  config.approval = "never";
  Json response = {{"hookSpecificOutput", {{"decision", {{"behavior", "allow"}}}}}};
  std::vector<Json> observed;
  process.hook = [&](const ProcessSpec &spec) {
    observed.push_back(Json::parse(files.read(spec.argv.back())));
    return ProcessResult{0, response.dump()};
  };
  Tools tools(config, files, process, http, clock);
  const auto start =
      tools.execute(call("exec_command", {{"cmd", "python3 -i"}, {"yield_time_ms", 250}}), cancel);
  ASSERT_FALSE(start.error);
  const auto id = start.metadata.at("session_id");
  config.permission_mode = "default";
  config.hooks = {
      {"PermissionRequest", Json::array({{{"matcher", "Bash"}, {"command", "check"}}})}};
  auto input =
      call("write_stdin", {{"session_id", id}, {"chars", "arbitrary input"}, {"yield_time_ms", 1}});
  for (const auto &invalid : {Json(), Json("exec_command"), Json("write_stdin*"), Json(1)}) {
    if (invalid.is_null())
      response["hookSpecificOutput"]["decision"].erase("tool");
    else
      response["hookSpecificOutput"]["decision"]["tool"] = invalid;
    EXPECT_TRUE(tools.execute(input, cancel).error);
    auto eof = call("write_stdin", {{"session_id", id}, {"close_stdin", true}});
    EXPECT_TRUE(tools.execute(eof, cancel).error);
  }
  EXPECT_TRUE(process.writes.empty());
  EXPECT_EQ(process.closes, 0u);
  ASSERT_FALSE(observed.empty());
  EXPECT_EQ(observed.back().at("tool"), "write_stdin");
  EXPECT_EQ(observed.back().at("tool_input").at("command"), "python3 -i");
  response["hookSpecificOutput"]["decision"]["tool"] = "write_stdin";
  EXPECT_FALSE(tools.execute(input, cancel).error);
  process.running = false;
  EXPECT_FALSE(
      tools.execute(call("write_stdin", {{"session_id", id}, {"close_stdin", true}}), cancel)
          .error);
  EXPECT_EQ(process.writes.size(), 1u);
  EXPECT_EQ(process.closes, 1u);
}
TEST_F(Runtime, PermissionMatcherAliasesOnlySelectTheirOwnOperationAndDenyAlwaysWins) {
  Recording process;
  config.approval = "never";
  std::vector<std::string> matched;
  process.hook = [&](const ProcessSpec &spec) {
    matched.push_back(spec.argv[4]);
    return ProcessResult{
        0, R"({"hookSpecificOutput":{"decision":{"behavior":"allow","tool":"exec_command"}}})"};
  };
  config.hooks = {
      {"PermissionRequest", Json::array({{{"matcher", "write_stdin"}, {"command", "stdin-only"}},
                                         {{"matcher", "exec_command"}, {"command", "exec-only"}},
                                         {{"matcher", "Ba*"}, {"command", "bash-glob"}}})}};
  Tools tools(config, files, process, http, clock);
  EXPECT_FALSE(tools.execute(call("exec_command", {{"cmd", "safe"}}), cancel).error);
  EXPECT_EQ(matched, (std::vector<std::string>{"exec-only", "bash-glob"}));
  config.hooks["PermissionRequest"].push_back({{"matcher", "Bash"}, {"command", "deny"}});
  process.hook = [](const ProcessSpec &spec) {
    return ProcessResult{
        0, spec.argv[4] == "deny"
               ? R"({"hookSpecificOutput":{"decision":{"behavior":"deny","message":"blocked"}}})"
               : R"({"hookSpecificOutput":{"decision":{"behavior":"allow"}}})"};
  };
  EXPECT_TRUE(tools.execute(call("exec_command", {{"cmd", "safe"}}), cancel).error);
}
TEST_F(Runtime, GlobalOutputLimitStillReportsOriginalTokenCount) {
  config.permission_mode = "bypass";
  config.tool_output_limit = 256;
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute(
      call("exec_command", {{"cmd", "printf '" + std::string(1000, 'x') + "'"}, {"login", false}}),
      cancel);
  ASSERT_FALSE(result.error);
  EXPECT_TRUE(result.metadata.at("truncated"));
  EXPECT_EQ(result.metadata.at("original_token_count"), 250);
}
TEST_F(Runtime, ScalarSchemasAndObsoleteHookMatchersAreExplicit) {
  nlohmann::json_schema::json_validator validator;
  validator.set_root_schema(tool_definition("computer.set_value")->schema);
  Json args = {{"snapshot_id", "s"}, {"element_id", "e"}, {"value", true}};
  for (const Json &value : {Json(true), Json(1.5), Json("text")}) {
    args["value"] = value;
    EXPECT_NO_THROW(validator.validate(args));
  }
  for (const Json &value : {Json(), Json::object(), Json::array()}) {
    args["value"] = value;
    EXPECT_THROW(validator.validate(args), std::exception);
  }
  EXPECT_THROW(validate_hook_matchers({{"PreToolUse", Json::array({{{"matcher", "Read"}}})}}),
               Error);
  EXPECT_NO_THROW(validate_hook_matchers(
      {{"PreToolUse", Json::array({{{"matcher", "Bash"}, {"command", "true"}},
                                   {{"matcher", "computer.*"}, {"command", "true"}}})}}));
}
TEST_F(Runtime, MalformedHookEntriesAreConfigurationErrorsForEveryEvent) {
  for (const auto *event : {"PreToolUse", "Stop", "SessionStart", "PermissionRequest"}) {
    for (const auto &entry : {Json("echo hi"), Json::object(), Json({{"command", 1}}),
                              Json({{"command", "true"}, {"matcher", false}}),
                              Json({{"command", "true"}, {"timeout_ms", "slow"}}),
                              Json({{"command", "true"}, {"timeout_ms", uint64_t{1} << 40}})}) {
      try {
        validate_hook_matchers({{event, Json::array({entry})}});
        FAIL() << event << entry;
      } catch (const Error &error) {
        EXPECT_EQ(error.code, 2);
      }
    }
  }
  files.atomic_write(config.home / "config.toml", "[hooks]\nPreToolUse=['echo hi']\n");
  try {
    load_config(config.home, config.cwd, "", {}, files);
    FAIL();
  } catch (const Error &error) {
    EXPECT_EQ(error.code, 2);
    EXPECT_THAT(error.what(), HasSubstr("Invalid hook entry"));
  }
}
TEST_F(Runtime, WorkerDeliveryRetainsRemainingEventsOnUiFailureAndExpiresApprovals) {
  Interaction input;
  input.begin_turn("first");
  input.post({{"type", "ui.failure"}});
  auto work = std::async(std::launch::async, [&] {
    return input.request("codet.approval", {{"tool", "exec_command"}}, cancel);
  });
  std::vector<Json> staged;
  std::string request;
  for (int i = 0; i < 1000 && request.empty(); ++i) {
    for (const auto &event : input.events()) {
      staged.push_back(event);
      if (event.contains("request_id"))
        request = event.at("request_id");
    }
    if (request.empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  for (const auto &event : staged)
    input.post(event);
  input.post({{"type", "codet.diagnostic"}, {"text", "recovery diagnostic"}});
  EXPECT_THROW(input.deliver([](const Json &) { throw Error("UI failed"); }), Error);
  input.end_turn();
  EXPECT_THROW(work.get(), Error);
  ASSERT_FALSE(staged.empty());
  ASSERT_FALSE(request.empty());
  EXPECT_FALSE(input.answer(request, "yes"));
  input.post({{"type", "turn.failed"}});
  std::vector<std::string> delivered;
  input.deliver(
      [&](const Json &event) {
        const auto type = event.at("type").get<std::string>();
        if (type == "ui.failure")
          throw Error("UI still unavailable");
        delivered.push_back(type);
      },
      [](const std::string &) {}, true);
  EXPECT_EQ(delivered, (std::vector<std::string>{"codet.diagnostic", "turn.failed"}));
  EXPECT_TRUE(input.events().empty());
  input.begin_turn("second");
  EXPECT_FALSE(input.answer(request, "yes"));
  EXPECT_FALSE(input.current_event(staged.front()));
}
TEST_F(Runtime, InvalidWorkerEventDoesNotDiscardTheFollowingOutcome) {
  Interaction input;
  input.begin_turn("current");
  input.post({{"type", "bad"}, {"text", 42}});
  input.post({{"type", "turn.failed"}});
  size_t warnings = 0, outcomes = 0;
  input.deliver(
      [&](const Json &event) {
        if (event.at("type") == "bad")
          (void)event.at("text").get<std::string>();
        else
          ++outcomes;
      },
      [&](const std::string &) { ++warnings; });
  EXPECT_EQ(warnings, 1u);
  EXPECT_EQ(outcomes, 1u);
}
TEST_F(Runtime, CaptureRejectsInvalidDisplayBoundsBeforeImageArithmetic) {
  for (const auto &display : {CaptureRect{0, 0, 0, 100}, CaptureRect{0, 0, 100, -1},
                              CaptureRect{0, 0, std::numeric_limits<double>::infinity(), 100},
                              CaptureRect{std::numeric_limits<double>::quiet_NaN(), 0, 100, 100}}) {
    EXPECT_THAT([&] { capture_area({display}, Json::object()); },
                ThrowsMessage<Error>(HasSubstr("Invalid display bounds")));
  }
}
TEST_F(Runtime, PendingInputCannotLeakAcrossTurnOrSessionBoundaries) {
  Interaction input;
  input.begin_turn("first");
  input.submit("before completion");
  input.end_turn();
  input.submit("after completion");
  EXPECT_TRUE(input.take_input().empty());
  EXPECT_THROW(input.begin_turn("second"), Error);
  EXPECT_EQ(input.take_pending("first"),
            (std::vector<std::string>{"before completion", "after completion"}));
  input.begin_turn("second");
  EXPECT_TRUE(input.take_input().empty());
  for (int i = 0; i < 100; ++i)
    input.submit(std::to_string(i));
  EXPECT_THROW(input.submit("overflow"), Error);
  EXPECT_EQ(input.take_input().size(), 100u);
}
TEST_F(Runtime, ScreenshotSelectsOneDisplayAndRejectsSpansOrMixedSelectors) {
  const std::vector<CaptureRect> displays = {{0, 0, 1280, 800}, {-1024, 100, 1024, 768}};
  const auto left =
      capture_area(displays, {{"window_id", "observed"}}, CaptureRect{-900, 120, 500, 300});
  EXPECT_EQ(left.display, 1u);
  EXPECT_DOUBLE_EQ(left.bounds.x, -900);
  const auto clipped =
      capture_area(displays, {{"x", -1100}, {"y", 120}, {"width", 500}, {"height", 300}});
  EXPECT_DOUBLE_EQ(clipped.bounds.x, -1024);
  EXPECT_DOUBLE_EQ(clipped.bounds.width, 424);
  EXPECT_THROW(capture_area(displays, {{"window_id", "w"}}, CaptureRect{-20, 200, 100, 200}),
               Error);
  EXPECT_THROW(capture_area(displays, {{"display_id", 1}, {"window_id", "w"}}), Error);
  EXPECT_THROW(capture_area(displays, {{"x", 1}}), Error);
  EXPECT_THROW(capture_area(displays, {{"display_id", 0}}), Error);
}
TEST_F(Runtime, EphemeralArtifactRootIsUsedForPersistenceAndReplay) {
  config.artifact_root = temp.root / "ephemeral" / "artifacts";
  const auto image = persist_image(config, files, {"encoded", "image/png", 1, 1}, "high");
  EXPECT_TRUE(within(image.path, config.artifact_root));
  EXPECT_FALSE(fs::exists(config.home / "artifacts"));
  EXPECT_EQ(image_data_url(image, config), "data:image/png;base64,ZW5jb2RlZA==");
  StateStore state(config.artifact_root.parent_path() / "state", config.artifact_root);
  auto session = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(session.id);
  codet::Message message;
  message.role = "system";
  message.message_id = unique_id();
  message.images = {image};
  message.artifact_index = tools.archive_images({message}).string();
  EXPECT_TRUE(within(message.artifact_index, config.artifact_root));
  session.messages = {message};
  state.save(session);
  const auto fork = state.fork(session);
  EXPECT_EQ(fork.messages[0].artifact_index, artifact_index_reference(fork.id));
  EXPECT_NE(fork.messages[0].artifact_index, message.artifact_index);
  fs::remove_all(config.artifact_root.parent_path());
  EXPECT_THROW(image_data_url(image, config), Error);
}
TEST_F(Runtime, MissingImageNotesPreserveToolEnvelopeAndAnthropicTextAppearsOnce) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  const auto image = persist_image(config, files, {"encoded", "image/png", 1, 1}, "high");
  codet::Message summary;
  summary.role = "system";
  summary.text = "unique summary text";
  summary.images = {image};
  codet::Message tool;
  tool.role = "tool";
  tool.tool_call_id = "shot";
  tool.text = R"({"content":"observed result","is_error":false,"metadata":{"unchanged":true}})";
  tool.images = {image};
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"shot", "computer.screenshot", Json::object()}};
  for (const auto *name : {"codex", "openai", "anthropic", "google", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fake";
    config.providers[name].image_input = true;
    const auto body = provider
                          .request(config, "instructions", {summary, assistant, tool},
                                   Json::array(), {{"access_token", "fake"}})
                          .body;
    const auto at = body.find("unique summary text");
    EXPECT_NE(at, std::string::npos) << name;
    EXPECT_EQ(body.find("unique summary text", at + 1), std::string::npos) << name;
  }
  fs::remove(image.path);
  config.provider = "openai";
  const auto body = provider.request(config, "instructions", {assistant, tool}, Json::array()).body;
  EXPECT_THAT(body, HasSubstr("observed result"));
  EXPECT_THAT(body, HasSubstr("Image artifact"));
  EXPECT_THAT(body, Not(HasSubstr("unchanged")));
  EXPECT_TRUE(Json::parse(tool.text).at("metadata").at("unchanged"));
}
TEST_F(Runtime, CaptureAccountingIsLinearAndPendingUtf8DoesNotSpill) {
  auto store = std::make_shared<CaptureStore>(temp.root / "output");
  OutputCapture output(store, 16 * 1024 * 1024);
  for (int i = 0; i < 512; ++i)
    output.append(std::string(8192, 'x'));
  EXPECT_EQ(store->metrics.display_bytes, 4u * 1024 * 1024);
  OutputCapture tiny(store, 512);
  tiny.append("hi\xf0\x9f");
  const auto live = tiny.render(512);
  EXPECT_EQ(live.text, "hi");
  EXPECT_EQ(live.omitted, 0u);
  EXPECT_EQ(store->metrics.files, 0u);
  tiny.append("\x8c\xb2");
  tiny.finish();
  EXPECT_EQ(tiny.render(512).text, "hi🌲");
}
TEST_F(Runtime, StartupDescriptorExhaustionIsFatal) {
  const auto child = fork();
  ASSERT_GE(child, 0);
  if (!child) {
    struct rlimit limit {
      3, 3
    };
    if (setrlimit(RLIMIT_NOFILE, &limit))
      _exit(2);
    NativeProcesses owned;
    ProcessSpec spec;
    spec.argv = {"/bin/sh", "-c", "true"};
    try {
      owned.start(spec);
    } catch (const ProcessResourceError &) {
      _exit(0);
    } catch (...) {
      _exit(3);
    }
    _exit(4);
  }
  int status = 0;
  ASSERT_EQ(waitpid(child, &status, 0), child);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

struct ObservedProcesses : Processes {
  NativeProcesses native;
  std::shared_ptr<CaptureStore> store;
  size_t observations = 0;
  ProcessResult run(const ProcessSpec &s, const Cancellation &c) override {
    return native.run(s, c);
  }
  std::string start(const ProcessSpec &s) override {
    store = s.capture_store;
    return native.start(s);
  }
  ProcessResult poll(const std::string &id) override {
    return native.poll(id);
  }
  ProcessStatus status(const std::string &id) override {
    ++observations;
    return native.status(id);
  }
  ProcessResult take_output(const std::string &id) override {
    return native.take_output(id);
  }
  void stop(const std::string &id) override {
    native.stop(id);
  }
};
TEST_F(Runtime, ActualExecWaitLoopDoesNotRenderEveryObservation) {
  ObservedProcesses owned;
  config.permission_mode = "bypass";
  Tools tools(config, files, owned, http, clock);
  const auto result = tools.execute(
      call("exec_command", {{"cmd", "printf ready; sleep 0.05"}, {"login", false}}), cancel);
  ASSERT_FALSE(result.error) << result.content;
  ASSERT_NE(owned.store, nullptr);
  EXPECT_GT(owned.observations, 1u);
  EXPECT_EQ(owned.store->metrics.renders, 0u);
  EXPECT_EQ(result.metadata.at("output"), "ready");
}
struct FailingRead : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  bool fail = false;
  int watch(pid_t p) override {
    return native->watch(p);
  }
  bool exited(pid_t p, int fd) override {
    return native->exited(p, fd);
  }
  void signal(pid_t p, int sig) override {
    native->signal(p, sig);
  }
  int reap(pid_t p) override {
    return native->reap(p);
  }
  ssize_t read_output(int fd, void *data, size_t n) override {
    if (fail)
      throw Error("injected drain failure");
    return native->read_output(fd, data, n);
  }
};
TEST_F(Runtime, DrainFailureStillFinalizesBufferedCapture) {
  auto control = std::make_shared<FailingRead>();
  NativeProcesses owned(control);
  auto store = std::make_shared<CaptureStore>(temp.root / "captures");
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "printf '%08000d' 0; sleep 30"};
  spec.output_limit = 512;
  spec.capture_store = store;
  const auto id = owned.start(spec);
  for (int i = 0; i < 100 && store->metrics.display_bytes < 8000; ++i) {
    owned.status(id);
    clock.sleep_ms(5, cancel);
  }
  ASSERT_EQ(store->metrics.display_bytes, 8000u);
  EXPECT_EQ(store->metrics.syncs, 0u);
  control->fail = true;
  EXPECT_NO_THROW(owned.stop(id));
  const auto result = owned.poll(id);
  EXPECT_FALSE(result.running);
  ASSERT_FALSE(result.spill_path.empty());
  EXPECT_EQ(files.read(result.spill_path).size(), 8000u);
  EXPECT_EQ(store->metrics.syncs, 1u);
}
struct Desktop : ComputerBackend {
  size_t calls = 0;
  Json capabilities() const override {
    return {{"available", true}, {"accessibility", true}, {"applescript", true}};
  }
  DesktopResult invoke(const std::string &, const Json &, const Cancellation &) override {
    ++calls;
    return {{{"action_sent", true}}, {}};
  }
};
TEST_F(Runtime, ComputerRulesHaveOneTargetAndRemainIndependentOfExecRules) {
  auto desktop = std::make_shared<Desktop>();
  config.set_allow({"computer.*"});
  config.set_deny({"exec_command", "computer.type_text(secret)", "computer.applescript(secret)"});
  Recording process;
  size_t hooks = 0;
  process.hook = [&](const ProcessSpec &) {
    ++hooks;
    return ProcessResult{};
  };
  config.hooks = {{"PreToolUse", Json::array({{{"command", "record"}}})}};
  Tools tools(config, files, process, http, clock, {}, {}, {}, {}, desktop);
  EXPECT_TRUE(tools.execute(call("computer.type_text", {{"text", "secret"}}), cancel).error);
  EXPECT_TRUE(tools.execute(call("computer.applescript", {{"script", "secret"}}), cancel).error);
  EXPECT_EQ(hooks, 0u);
  EXPECT_EQ(desktop->calls, 0u);
  EXPECT_FALSE(
      tools.execute(call("computer.applescript", {{"script", "do shell script \"true\""}}), cancel)
          .error);
  EXPECT_EQ(desktop->calls, 1u);
}
TEST_F(Runtime, AccessibilityActionNulCannotAuthorizeDifferentDispatchText) {
  auto desktop = std::make_shared<Desktop>();
  config.set_allow({"computer.*"});
  Json args = {{"snapshot_id", "snapshot"}, {"element_id", "button"}, {"action", "AXPress"}};
  config.set_deny({"computer.perform_action(" + args.dump() + ")"});
  Tools tools(config, files, processes, http, clock, {}, {}, {}, {}, desktop);
  EXPECT_TRUE(tools.execute(call("computer.perform_action", args), cancel).error);
  args["action"] = std::string("AXPress\0ignored", 15);
  EXPECT_TRUE(tools.execute(call("computer.perform_action", args), cancel).error);
  EXPECT_EQ(desktop->calls, 0u);
}
TEST_F(Runtime, NumericOptionLabelsAreMappedOnlyOnce) {
  config.permission_mode = "plan";
  Tools tools(config, files, processes, http, clock, {},
              [](const std::string &, const std::vector<std::string> &) { return "1"; });
  Json question = {{"id", "pick"},
                   {"header", "Choice"},
                   {"question", "Pick"},
                   {"options", Json::array({{{"label", "2"}, {"description", "first"}},
                                            {{"label", "x"}, {"description", "second"}}})}};
  const auto result =
      tools.execute(call("request_user_input", {{"questions", Json::array({question})}}), cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(Json::parse(result.content)["answers"]["pick"]["answers"][0], "2");
}
struct FinalProvider : Provider {
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink & = {}) override {
    codet::Message message;
    message.role = "assistant";
    message.text = "done";
    return {message, {}, "stop"};
  }
};
TEST_F(Runtime, FatalStopHookAbortsWithoutBecomingOrdinaryBlockingFeedback) {
  config.hooks = {{"Stop", Json::array({{{"command", "fatal"}}})}};
  Recording process;
  process.hook = [](const ProcessSpec &) -> ProcessResult {
    throw ProcessResourceError("exhausted");
  };
  Tools tools(config, files, process, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  FinalProvider provider;
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(session, "finish", cancel), Error);
  const auto recovered = state.load_id(session.id);
  ASSERT_EQ(recovered.messages.size(), 2u);
  EXPECT_EQ(recovered.messages.back().text, "done");
}
TEST_F(Runtime, SteeringAtCompletionBecomesPendingForTheSameSession) {
  auto input = std::make_shared<Interaction>();
  Tools tools(config, files, processes, http, clock, {}, {}, {}, input);
  StateStore state(config.home);
  auto session = state.create();
  FinalProvider provider;
  Agent agent(config, provider, tools, state, [&](const Json &event) {
    if (event.value("type", "") == "turn.completed")
      input->submit("late steering");
  });
  EXPECT_EQ(agent.run(session, "first", cancel).text, "done");
  EXPECT_TRUE(input->take_input().empty());
  EXPECT_EQ(input->take_pending(session.id), (std::vector<std::string>{"late steering"}));
}
} // namespace

TEST(InteractionAdmission, ExactlyEmptyMessagesAreNeverQueued) {
  codet::Interaction input;
  input.begin_turn("session");
  input.submit("");
  EXPECT_TRUE(input.take_input().empty());
  input.submit(" ");
  EXPECT_EQ(input.take_input(), std::vector<std::string>{" "});
  input.end_turn();
  input.submit("");
  EXPECT_TRUE(input.take_pending("session").empty());
}

TEST(InteractionWarnings, CoalescesWorkerFailuresAndRetriesOnMainThreadOnly) {
  codet::Interaction input;
  auto producer = std::async(std::launch::async, [&] {
    for (int i = 0; i < 1000; ++i) {
      input.warn(codet::Interaction::Warning::exec_stop);
      input.warn(codet::Interaction::Warning::exec_original);
    }
  });
  producer.get();
  size_t attempts = 0;
  input.drain_warnings([&](std::string_view) {
    ++attempts;
    return false;
  });
  EXPECT_EQ(attempts, 2u);
  std::vector<std::string> received;
  input.drain_warnings([&](std::string_view message) {
    received.emplace_back(message);
    return true;
  });
  ASSERT_EQ(received.size(), 2u);
  EXPECT_EQ(received[0], "Exec cleanup failed while stopping owned process");
  EXPECT_EQ(received[1], "Exec cleanup failed; preserving original error");
  input.drain_warnings([](std::string_view) {
    ADD_FAILURE();
    return true;
  });
  input.post({{"type", "invalid"}, {"text", 42}});
  input.deliver([](const codet::Json &event) { (void)event.at("text").get<std::string>(); },
                [](const std::string &) { throw std::bad_alloc(); });
  attempts = 0;
  input.drain_warnings([&](std::string_view warning) {
    EXPECT_EQ(warning, "Worker event diagnostic could not be displayed");
    ++attempts;
    return true;
  });
  EXPECT_EQ(attempts, 1u);
}
