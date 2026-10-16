#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/interaction.hpp"
#include "codet/prompt.hpp"
#include "codet/tool_registry.hpp"
#include "core_contract.hpp"
#include "prompt_support.hpp"
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
struct NoHttp : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected network request");
  }
};
std::string png() {
  const std::string b64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/"
                          "x8AAwMCAO+jRZkAAAAASUVORK5CYII=";
  std::string out(b64.size(), '\0');
  const int count =
      EVP_DecodeBlock(reinterpret_cast<unsigned char *>(&out[0]),
                      reinterpret_cast<const unsigned char *>(b64.data()), b64.size());
  out.resize(static_cast<size_t>(count) - 1);
  return out;
}
struct Images : ImageBackend {
  bool available() const override {
    return true;
  }
  ImageData load(const fs::path &, const std::string &) override {
    return {png(), "image/png", 1, 1};
  }
};
struct Desktop : ComputerBackend {
  std::vector<std::pair<std::string, Json>> calls;
  bool fail = false;
  Json facts = codet::test::desktop_facts();
  Json capabilities() const override {
    return facts;
  }
  DesktopResult invoke(const std::string &name, const Json &args, const Cancellation &c) override {
    c.check();
    calls.emplace_back(name, args);
    if (fail)
      throw Error("Expired snapshot; refresh state");
    if (name == "screenshot")
      return {{{"screenshot_id", "shot"}, {"width", 1}, {"height", 1}},
              ImageData{png(), "image/png", 1, 1}};
    return {{{"action_sent", true}}, {}};
  }
};
struct Contracts : Test {
  fs::path root;
  Config config;
  NativeFileSystem files;
  NativeProcesses processes;
  NoHttp http;
  SystemClock clock;
  Cancellation cancel;
  std::shared_ptr<Images> images = std::make_shared<Images>();
  std::shared_ptr<Desktop> desktop = std::make_shared<Desktop>();
  void SetUp() override {
    std::string pattern = (fs::temp_directory_path() / "codet-contract-XXXXXX").string();
    std::vector<char> p(pattern.begin(), pattern.end());
    p.push_back(0);
    root = mkdtemp(p.data());
    config.home = root / "state";
    config.cwd = root / "work";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "fixture";
    config.providers["openai"].api_key = "fixture";
  }
  void TearDown() override {
    fs::remove_all(root);
  }
  Tools tools(std::shared_ptr<Interaction> channel = {}) {
    return Tools(config, files, processes, http, clock, {}, {}, {}, channel, desktop, images);
  }
  ToolCall call(const std::string &name, Json args = Json::object()) {
    return {unique_id(), name, args};
  }
};
TEST_F(Contracts, RegistryNamesRequiredFieldsAndModeGatesMatchFrozenContract) {
  auto t = tools();
  std::set<std::string> seen;
  for (const auto &schema : t.schemas()) {
    const auto name = schema.at("name").get<std::string>();
    EXPECT_TRUE(seen.insert(name).second);
    EXPECT_NE(tool_definition(name), nullptr);
  }
  for (const auto &name :
       {"exec_command", "write_stdin", "apply_patch", "view_image", "update_plan",
        "clock.curr_time", "clock.sleep", "computer.get_state", "computer.screenshot"})
    EXPECT_TRUE(seen.count(name)) << name;
  for (const auto &name :
       {"Read", "Edit", "Write", "Bash", "TaskCreate", "ApplyPatch", "Memory", "Skill", "WebSearch",
        "read_file", "request_user_input", "request_user_input_async"})
    EXPECT_FALSE(seen.count(name)) << name;
  const auto fixture = Json::parse(core_contract_json);
  for (auto it = fixture.begin(); it != fixture.end(); ++it) {
    const auto *d = tool_definition(it.key());
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->schema.value("required", Json::array()), it.value());
  }
  config.permission_mode = "plan";
  seen.clear();
  for (const auto &s : t.schemas())
    seen.insert(s.at("name"));
  EXPECT_TRUE(seen.count("read_file"));
  EXPECT_FALSE(seen.count("update_plan"));
  EXPECT_TRUE(t.execute(call("exec_command", {{"cmd", "touch denied"}}), cancel).error);
  EXPECT_FALSE(fs::exists(config.cwd / "denied"));
}
TEST_F(Contracts, LegacyCallsAndPermissionRulesFailExplicitly) {
  auto t = tools();
  for (const auto &name :
       {"Read", "Write", "Edit", "Bash", "ApplyPatch", "TodoWrite", "TaskList", "Memory", "Skill"})
    EXPECT_TRUE(t.execute(call(name), cancel).error) << name;
  EXPECT_THROW(validate_tool_rules({"Bash(*)"}), Error);
  EXPECT_NO_THROW(validate_tool_rules({"exec_command(*)", "computer.*", "apply_patch"}));
}
TEST_F(Contracts, PlanInspectionListsDirectoriesAndFiltersDeniedSymlinkTargets) {
  config.permission_mode = "plan";
  files.atomic_write(config.cwd / "public", "one\ntwo\n");
  files.atomic_write(config.cwd / "secret", "hidden");
  fs::create_symlink(config.cwd / "secret", config.cwd / "alias");
  config.set_deny({"read_file(*secret*)"});
  auto t = tools();
  auto listing = t.execute(call("read_file", {{"path", "."}}), cancel);
  ASSERT_FALSE(listing.error) << listing.content;
  const auto list = Json::parse(listing.content).at("entries");
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0]["name"], "public");
  EXPECT_TRUE(t.execute(call("read_file", {{"path", "alias"}}), cancel).error);
  const auto read =
      t.execute(call("read_file", {{"path", "public"}, {"offset", 2}, {"limit", 1}}), cancel);
  EXPECT_EQ(read.content, "2\ttwo\n");
  EXPECT_TRUE(t.execute(call("read_file", {{"path", "../outside"}}), cancel).error);
}
TEST_F(Contracts, PatchNeedsNoPriorReadAndValidatesAllChangesBeforeMutation) {
  config.permission_mode = "accept-edits";
  files.atomic_write(config.cwd / "a", "before\n");
  auto t = tools();
  auto result =
      t.execute(call("apply_patch",
                     {{"patch", "*** Begin Patch\n*** Update File: a\n@@\n-before\n+after\n*** Add "
                                "File: b\n+new\n*** End Patch\n"}}),
                cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(files.read(config.cwd / "a"), "after\n");
  result = t.execute(
      call("apply_patch", {{"patch", "*** Begin Patch\n*** Update File: a\n@@\n-after\n+bad\n*** "
                                     "Update File: b\n@@\n-missing\n+bad\n*** End Patch\n"}}),
      cancel);
  EXPECT_TRUE(result.error);
  EXPECT_EQ(files.read(config.cwd / "a"), "after\n");
}
TEST_F(Contracts, ExecYieldsNumericSessionAndReadsOnlyNewOutputThroughPipes) {
  config.permission_mode = "bypass";
  auto t = tools();
  auto first = t.execute(
      call("exec_command", {{"cmd", "printf first; read answer; printf 'second:%s' \"$answer\""},
                            {"login", false},
                            {"yield_time_ms", 250}}),
      cancel);
  ASSERT_FALSE(first.error) << first.content;
  auto data = Json::parse(first.content);
  ASSERT_TRUE(data.contains("session_id"));
  EXPECT_EQ(data["output"], "first");
  const int id = data.at("session_id");
  auto second = t.execute(call("write_stdin", {{"session_id", id}, {"chars", "reply\n"}}), cancel);
  ASSERT_FALSE(second.error) << second.content;
  data = Json::parse(second.content);
  EXPECT_EQ(data["output"], "second:reply");
  EXPECT_EQ(data["exit_code"], 0);
  EXPECT_FALSE(data.contains("session_id"));
  EXPECT_TRUE(t.execute(call("write_stdin", {{"session_id", id}}), cancel).error);
}
TEST_F(Contracts, IncrementalCollectionsPreserveSplitUnicodeAndStructuredSmallBudgets) {
  config.permission_mode = "bypass";
  auto t = tools();
  const auto first = t.execute(
      call("exec_command", {{"cmd", "printf '\\342'; read answer; printf '\\202\\254TAIL'"},
                            {"login", false},
                            {"yield_time_ms", 250}}),
      cancel);
  ASSERT_FALSE(first.error) << first.content;
  const auto pending = Json::parse(first.content);
  EXPECT_EQ(pending.at("output"), "");
  const auto next = t.execute(
      call("write_stdin", {{"session_id", pending.at("session_id")}, {"chars", "go\n"}}), cancel);
  ASSERT_FALSE(next.error) << next.content;
  EXPECT_EQ(Json::parse(next.content).at("output"), "€TAIL");
  const auto small = t.execute(call("exec_command", {{"cmd", "printf abcdefghijklmnopqrstuvwxyz"},
                                                     {"login", false},
                                                     {"max_output_tokens", 1}}),
                               cancel);
  ASSERT_FALSE(small.error) << small.content;
  EXPECT_TRUE(Json::accept(small.content));
  EXPECT_LE(Json::parse(small.content).at("output").get<std::string>().size(), 4u);
}
TEST_F(Contracts, ExecSupportsWorkdirTtyAndNonzeroStatusWithoutToolFailure) {
  config.permission_mode = "bypass";
  fs::create_directories(config.cwd / "nested");
  auto t = tools();
  auto result = t.execute(call("exec_command", {{"cmd", "pwd; test -t 0; exit 7"},
                                                {"workdir", "nested"},
                                                {"tty", true},
                                                {"login", false}}),
                          cancel);
  ASSERT_FALSE(result.error) << result.content;
  const auto data = Json::parse(result.content);
  EXPECT_EQ(data["exit_code"], 7);
  EXPECT_THAT(data.at("output").get<std::string>(), HasSubstr("nested"));
}
TEST_F(Contracts, InvalidBudgetsAndEscalationDoNotStartCommands) {
  config.permission_mode = "bypass";
  auto t = tools();
  EXPECT_TRUE(
      t.execute(call("exec_command", {{"cmd", "touch effect"}, {"max_output_tokens", 0}}), cancel)
          .error);
  EXPECT_TRUE(t.execute(call("exec_command", {{"cmd", "touch effect"},
                                              {"sandbox_permissions", "require_escalated"}}),
                        cancel)
                  .error);
  EXPECT_FALSE(fs::exists(config.cwd / "effect"));
}
TEST_F(Contracts, CancellingYieldWaitTerminatesOwnedGroup) {
  config.permission_mode = "bypass";
  auto t = tools();
  auto work = std::async(std::launch::async, [&] {
    try {
      t.execute(call("exec_command",
                     {{"cmd", "touch started; while :; do sleep .1; done"}, {"login", false}}),
                cancel);
      return 0;
    } catch (const Error &e) {
      return e.code;
    }
  });
  for (int i = 0; i < 200 && !fs::exists(config.cwd / "started"); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  cancel.requested = true;
  EXPECT_EQ(work.get(), 130);
  for (const auto &process : processes.list())
    EXPECT_FALSE(process.running);
}
TEST_F(Contracts, PlanPersistsAndRejectsMultipleActiveSteps) {
  auto t = tools();
  t.use_session("fixture");
  auto result =
      t.execute(call("update_plan",
                     {{"plan", Json::array({{{"step", "implement"}, {"status", "in_progress"}}})}}),
                cancel);
  EXPECT_FALSE(result.error);
  EXPECT_EQ(Json::parse(files.read(config.home / "plans/fixture.json"))[0]["step"], "implement");
  result = t.execute(
      call("update_plan", {{"plan", Json::array({{{"step", "one"}, {"status", "in_progress"}},
                                                 {{"step", "two"}, {"status", "in_progress"}}})}}),
      cancel);
  EXPECT_TRUE(result.error);
}
TEST_F(Contracts, SynchronousAnswersKeepQuestionIdsAndAllowFreeText) {
  config.permission_mode = "plan";
  Tools t(config, files, processes, http, clock, {},
          [](const std::string &, const std::vector<std::string> &) { return "a custom answer"; });
  const auto request = Json::parse(
      R"({"questions":[{"id":"choice","header":"Choice","question":"Which?","options":[{"label":"A","description":"first"},{"label":"B","description":"second"}]}]})");
  auto result = t.execute(call("request_user_input", request), cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(Json::parse(result.content)["answers"]["choice"]["answers"][0], "a custom answer");
}
TEST_F(Contracts, AsyncQuestionsReturnWithoutInventingAnswersAndSleepWakes) {
  auto channel = std::make_shared<Interaction>();
  auto t = tools(channel);
  auto result = t.execute(
      call("request_user_input_async",
           {{"questions", Json::array({{{"title", "Pick one"}, {"options", {"A", "B"}}}})}}),
      cancel);
  ASSERT_FALSE(result.error);
  EXPECT_EQ(Json::parse(result.content), Json({{"accepted", true}}));
  EXPECT_TRUE(channel->take_input().empty());
  ASSERT_EQ(channel->events().size(), 1u);
  auto wait = std::async(std::launch::async, [&] {
    return t.execute(call("clock.sleep", {{"duration_ms", 10000}}), cancel);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  channel->submit("B");
  result = wait.get();
  EXPECT_TRUE(Json::parse(result.content)["interrupted"].get<bool>());
  EXPECT_EQ(channel->take_input(), std::vector<std::string>{"B"});
}
TEST_F(Contracts, InteractionRequestsTransferAnswersAcrossThreads) {
  auto channel = std::make_shared<Interaction>();
  auto future = std::async(std::launch::async, [&] {
    return channel->request("question", {{"text", "hello"}}, cancel);
  });
  std::vector<Json> events;
  for (int i = 0; i < 100 && events.empty(); ++i) {
    events = channel->events();
    if (events.empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_EQ(events.size(), 1u);
  channel->answer(events[0].at("request_id"), "answer");
  EXPECT_EQ(future.get(), "answer");
}
TEST_F(Contracts, DesktopObservationsAndMutationsUseDistinctPermissionsAndReportErrors) {
  auto t = tools();
  EXPECT_FALSE(t.execute(call("computer.get_state"), cancel).error);
  EXPECT_EQ(desktop->calls.size(), 1u);
  EXPECT_TRUE(t.execute(call("computer.key", {{"key", "enter"}}), cancel).error);
  EXPECT_EQ(desktop->calls.size(), 1u);
  config.permission_mode = "bypass";
  EXPECT_FALSE(t.execute(call("computer.key", {{"key", "enter"}}), cancel).error);
  config.set_deny({"computer.key"});
  EXPECT_TRUE(t.execute(call("computer.key", {{"key", "enter"}}), cancel).error);
  config.set_deny({});
  desktop->fail = true;
  EXPECT_THAT(
      t.execute(call("computer.perform_action",
                     {{"snapshot_id", "stale"}, {"element_id", "node"}, {"action", "AXPress"}}),
                cancel)
          .content,
      HasSubstr("Expired snapshot"));
}
TEST_F(Contracts, ScreenshotProducesPrivateImmutablePixelsAndNoInlineEventBytes) {
  auto t = tools();
  auto result = t.execute(call("computer.screenshot"), cancel);
  ASSERT_FALSE(result.error) << result.content;
  ASSERT_EQ(result.images.size(), 1u);
  const auto &image = result.images[0];
  EXPECT_EQ(files.read(image.path), png());
  struct stat st{};
  ASSERT_EQ(stat(image.path.c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0600);
  EXPECT_EQ(image_data_url(image, config), "data:image/png;base64," + base64_encode(png()));
  EXPECT_EQ(result.content.find("base64"), std::string::npos);
  const auto second = t.execute(call("computer.screenshot"), cancel);
  EXPECT_EQ(second.images[0].path, image.path);
  files.atomic_write(image.path, "corrupt", true);
  EXPECT_THROW(image_data_url(image, config), Error);
}
TEST_F(Contracts, FramebufferConversionHandlesPpcByteOrderStrideAnd16BitPixels) {
  const unsigned char big[] = {0, 255, 0, 0, 0, 0, 255, 0, 9, 9, 9, 9};
  const auto rgb = framebuffer_rgb(big, 2, 1, 12, 32, true);
  EXPECT_EQ(rgb, std::string("\xff\0\0\0\xff\0", 6));
  const unsigned char small[] = {0x7c, 0x00, 0x00, 0x1f};
  EXPECT_EQ(framebuffer_rgb(small, 2, 1, 4, 16, true), std::string("\xff\0\0\0\0\xff", 6));
  const unsigned char little[] = {0, 0, 255, 0};
  EXPECT_EQ(framebuffer_rgb(little, 1, 1, 4, 32, false), std::string("\xff\0\0", 3));
  EXPECT_THROW(framebuffer_rgb(big, 999999, 999999, 4, 32, true), Error);
  EXPECT_THROW(framebuffer_rgb(big, 2, 1, 1, 32, true), Error);
}
TEST_F(Contracts, ImageLimitsCapabilitiesAndSessionForkKeepArtifactReferences) {
  EXPECT_THROW(persist_image(config, files,
                             {std::string(image_file_limit + 1, 'x'), "image/png", 1, 1}, "high"),
               Error);
  EXPECT_THROW(persist_image(config, files, {png(), "image/png", 32768, 32768}, "original"), Error);
  config.providers["openai"].image_input = false;
  auto t = tools();
  EXPECT_TRUE(t.execute(call("view_image", {{"path", "anything"}}), cancel).error);
  EXPECT_TRUE(t.execute(call("computer.screenshot"), cancel).error);
  const auto image = persist_image(config, files, {png(), "image/png", 1, 1}, "high");
  StateStore state(config.home);
  auto session = state.create();
  codet::Message m;
  m.role = "user";
  m.text = "image";
  m.images = {image};
  session.messages = {m};
  state.save(session);
  const auto fork = state.fork(state.load(session.id));
  ASSERT_EQ(fork.messages[0].images.size(), 1u);
  EXPECT_EQ(fork.messages[0].images[0].sha256, image.sha256);
}
TEST_F(Contracts, AllProvidersSerializePixelsAndReportMissingArtifactsWithoutReexecution) {
  const auto image = persist_image(config, files, {png(), "image/png", 1, 1}, "high");
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"shot", "computer.screenshot", Json::object()}};
  codet::Message output;
  output.role = "tool";
  output.tool_call_id = "shot";
  output.text = "screenshot";
  output.images = {image};
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  for (const auto &name : {"codex", "openai", "anthropic", "google", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fake";
    config.providers[name].image_input = true;
    const auto request = provider.request(config, "system", {assistant, output}, tools().schemas(),
                                          {{"access_token", "fake"}});
    EXPECT_THAT(request.body, HasSubstr(base64_encode(png()))) << name;
    EXPECT_EQ(request.body.find("codet_prepared_images"), std::string::npos) << name;
  }
  fs::remove(image.path);
  config.provider = "openai";
  const auto request = provider.request(config, "system", {assistant, output}, Json::array());
  EXPECT_THAT(request.body, HasSubstr("Image artifact"));
}
TEST_F(Contracts, ResponsesNamespacesAndOtherProviderNamesRoundTrip) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  auto t = tools();
  config.provider = "codex";
  auto request =
      Json::parse(provider.request(config, "", {}, t.schemas(), {{"access_token", "fake"}}).body);
  bool clock_namespace = false;
  for (const auto &tool : request["tools"])
    if (tool.value("type", "") == "namespace" && tool.at("name") == "clock")
      clock_namespace = true;
  EXPECT_TRUE(clock_namespace);
  config.provider = "openai";
  request = Json::parse(provider.request(config, "", {}, t.schemas()).body);
  bool flat = false;
  for (const auto &tool : request["tools"])
    if (tool["function"]["name"] == "clock__curr_time")
      flat = true;
  EXPECT_TRUE(flat);
  EXPECT_EQ(tool_logical_name("computer__get_state"), "computer.get_state");
}

TEST_F(Contracts, ScreenshotCoordinatesRespectCroppingScalingAndNegativeDisplayOrigins) {
  const auto point = desktop_point(640, 360, 1280, 720, -1920, 100, 1920, 1080);
  EXPECT_DOUBLE_EQ(point.first, -960);
  EXPECT_DOUBLE_EQ(point.second, 640);
  EXPECT_THROW(desktop_point(1280, 0, 1280, 720, 0, 0, 1920, 1080), Error);
  EXPECT_THROW(desktop_point(-1, 0, 1280, 720, 0, 0, 1920, 1080), Error);
}
struct ScriptProvider : Provider {
  std::function<ProviderResult(const std::vector<codet::Message> &, const Json &)> next;
  ProviderResult complete(const Config &, const std::string &,
                          const std::vector<codet::Message> &messages, const Json &schemas,
                          const Cancellation &, const ChunkSink &) override {
    return next(messages, schemas);
  }
};
TEST_F(Contracts, AsyncRepliesJoinTheTranscriptOnlyAfterTheirOwningToolBatch) {
  auto channel = std::make_shared<Interaction>();
  auto t = tools(channel);
  StateStore state(config.home);
  auto session = state.create();
  ScriptProvider provider;
  int requests = 0;
  provider.next = [&](const auto &messages, const auto &) {
    ProviderResult r;
    r.message.role = "assistant";
    if (++requests == 1) {
      channel->submit("use the revised constraint");
      r.message.calls = {call("request_user_input_async",
                              Json::parse(R"({"questions":[{"title":"Choose a constraint"}]})"))};
    } else {
      EXPECT_EQ(messages.size(), 4u);
      EXPECT_EQ(messages[1].role, "assistant");
      EXPECT_EQ(messages[2].role, "tool");
      EXPECT_EQ(messages[3].role, "user");
      EXPECT_EQ(messages[3].text, "use the revised constraint");
      r.message.text = "done";
    }
    return r;
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "begin", cancel).text, "done");
  EXPECT_EQ(requests, 2);
}
TEST_F(Contracts, CompactionRetainsReloadableImageReferences) {
  const auto artifact = persist_image(config, files, {png(), "image/png", 1, 1}, "high");
  StateStore state(config.home);
  auto session = state.create();
  codet::Message first;
  first.role = "user";
  first.text = std::string(2000, 'x');
  first.images = {artifact};
  for (int i = 0; i < 200; ++i) {
    auto older = artifact;
    older.path += ".historical-" + std::to_string(i);
    first.images.push_back(older);
  }
  codet::Message second;
  second.role = "assistant";
  second.text = "earlier response";
  second.images = {artifact};
  session.messages = {first, second};
  state.save(session);
  auto t = tools();
  config.context_tokens = codet::test::context_tokens(t, config, 1200);
  ScriptProvider provider;
  int summaries = 0;
  provider.next = [&](const auto &, const Json &schemas) {
    ProviderResult r;
    r.message.role = "assistant";
    if (schemas.empty()) {
      ++summaries;
      r.message.text = "continuation";
    } else
      r.message.text = "done";
    return r;
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
  EXPECT_GE(summaries, 1);
  EXPECT_TRUE(session.messages[0].images.empty());
  EXPECT_TRUE(session.messages[1].images.empty());
  const auto index = artifact_index_path(artifact_directory(config), session.id);
  const auto text = files.read(index);
  EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 201);
  EXPECT_EQ(session.messages[0].artifact_index, index.string());
  EXPECT_THAT(t.dynamic_context(), HasSubstr(index.string()));
  const auto restored = state.load_id(session.id);
  EXPECT_EQ(restored.messages[0].artifact_index, index.string());
  const auto fork = state.fork(restored);
  const auto child_index = artifact_index_path(artifact_directory(config), fork.id);
  EXPECT_EQ(fork.messages[0].artifact_index, child_index.string());
  EXPECT_EQ(files.read(child_index), text);
  config.permission_mode = "plan";
  EXPECT_FALSE(
      t.execute(call("read_file", {{"path", index.string()}, {"offset", 2}, {"limit", 2}}), cancel)
          .error);
  EXPECT_TRUE(t.execute(call("read_file", {{"path", child_index.string()}}), cancel).error);
  EXPECT_FALSE(t.execute(call("view_image", {{"path", artifact.path}}), cancel).error);
  config.set_deny({"read_file(" + index.string() + ")", "view_image(" + artifact.path + ")"});
  EXPECT_TRUE(t.execute(call("read_file", {{"path", index.string()}}), cancel).error);
  EXPECT_TRUE(t.execute(call("view_image", {{"path", artifact.path}}), cancel).error);
  config.set_deny({});
  files.atomic_write(artifact.path, "corrupt");
  EXPECT_TRUE(t.execute(call("view_image", {{"path", artifact.path}}), cancel).error);
}
struct FailingFiles : FileSystem {
  NativeFileSystem native;
  bool armed = true;
  std::string read(const fs::path &p, size_t cap) const override {
    return native.read(p, cap);
  }
  void atomic_write(const fs::path &p, const std::string &text, bool private_file) override {
    if (armed && p.filename() == "b") {
      armed = false;
      throw Error("injected write failure");
    }
    native.atomic_write(p, text, private_file);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    return native.canonical(p);
  }
};
TEST_F(Contracts, PatchRollsBackAnEarlierWriteWhenALaterFileFails) {
  config.permission_mode = "accept-edits";
  files.atomic_write(config.cwd / "a", "old a\n");
  files.atomic_write(config.cwd / "b", "old b\n");
  FailingFiles failing;
  Tools t(config, failing, processes, http, clock);
  const auto patch = "*** Begin Patch\n*** Update File: a\n@@\n-old a\n+new a\n*** Update File: "
                     "b\n@@\n-old b\n+new b\n*** End Patch\n";
  EXPECT_TRUE(t.execute(call("apply_patch", {{"patch", patch}}), cancel).error);
  EXPECT_EQ(files.read(config.cwd / "a"), "old a\n");
  EXPECT_EQ(files.read(config.cwd / "b"), "old b\n");
}
TEST_F(Contracts, NativeImageIoCanEncodeAndReloadPixelsWithoutAConsole) {
  const auto codec = native_images();
  if (!codec->available())
    GTEST_SKIP() << "ImageIO is only available on Apple hosts";
  const auto encoded = encode_rgb(std::string("\xff\0\0\0\0\xff", 6), 1, 2, "high");
  const auto path = config.cwd / "pixels.png";
  files.atomic_write(path, encoded.bytes);
  const auto loaded = codec->load(path, "original");
  EXPECT_EQ(loaded.width, 1u);
  EXPECT_EQ(loaded.height, 2u);
  EXPECT_EQ(loaded.mime, "image/png");
}
TEST_F(Contracts, ImageRequestBudgetKeepsOnlyFourRecentImages) {
  const auto artifact = persist_image(config, files, {png(), "image/png", 1, 1}, "high");
  std::vector<codet::Message> messages;
  for (int i = 0; i < 5; ++i) {
    codet::Message m;
    m.role = "user";
    m.text = "image";
    m.images = {artifact};
    messages.push_back(m);
  }
  CredentialStore creds(config.home, files, clock);
  ApiProvider provider(http, creds, clock);
  const auto request = provider.request(config, "", messages, Json::array());
  const auto body = Json::parse(request.body);
  int count = 0;
  for (const auto &m : body["messages"])
    if (m["content"].is_array())
      for (const auto &part : m["content"])
        if (part.value("type", "") == "image_url")
          ++count;
  EXPECT_EQ(count, 4);
  EXPECT_THAT(request.body, HasSubstr("omitted from request budget"));
}

TEST_F(Contracts, NativeTigerDesktopAcceptance) {
  if (env("CODET_TEST_DESKTOP") != "1")
    GTEST_SKIP() << "Set CODET_TEST_DESKTOP=1 explicitly on Tiger";
  auto computer = native_computer();
  ASSERT_TRUE(computer->capabilities().value("available", false)) << computer->capabilities();
  config.permission_mode = "bypass";
  Tools t(config, files, processes, http, clock, {}, {}, {}, {}, computer, native_images());
  const auto file = config.cwd / "codet-desktop-acceptance.txt";
  files.atomic_write(file, "Codet desktop fixture\n");
  auto script = [&](const std::string &text) {
    return t.execute(call("computer.applescript", {{"script", text}}), cancel);
  };
  const auto opened = script("tell application \"TextEdit\"\nactivate\nopen POSIX file " +
                             Json(file.string()).dump() + "\nend tell");
  ASSERT_FALSE(opened.error) << opened.content;
  ASSERT_EQ(opened.metadata.value("exit_code", -1), 0) << opened.content;
  struct Cleanup {
    std::function<void()> close;
    ~Cleanup() {
      close();
    }
  } cleanup{[&] {
    Cancellation close_cancel;
    try {
      computer->invoke("applescript",
                       {{"script", "tell application \"TextEdit\" to close document "
                                   "\"codet-desktop-acceptance.txt\" saving no"}},
                       close_cancel);
    } catch (...) {
    }
  }};
  clock.sleep_ms(500, cancel);
  auto observed = t.execute(call("computer.get_state"), cancel);
  ASSERT_FALSE(observed.error) << observed.content;
  ASSERT_TRUE(computer->capabilities().value("accessibility", false))
      << "Enable access for assistive devices in Universal Access";
  ASSERT_FALSE(
      t.execute(call("computer.key", {{"key", "a"}, {"modifiers", {"command"}}}), cancel).error);
  const auto typed =
      t.execute(call("computer.type_text", {{"text", "Codet Tiger café Ω"}}), cancel);
  ASSERT_FALSE(typed.error) << typed.content;
  clock.sleep_ms(200, cancel);
  const auto read = script(
      "tell application \"TextEdit\" to get text of document \"codet-desktop-acceptance.txt\"");
  ASSERT_FALSE(read.error) << read.content;
  EXPECT_THAT(read.content, HasSubstr("Codet Tiger café Ω"));
  const auto shot = t.execute(call("computer.screenshot"), cancel);
  ASSERT_FALSE(shot.error) << shot.content;
  ASSERT_EQ(shot.images.size(), 1u);
  const auto report = env("CODET_DESKTOP_REPORT");
  if (!report.empty()) {
    fs::create_directories(report);
    files.atomic_write(fs::path(report) / "desktop-state.json", observed.content, true);
    files.atomic_write(fs::path(report) / "desktop.png", files.read(shot.images[0].path), true);
  }
  const auto clipboard = t.execute(call("computer.clipboard_read"), cancel);
  if (!clipboard.error) {
    const auto previous = Json::parse(clipboard.content).at("text").get<std::string>();
    const auto clipboard_fixture = std::string(9000, 'x') + " Codet clipboard ✓";
    ASSERT_FALSE(
        t.execute(call("computer.clipboard_write", {{"text", clipboard_fixture}}), cancel).error);
    const auto copied = t.execute(call("computer.clipboard_read"), cancel);
    EXPECT_EQ(Json::parse(copied.content).at("text"), clipboard_fixture);
    EXPECT_FALSE(t.execute(call("computer.clipboard_write", {{"text", previous}}), cancel).error);
  }
  EXPECT_FALSE(script("tell application \"Finder\" to activate").error);
  EXPECT_FALSE(t.execute(call("computer.open_url", {{"url", "http://127.0.0.1/"}}), cancel).error);
}
TEST_F(Contracts, SeparateToolLifetimesUseIndependentSessionIdentities) {
  config.permission_mode = "bypass";
  auto first = tools();
  auto second = tools();
  const auto command = Json({{"cmd", "read answer"}, {"login", false}, {"yield_time_ms", 250}});
  const auto a = first.execute(call("exec_command", command), cancel);
  const auto b = second.execute(call("exec_command", command), cancel);
  ASSERT_FALSE(a.error) << a.content;
  ASSERT_FALSE(b.error) << b.content;
  EXPECT_NE(Json::parse(a.content).at("session_id"), Json::parse(b.content).at("session_id"));
  first.cancel_exec();
  second.cancel_exec();
}
} // namespace
