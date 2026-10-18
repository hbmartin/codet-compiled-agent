#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/process_control.hpp"
#include "codet/support.hpp"
#include "prompt_support.hpp"
#include "temp.hpp"
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <numeric>
#include <thread>
using namespace codet;
using namespace testing;
namespace {
struct Offline : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected network");
  }
};
struct OwnedProcess : Processes {
  std::exception_ptr failure;
  size_t starts = 0, reads = 0;
  bool running = true;
  std::string unread, error;
  ProcessResult run(const ProcessSpec &, const Cancellation &) override {
    throw Error("Unexpected synchronous execution");
  }
  std::string start(const ProcessSpec &) override {
    ++starts;
    return "owned";
  }
  ProcessStatus status(const std::string &id) override {
    if (failure)
      std::rethrow_exception(failure);
    return {id, running, 9, error};
  }
  ProcessResult poll(const std::string &) override {
    ++reads;
    auto text = std::move(unread);
    unread.clear();
    return {9, text, false, running};
  }
  void stop(const std::string &) override {}
};
struct Reply : Provider {
  std::function<ProviderResult(const std::vector<codet::Message> &, const Json &)> reply;
  ProviderResult complete(const Config &, const std::string &,
                          const std::vector<codet::Message> &messages, const Json &schemas,
                          const Cancellation &, const ChunkSink &, const ItemSink & = {}) override {
    return reply(messages, schemas);
  }
};
ProviderResult answer(const std::string &text) {
  ProviderResult result;
  result.message.role = "assistant";
  result.message.text = text;
  return result;
}
struct Checkpoint : Test {
  TempDirectory temp;
  Config config;
  NativeFileSystem files;
  OwnedProcess processes;
  Offline http;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "work";
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.approval = "never";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
  }
  int start(Tools &tools) {
    const auto result = tools.execute(
        {"start",
         "exec_command",
         {{"cmd", "fixture"}, {"login", false}, {"shell", "/bin/sh"}, {"yield_time_ms", 250}}},
        cancel);
    EXPECT_FALSE(result.error) << result.content;
    return result.metadata.at("session_id").get<int>();
  }
};
TEST_F(Checkpoint, PendingOutputRemainsOwnedUntilCollectedAndNeverRevivesAfterResume) {
  Tools tools(config, files, processes, http, clock);
  const auto id = start(tools);
  EXPECT_EQ(tools.execution_state({})[0]["state"], "running");
  processes.running = false;
  processes.unread = "terminal output";
  const auto reads = processes.reads;
  const auto facts = tools.execution_state({});
  ASSERT_EQ(facts.size(), 1u);
  EXPECT_EQ(facts[0]["state"], "awaiting_output");
  EXPECT_EQ(facts[0]["exit_code"], 9);
  EXPECT_EQ(processes.reads, reads);
  codet::Message saved;
  saved.checkpoint_state = facts;
  Tools resumed(config, files, processes, http, clock);
  EXPECT_TRUE(resumed.execution_state({saved}).empty());
  const auto result = tools.execute({"poll", "write_stdin", {{"session_id", id}}}, cancel);
  EXPECT_FALSE(result.error);
  EXPECT_THAT(result.content, HasSubstr("terminal output"));
  EXPECT_EQ(result.metadata["exit_code"], 9);
  EXPECT_TRUE(tools.execution_state({saved}).empty());
}
TEST_F(Checkpoint, ObservationFailuresStayLocalAndFatalFailuresPropagate) {
  Tools tools(config, files, processes, http, clock);
  const auto id = start(tools);
  for (const auto &failure :
       {std::make_exception_ptr(Error("Unknown or expired job")),
        std::make_exception_ptr(std::runtime_error(std::string(2000, 'x')))}) {
    processes.failure = failure;
    const auto facts = tools.execution_state({});
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0]["session_id"], id);
    EXPECT_EQ(facts[0]["state"], "unknown");
    EXPECT_LE(facts[0]["error"].get<std::string>().size(), 512u);
  }
  processes.failure = std::make_exception_ptr(Error("Interrupted", 130));
  EXPECT_THROW(tools.execution_state({}), Error);
  processes.failure = std::make_exception_ptr(std::bad_alloc());
  EXPECT_THROW(tools.execution_state({}), std::bad_alloc);
  processes.failure = std::make_exception_ptr(ProcessResourceError("resource exhaustion"));
  EXPECT_THROW(tools.execution_state({}), ProcessResourceError);
  processes.failure = {};
  processes.error = "ordinary observer diagnostic";
  EXPECT_EQ(tools.execution_state({})[0]["state"], "unknown");
  processes.error.clear();
  processes.running = false;
  EXPECT_EQ(tools.execution_state({})[0]["state"], "awaiting_output");
}
TEST_F(Checkpoint, ExpiredNativeRecordsDoNotAbortCheckpointCollection) {
  NativeProcesses native;
  Tools tools(config, files, native, http, clock);
  const auto result = tools.execute(
      {"start",
       "exec_command",
       {{"cmd", "sleep 1"}, {"login", false}, {"shell", "/bin/sh"}, {"yield_time_ms", 250}}},
      cancel);
  ASSERT_TRUE(result.metadata.contains("session_id"));
  const auto id = result.metadata["session_id"];
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  auto observed = tools.execution_state({});
  while (!observed.empty() && observed[0]["state"] == "running" &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    observed = tools.execution_state({});
  }
  ASSERT_EQ(observed.size(), 1u);
  ASSERT_EQ(observed[0]["state"], "awaiting_output");
  ProcessSpec spec;
  spec.argv = {"/bin/sh", "-c", "exit 0"};
  spec.cwd = config.cwd;
  for (int i = 0; i < 70; ++i) {
    const auto process = native.start(spec, cancel);
    while (native.status(process).running)
      clock.sleep_ms(1, cancel);
  }
  (void)native.list();
  const auto facts = tools.execution_state({});
  ASSERT_EQ(facts.size(), 1u);
  EXPECT_EQ(facts[0]["session_id"], id);
  EXPECT_EQ(facts[0]["state"], "unknown");
  EXPECT_THAT(facts[0]["error"].get<std::string>(), HasSubstr("expired"));
}
TEST_F(Checkpoint, RetiredGroupStillAllowsItsUncollectedOutputToBeReadExactlyOnce) {
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 14000);
  StateStore state(config.home);
  auto session = state.create();
  tools.use_session(session.id);
  const auto id = start(tools);
  processes.running = false;
  processes.unread = "collected after checkpoint";
  codet::Message calls, result;
  calls.role = "assistant";
  calls.calls = {{"start", "exec_command", {{"cmd", std::string(130000, 'x')}}}};
  result.role = "tool";
  result.tool_call_id = "start";
  result.text =
      Json({{"content", "yielded"}, {"is_error", false}, {"metadata", {{"session_id", id}}}})
          .dump();
  session.messages = {calls, result};
  state.save(session);
  Reply provider;
  size_t turns = 0;
  provider.reply = [&](const auto &messages, const Json &schemas) {
    if (schemas.empty())
      return answer("Collect pending output; do not repeat completed effects.");
    if (!turns++) {
      EXPECT_EQ(messages[0].checkpoint_state[0]["state"], "awaiting_output");
      auto response = answer("");
      response.message.calls = {{"collect", "write_stdin", {{"session_id", id}}}};
      return response;
    }
    EXPECT_THAT(messages.back().text, HasSubstr("collected after checkpoint"));
    return answer("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
  EXPECT_EQ(processes.starts, 1u);
  EXPECT_EQ(processes.reads, 2u);
  EXPECT_TRUE(tools.execution_state(session.messages).empty());
}
TEST_F(Checkpoint, ArchiveIdentityIgnoresOnlyTheTopLevelPresentationFlag) {
  codet::Message original;
  original.role = "tool";
  original.tool_call_id = "json-file";
  original.message_id = unique_id();
  original.checkpoint_state = {{"count", 1}};
  original.text = "{\n  \"previewed\": true,\n  \"unicode\": \"é\"\n}\n";
  Tools tools(config, files, processes, http, clock);
  tools.use_session("archive");
  auto first = original;
  const auto path = tools.retain_result(first);
  const auto bytes = files.read(path);
  auto retry = original;
  retry.previewed = true;
  EXPECT_EQ(tools.retain_result(retry), path);
  EXPECT_EQ(files.read(path), bytes);
  for (bool flag : {false, true}) {
    Json legacy = original;
    legacy["previewed"] = flag;
    files.atomic_write(path, legacy.dump(), true);
    const auto before = files.read(path);
    Tools reopened(config, files, processes, http, clock);
    reopened.use_session("archive");
    retry = original;
    retry.previewed = !flag;
    EXPECT_EQ(reopened.retain_result(retry), path);
    EXPECT_EQ(files.read(path), before);
  }
  for (const auto &change :
       {Json{{"previewed", "invalid"}}, Json{{"text", "changed"}}, Json{{"extra_field", true}},
        Json{{"checkpoint_state", {{"count", 1.0}}}}}) {
    auto corrupt = Json(original);
    corrupt.update(change);
    files.atomic_write(path, corrupt.dump(), true);
    Tools reopened(config, files, processes, http, clock);
    reopened.use_session("archive");
    retry = original;
    EXPECT_THROW(reopened.retain_result(retry), Error);
  }
  files.atomic_write(path, "{", true);
  Tools reopened(config, files, processes, http, clock);
  reopened.use_session("archive");
  retry = original;
  EXPECT_THROW(reopened.retain_result(retry), Error);
}
TEST_F(Checkpoint, FailedSummaryThenPersistedPreviewCanRetryTheSameArchive) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message call, result, human;
  call.role = "assistant";
  call.calls = {{"old", "read_file", {{"path", "file"}}}};
  result.role = "tool";
  result.tool_call_id = "old";
  result.text =
      Json({{"content", std::string(1000, 'x')}, {"is_error", false}, {"metadata", Json::object()}})
          .dump();
  human.role = "user";
  human.origin = "human";
  human.text = "latest instruction";
  auto newer = call;
  newer.calls[0].id = "new";
  auto recent = result;
  recent.tool_call_id = "new";
  session.messages = {call, result, human, newer, recent};
  state.save(session);
  Tools tools(config, files, processes, http, clock);
  Reply provider;
  bool fail = true;
  provider.reply = [&](const auto &, const Json &) {
    if (fail)
      throw Error("injected provider failure");
    return answer("Completed earlier inspection; continue from latest instruction.");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.compact(session, cancel), Error);
  session = state.load_id(session.id);
  EXPECT_TRUE(session.messages[1].result_file.empty());
  EXPECT_NO_THROW(project_history(session.messages, 100000, tools));
  EXPECT_TRUE(session.messages[1].previewed);
  EXPECT_TRUE(session.messages[1].result_file.empty());
  state.revise_messages(session, {1});
  fail = false;
  EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
  EXPECT_THAT(state.load_id(session.id).messages,
              Contains(Field(&codet::Message::text, human.text)));
}
TEST_F(Checkpoint, ResponsesRetainOnePassCustomCallTrackingAcrossProviderChanges) {
  std::vector<codet::Message> messages;
  for (int i = 0; i < 100; ++i) {
    const auto id = std::to_string(i);
    codet::Message call, result;
    call.role = "assistant";
    call.calls = {{id, i % 2 ? "functions.read_file" : "apply_patch", {{"patch", "fixture"}}}};
    call.replay_provider = "codex";
    if (!(i % 2))
      call.replay = Json::array({{{"type", "custom_tool_call"},
                                  {"call_id", id},
                                  {"name", "apply_patch"},
                                  {"input", "fixture"}}});
    result.role = "tool";
    result.tool_call_id = id;
    result.text = Json({{"content", "outcome"}, {"is_error", false}}).dump();
    messages.push_back(call);
    messages.push_back(result);
  }
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  for (const auto *name : {"codex", "openai"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    const auto size = provider_history_size(config, messages);
    EXPECT_LE(size.encoded_parts, messages.size() * 2 + 1);
    const auto wire = Json::parse(
        provider.request(config, "", messages, Json::array(), {{"access_token", "fixture"}})
            .body)["input"];
    EXPECT_EQ(size.total, wire.dump().size());
    size_t outcomes = 0;
    for (const auto &item : wire) {
      const auto type = item.value("type", "");
      if (type == "custom_tool_call_output" || type == "function_call_output") {
        const auto id = std::stoi(item.at("call_id").get<std::string>());
        EXPECT_EQ(type == "custom_tool_call_output", config.provider == "codex" && !(id % 2));
        ++outcomes;
      }
    }
    EXPECT_EQ(outcomes, 100u);
  }
}
TEST_F(Checkpoint, FailedLexicalAnchorsDoNotConsumeAdmissionOrShadowAUsableSkill) {
  struct ChangingFiles : FileSystem {
    NativeFileSystem native;
    std::function<void(const fs::path &)> after_read;
    std::string read(const fs::path &p, size_t limit) const override {
      const auto text = native.read(p, limit);
      if (after_read)
        after_read(p);
      return text;
    }
    void atomic_write(const fs::path &p, const std::string &text, bool secret) override {
      native.atomic_write(p, text, secret);
    }
    void remove(const fs::path &p) override {
      native.remove(p);
    }
    fs::path canonical(const fs::path &p) const override {
      return native.canonical(p);
    }
  } changing;
  struct RestoreCwd {
    fs::path previous = fs::current_path();
    ~RestoreCwd() {
      std::error_code error;
      fs::current_path(previous, error);
    }
  } restore;
  const auto bad = config.cwd / ".agents/skills/000/SKILL.md";
  const auto good = config.home / "skills/same/SKILL.md";
  fs::create_directories(bad.parent_path());
  fs::create_directories(good.parent_path());
  fs::create_directory(config.cwd / ".git");
  files.atomic_write(bad, "---\nname: same\ndescription: unusable anchor\n---\n");
  for (int i = 1; i < 100; ++i) {
    const auto other = bad.parent_path().parent_path() / ("skill" + std::to_string(i)) / "SKILL.md";
    fs::create_directories(other.parent_path());
    files.atomic_write(other, "---\nname: same\ndescription: unusable anchor\n---\n");
  }
  files.atomic_write(good, "---\nname: same\ndescription: usable fallback\n---\n");
  const auto canonical_bad = files.canonical(bad);
  const auto gone = temp.root / "gone";
  fs::create_directory(gone);
  fs::current_path(temp.root);
  config.cwd = "work";
  std::vector<std::string> warnings;
  Tools tools(config, changing, processes, http, clock, {}, {},
              [&](const auto &warning) { warnings.push_back(warning); });
  changing.after_read = [&](const auto &path) {
    if (path == canonical_bad) {
      fs::current_path(gone);
      fs::remove(gone);
    }
  };
  const auto text = tools.instructions();
  fs::current_path(restore.previous);
  EXPECT_THAT(text, Not(HasSubstr("unusable anchor")));
  EXPECT_THAT(text, HasSubstr("usable fallback"));
  EXPECT_THAT(warnings, Contains(HasSubstr("Discovery anchor unavailable")));
}

TEST_F(Checkpoint, SkillAdmissionStillStopsAtOneHundredValidAnchors) {
  fs::create_directory(config.cwd / ".git");
  for (int i = 1000; i <= 1100; ++i) {
    const auto name = "s" + std::to_string(i);
    const auto path = config.cwd / ".agents/skills" / name / "SKILL.md";
    fs::create_directories(path.parent_path());
    files.atomic_write(path, "---\nname: " + name + "\ndescription: ok\n---\n");
  }
  const auto fallback = config.home / "skills/fallback/SKILL.md";
  fs::create_directories(fallback.parent_path());
  files.atomic_write(fallback, "---\nname: fallback\ndescription: later root\n---\n");
  Tools tools(config, files, processes, http, clock);
  const auto text = tools.instructions();
  size_t advertised = 0;
  for (int i = 1000; i <= 1100; ++i)
    advertised += text.find("- s" + std::to_string(i) + ": ok (file:") != std::string::npos;
  EXPECT_EQ(advertised, 100u);
  EXPECT_THAT(text, Not(HasSubstr("- fallback:")));
}

// OpenAI's two wire APIs are separate cases even though they share a provider name.

TEST_F(Checkpoint, RepeatedCompactionKeepsTheOriginalImageArchiveReference) {
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 14000);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message human, call, result;
  human.role = "user";
  human.origin = "human";
  human.text = "keep this image";
  human.message_id = unique_id();
  human.images = {persist_image(config, files, {"pixels", "image/png", 1, 1}, "high")};
  call.role = "assistant";
  call.calls = {{"huge", "exec_command", {{"cmd", std::string(130000, 'x')}}}};
  result.role = "tool";
  result.tool_call_id = "huge";
  result.text = "completed";
  session.messages = {human, call, result};
  state.save(session);
  Reply provider;
  provider.reply = [&](const auto &, const auto &) { return answer("completed work"); };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
  auto found = std::find_if(session.messages.begin(), session.messages.end(),
                            [&](const auto &m) { return m.message_id == human.message_id; });
  ASSERT_NE(found, session.messages.end());
  EXPECT_TRUE(found->images.empty());
  ASSERT_FALSE(found->result_file.empty());
  const auto archive_path = artifact_directory(config) / found->result_file;
  const auto archived = files.read(archive_path, 10000);
  EXPECT_EQ(Json::parse(archived)["images"].size(), 1u);
  human.message_id = unique_id();
  human.text = "next steering";
  human.images.clear();
  session.messages.push_back(human);
  state.save(session);
  EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
  EXPECT_EQ(files.read(archive_path, 10000), archived);
}
TEST_F(Checkpoint, UnseenImagesAndHooksKeepChronologicalOrderExactlyOnce) {
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 14000);
  StateStore state(config.home);
  auto session = state.create();
  const auto image = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  codet::Message call, result, old, latest, hook;
  call.role = "assistant";
  call.calls = {{"huge", "exec_command", {{"cmd", std::string(130000, 'x')}}}};
  result.role = "tool";
  result.tool_call_id = "huge";
  result.text = "completed";
  result.images = {image};
  old.role = "user";
  old.origin = "artifact";
  old.text = "older unseen image";
  old.images = {image};
  latest.role = "user";
  latest.origin = "human";
  latest.text = "latest steering";
  hook.role = "user";
  hook.origin = "hook";
  hook.text = "hook with image";
  hook.images = {image};
  session.messages = {call, result, old, latest, hook};
  state.save(session);
  Reply provider;
  provider.reply = [&](const auto &, const auto &) { return answer("completed work"); };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
  ASSERT_EQ(session.messages.size(), 5u);
  EXPECT_EQ(session.messages[1].origin, "artifact");
  EXPECT_EQ(session.messages[2].text, old.text);
  EXPECT_EQ(session.messages[3].text, latest.text);
  EXPECT_EQ(session.messages[4].text, hook.text);
}
TEST_F(Checkpoint, LiveFactsRefreshAfterCheckpointWithoutPersistingRunningProse) {
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 14000);
  StateStore state(config.home);
  auto session = state.create();
  tools.use_session(session.id);
  const auto id = start(tools);
  codet::Message call, result;
  call.role = "assistant";
  call.calls = {{"huge", "exec_command", {{"cmd", std::string(130000, 'x')}}}};
  result.role = "tool";
  result.tool_call_id = "huge";
  result.text = "completed";
  session.messages = {call, result};
  state.save(session);
  struct Script : Provider {
    std::function<ProviderResult(const std::string &, const std::vector<codet::Message> &,
                                 const Json &)>
        reply;
    ProviderResult complete(const Config &, const std::string &system,
                            const std::vector<codet::Message> &messages, const Json &schemas,
                            const Cancellation &, const ChunkSink &, const ItemSink &) override {
      return reply(system, messages, schemas);
    }
  } provider;
  int requests = 0;
  provider.reply = [&](const auto &system, const auto &messages, const auto &schemas) {
    if (schemas.empty()) {
      EXPECT_THAT(system, Not(HasSubstr("\"state\":\"running\"")));
      EXPECT_EQ(messages.back().origin, "runtime");
      EXPECT_THAT(messages.back().text, HasSubstr("\"state\":\"running\""));
      return answer("Completed work; collect output.");
    }
    EXPECT_THAT(messages[0].text, Not(HasSubstr("\"state\":\"running\"")));
    if (requests++ == 0) {
      EXPECT_THAT(system, Not(HasSubstr("\"state\":\"running\"")));
      EXPECT_EQ(messages.back().origin, "runtime");
      EXPECT_THAT(messages.back().text, HasSubstr("\"state\":\"running\""));
      processes.running = false;
      auto r = answer("");
      r.message.calls = {{"collect", "write_stdin", {{"session_id", id}}}};
      return r;
    }
    EXPECT_THAT(system, Not(HasSubstr("\"state\":\"running\"")));
    EXPECT_TRUE(tools.execution_state(messages).empty());
    return answer("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
}
class WireBudget : public Checkpoint, public WithParamInterface<std::string> {
  void SetUp() override {
    Checkpoint::SetUp();
    config.provider = GetParam() == "chat" ? "openai" : GetParam();
    if (GetParam() == "chat")
      config.providers["openai"].wire_api = "chat_completions";
  }
};
TEST_P(WireBudget, ExactCombinedHistoryFitsAndOneByteLessRejectsWithoutMutation) {
  Tools tools(config, files, processes, http, clock);
  std::vector<codet::Message> messages;
  for (int i = 0; i < 50; ++i) {
    codet::Message m;
    m.role = i % 5 == 4 ? "assistant" : "user";
    m.text = "coalesced text é\n\"quoted\"";
    messages.push_back(m);
  }
  ensure_message_ids(messages);
  const auto size = tools.history_size(messages);
  EXPECT_EQ(size.aggregate_items, 0u);
  EXPECT_LE(size.encoded_parts, messages.size() * 2 + 1);
  EXPECT_EQ(size.total,
            size.fixed + std::accumulate(size.messages.begin(), size.messages.end(), size_t{}));
  const auto before = Json(messages);
  EXPECT_THROW(project_history(messages, size.total - 1, tools), ContextCapacity);
  EXPECT_EQ(Json(messages), before);
  EXPECT_EQ(Json(project_history(messages, size.total, tools)), before);
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  config.providers[config.provider].api_key = "fixture";
  const auto request =
      provider.request(config, "", messages, Json::array(), {{"access_token", "fixture"}});
  const auto body = Json::parse(request.body);
  const auto &wire = body.at(uses_responses(config)        ? "input"
                             : config.provider == "google" ? "contents"
                                                           : "messages");
  EXPECT_EQ(size.total, wire.dump().size());
}
TEST_P(WireBudget, FreshPagesAndStablePreviewsKeepTheirBytesWhileNewResultsShareSpace) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("budget");
  codet::Message calls;
  calls.role = "assistant";
  std::vector<codet::Message> messages{calls};
  for (int i = 0; i < 4; ++i) {
    const auto id = std::to_string(i);
    messages[0].calls.push_back({id, i ? "exec_command" : "read_file", {{"cmd", "test"}}});
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = id;
    const auto content =
        i ? Json({{"output", std::string(18000, 'x') + "\nTEST RESULT"}, {"exit_code", 0}}).dump()
          : std::string(1000, 'f');
    result.text = Json({{"content", content},
                        {"is_error", false},
                        {"metadata",
                         i ? Json::object() : Json{{"next_byte_offset", 1000}, {"next_line", 1}}}})
                      .dump();
    messages.push_back(result);
  }
  const auto page = messages[1].text;
  const auto full = tools.history_bytes(messages);
  const auto projected = project_history(messages, 14000, tools);
  EXPECT_LT(tools.history_bytes(projected), full);
  EXPECT_LE(tools.history_bytes(projected), 14000u);
  EXPECT_EQ(projected[1].text, page);
  EXPECT_TRUE(projected[1].result_file.empty());
  for (size_t i = 2; i < projected.size(); ++i) {
    EXPECT_THAT(projected[i].text, HasSubstr("TEST RESULT"));
    EXPECT_FALSE(projected[i].result_file.empty());
  }
  const auto before = Json(messages);
  EXPECT_EQ(Json(project_history(messages, 15000, tools)), before);
}
TEST_P(WireBudget, SmallFreshResultsUseTheFastPathWithoutCreatingArchives) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("full");
  codet::Message call, result;
  call.role = "assistant";
  call.calls = {{"test", "exec_command", {{"cmd", "fixture"}}}};
  result.role = "tool";
  result.tool_call_id = "test";
  result.text = Json({{"content", "short but useful output"},
                      {"is_error", false},
                      {"metadata", Json::object()}})
                    .dump();
  std::vector<codet::Message> messages{call, result};
  const auto projected = project_history(messages, tools.history_bytes(messages), tools);
  EXPECT_EQ(projected[1].text, result.text);
  EXPECT_TRUE(projected[1].previewed);
  EXPECT_TRUE(projected[1].result_file.empty());
  EXPECT_FALSE(fs::exists(artifact_directory(config) / "results"));
}

TEST_P(WireBudget, QuotedFilePagesUseWireAllowanceAndKeepContiguousUnicode) {
  config.permission_mode = "plan";
  std::string source;
  for (int i = 0; i < 400; ++i)
    source += "{\"x\":\"\\\\\",\"y\":\"é\"}\n";
  files.atomic_write(config.cwd / "quoted.json", source, true);
  Tools tools(config, files, processes, http, clock);
  tools.inspection_budget(2048);
  const auto result = tools.execute(
      {"read", "read_file", {{"path", "quoted.json"}, {"byte_offset", 0}, {"byte_limit", 65536}}},
      cancel);
  ASSERT_FALSE(result.error) << result.content;
  codet::Message page;
  page.role = "tool";
  page.tool_call_id = "read";
  page.text =
      Json({{"content", result.content}, {"is_error", false}, {"metadata", result.metadata}})
          .dump();
  EXPECT_LE(tools.result_bytes(page, "read_file"), 2048u);
  EXPECT_GT(tools.result_bytes(page, "read_file"), 1800u);
  EXPECT_EQ(result.content, source.substr(0, result.content.size()));
  EXPECT_EQ(result.metadata["next_byte_offset"], result.content.size());
}
INSTANTIATE_TEST_SUITE_P(AllAdapters, WireBudget,
                         Values("codex", "openai", "chat", "anthropic", "google", "ollama"));

TEST_F(Checkpoint, MissingAndMalformedArchivesPreserveUnknownsAndReconstructJournalFacts) {
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  tools.use_session(session.id);
  codet::Message saved;
  saved.role = "system";
  saved.origin = "compaction";
  saved.text = "old checkpoint";
  saved.checkpoint_state = {{"records", 3}, {"recovery_record", "results/parent/lost.json"}};
  session.messages = {saved};
  state.save(session);
  state.tool_started(session.id, {"lost-call", "apply_patch", {{"patch", "old"}}});
  for (int i = 0; i < 2; ++i) {
    const auto facts =
        tools.execution_state(session.messages, [&] { return state.recovery_facts(session.id); });
    ASSERT_EQ(facts.size(), 2u);
    EXPECT_EQ(facts[0]["call_id"], "lost-call");
    EXPECT_EQ(facts[0]["recovery"], "uncertain");
    EXPECT_EQ(facts[1]["kind"], "recovery_unavailable");
    EXPECT_EQ(facts[1]["checkpoint_state"]["recovery_record"], "results/parent/lost.json");
    session.messages[0].checkpoint_state = facts;
    state.save(session);
  }
  const auto child = state.fork(state.load_id(session.id));
  tools.use_session(child.id);
  EXPECT_EQ(tools.execution_state(child.messages).size(), 2u);
  Reply provider;
  provider.reply = [&](const auto &messages, const auto &) {
    EXPECT_EQ(messages.back().origin, "runtime");
    EXPECT_THAT(messages.back().text, HasSubstr("recovery_unavailable"));
    return answer("new work remains possible");
  };
  Agent agent(config, provider, tools, state, {});
  auto resumed = state.load_id(child.id);
  EXPECT_EQ(agent.run(resumed, "inspect status", cancel).text, "new work remains possible");
  EXPECT_EQ(processes.starts, 0u);
  auto malformed = saved;
  const auto path = artifact_directory(config) / "results/parent/lost.json";
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "{broken", true);
  EXPECT_EQ(tools.execution_state({malformed}).back()["kind"], "recovery_unavailable");
  auto second = saved;
  second.checkpoint_state["recovery_record"] = "results/parent/another.json";
  EXPECT_EQ(tools.execution_state({malformed, second}).size(), 2u);
}
TEST_F(Checkpoint, OversizedLiveSnapshotsReplaceOneFileWithoutPermanentReferences) {
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  tools.use_session(session.id);
  Json facts = Json::array();
  for (int i = 0; i < 100; ++i)
    facts.push_back({{"session_id", i}, {"state", "running"}});
  std::string previous;
  for (int i = 0; i < 20; ++i) {
    facts[0]["session_id"] = i;
    const auto context = tools.execution_context(facts);
    EXPECT_THAT(context, HasSubstr("sha256"));
    EXPECT_NE(context, previous);
    previous = context;
    EXPECT_TRUE(state.references(session.id).empty());
  }
  const auto dir = invocation_directory(config) / "runtime";
  EXPECT_EQ(std::distance(fs::directory_iterator(dir), fs::directory_iterator()), 1);
  const auto path = dir / (session.id + ".execution.json");
  EXPECT_EQ(Json::parse(files.read(path, 100000)), facts);
  config.permission_mode = "plan";
  const auto read = tools.execute({"read-live", "read_file", {{"path", path.string()}}}, cancel);
  EXPECT_FALSE(read.error) << read.content;
}
TEST_F(Checkpoint, CompletedSiblingResultsAreTrimmedBeforeNextBatchAdmission) {
  struct OutputProcesses : OwnedProcess {
    std::string start(const ProcessSpec &) override {
      return std::to_string(++starts);
    }
    ProcessStatus status(const std::string &id) override {
      return {id, false, 0, ""};
    }
    ProcessResult poll(const std::string &) override {
      return {0, std::string(5000, 'z'), false, false};
    }
  } output;
  Tools tools(config, files, output, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 8500);
  config.max_output_tokens = 128;
  StateStore state(config.home);
  auto session = state.create();
  Reply provider;
  int requests = 0;
  provider.reply = [&](const auto &messages, const auto &schemas) {
    EXPECT_FALSE(schemas.empty());
    if (!requests++) {
      auto response = answer("");
      for (int i = 0; i < 3; ++i)
        response.message.calls.push_back(
            {"call" + std::to_string(i),
             "exec_command",
             {{"cmd", "fixture"}, {"shell", "/bin/sh"}, {"login", false}}});
      return response;
    }
    EXPECT_EQ(std::count_if(messages.begin(), messages.end(),
                            [](const auto &m) { return m.role == "tool"; }),
              3);
    return answer("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "run three commands", cancel).text, "done");
  EXPECT_EQ(output.starts, 3u);
  EXPECT_EQ(state.load_id(session.id).messages.size(), session.messages.size());
}

TEST_F(Checkpoint, FailedCheckpointRemovesOnlyItsUnreferencedRecoveryCandidate) {
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  tools.use_session(session.id);
  Json facts = Json::array();
  for (int i = 0; i < 100; ++i)
    facts.push_back({{"call_id", "call-" + std::to_string(i)}, {"recovery", "uncertain"}});
  codet::Message checkpoint, old;
  checkpoint.role = "system";
  checkpoint.origin = "compaction";
  checkpoint.text = "saved checkpoint";
  checkpoint.checkpoint_state = facts;
  old.role = "assistant";
  old.text = "older completed work";
  session.messages = {checkpoint, old};
  state.save(session);
  config.context_tokens = test::context_tokens(tools, config, 14000);
  const auto ref = "results/" + session.id + "/" + digest(facts.dump()) + ".json";
  Reply provider;
  provider.reply = [&](const auto &, const auto &) -> ProviderResult {
    throw Error("summary failed");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.compact(session, cancel), Error);
  EXPECT_FALSE(fs::exists(artifact_directory(config) / ref));
  const auto references = state.references(session.id);
  EXPECT_EQ(std::find(references.begin(), references.end(), ref), references.end());
  EXPECT_EQ(state.load_id(session.id, false).messages[0].checkpoint_state, facts);
}

TEST_F(Checkpoint, LegacySystemExecutionFactsReceiveAnExplicitEmptyRuntimeUpdate) {
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  session.provider_state = {
      {"provider", "anthropic"},
      {"messages", Json::array()},
      {"system", "# Current execution and recovery facts\n[{\"state\":\"running\"}]"}};
  state.save(session);
  Reply provider;
  provider.reply = [](const auto &messages, const auto &) {
    EXPECT_EQ(messages.back().origin, "runtime");
    EXPECT_THAT(messages.back().text, HasSubstr("[]"));
    EXPECT_THAT(messages.back().text, Not(HasSubstr("\"state\":\"running\"")));
    return answer("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "continue after upgrade", cancel).text, "done");
}
} // namespace
