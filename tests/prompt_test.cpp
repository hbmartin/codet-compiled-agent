#include "codet/computer.hpp"
#include "codet/interaction.hpp"
#include "codet/prompt.hpp"
#include "prompt_support.hpp"
#include "temp.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace codet;
using namespace testing;
namespace {
struct NoNetwork : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected network call");
  }
};
struct Desktop : ComputerBackend {
  Json facts = codet::test::desktop_facts();
  Json capabilities() const override {
    return facts;
  }
  DesktopResult invoke(const std::string &, const Json &, const Cancellation &) override {
    throw Error("Unexpected desktop action");
  }
};
struct Images : ImageBackend {
  bool enabled = true;
  bool available() const override {
    return enabled;
  }
  ImageData load(const fs::path &, const std::string &) override {
    throw Error("Unexpected image read");
  }
};
struct CountFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t document_reads = 0;
  std::string read(const fs::path &p, size_t n) const override {
    return native.read(p, n);
  }
  std::string read_prefix(const fs::path &p, size_t n, bool &cut) const override {
    if (p.filename() == "AGENTS.md")
      ++document_reads;
    return native.read_prefix(p, n, cut);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    native.atomic_write(p, s, secret);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    return native.canonical(p);
  }
};
struct Script : Provider {
  std::vector<size_t> output_limits;
  std::function<ProviderResult(const std::string &, const std::vector<codet::Message> &,
                               const Json &)>
      next;
  ProviderResult complete(const Config &config, const std::string &s,
                          const std::vector<codet::Message> &m, const Json &t, const Cancellation &,
                          const ChunkSink &) override {
    EXPECT_LE(Json(m).dump().size(), prompt_budget(config, s, t).available());
    output_limits.push_back(config.max_output_tokens);
    return next(s, m, t);
  }
};
ProviderResult answer(const std::string &text) {
  ProviderResult out;
  out.message.role = "assistant";
  out.message.text = text;
  return out;
}
struct Prompts : Test {
  TempDirectory temp;
  Config config;
  CountFiles files;
  NativeProcesses processes;
  NoNetwork http;
  SystemClock clock;
  Cancellation cancel;
  std::shared_ptr<Desktop> desktop = std::make_shared<Desktop>();
  std::shared_ptr<Images> images = std::make_shared<Images>();
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "work";
    config.roots = {config.cwd};
    config.model = "gpt-6.1-sol";
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
  }
  Tools tools(std::shared_ptr<Interaction> interaction = {}, Questioner questions = {}) {
    return Tools(config, files, processes, http, clock, {}, questions, {}, interaction, desktop,
                 images);
  }
  std::string prompt(Tools &t) {
    const auto facts = t.capabilities();
    return assemble_prompt(config, t.schemas(facts), facts, t.instructions());
  }
  static bool has(const Json &schemas, const std::string &name) {
    return std::any_of(schemas.begin(), schemas.end(),
                       [&](const auto &s) { return s.at("name") == name; });
  }
};
TEST_F(Prompts, ExactModelSelectionDeduplicatesAndRetainsExplicitEmptyOverrides) {
  ASSERT_NE(model_prompt("gpt-6.1-sol"), nullptr);
  EXPECT_EQ(model_prompt("gpt-6.1-sol-unknown"), nullptr);
  EXPECT_EQ(model_prompt("gpt-5.6-sol")->instructions, model_prompt("gpt-5.6-terra")->instructions);
  EXPECT_NE(model_prompt("gpt-6.1-sol")->instructions, model_prompt("gpt-6-astra")->instructions);
  EXPECT_EQ(resolve_prompt(nullptr, "fallback"), "fallback");
  EXPECT_EQ(resolve_prompt("", "fallback"), "");
  auto t = tools();
  config.model = "other-provider-model";
  const auto fallback = prompt(t);
  EXPECT_EQ(fallback.find(model_prompt("gpt-5.5")->instructions), 0u);
  EXPECT_THAT(fallback, Not(HasSubstr("based on GPT")));
  for (const auto &slug : {"gpt-6-astra", "gpt-6.1-sol", "gpt-6-sol", "gpt-6-luna", "gpt-5.6-sol",
                           "gpt-daybreak-blue-latest", "gpt-daybreak-red-latest", "gpt-5.5"}) {
    const std::string base = model_prompt(slug)->instructions;
    for (const auto &unavailable :
         {"functions.", "multi_tool_use.parallel", "skills.read", "skills.list",
          "# Apps (Connectors)", "# Plugins", "Playwright screenshots"})
      EXPECT_THAT(base, Not(HasSubstr(unavailable))) << slug;
  }
}
TEST_F(Prompts, CapabilityGatesAndGuidanceAgreeForEveryDegradation) {
  auto t = tools();
  EXPECT_TRUE(has(t.schemas(), "computer.click"));
  EXPECT_THAT(prompt(t), HasSubstr("prefer screenshots"));
  desktop->facts["accessibility"] = false;
  EXPECT_FALSE(has(t.schemas(), "computer.perform_action"));
  EXPECT_FALSE(has(t.schemas(), "computer.set_value"));
  EXPECT_TRUE(has(t.schemas(), "computer.screenshot"));
  EXPECT_THAT(prompt(t), HasSubstr("System Preferences > Universal Access"));
  config.providers["codex"].image_input = false;
  for (const auto &name : {"computer.screenshot", "computer.move", "computer.click",
                           "computer.drag", "computer.scroll", "view_image"})
    EXPECT_FALSE(has(t.schemas(), name));
  EXPECT_TRUE(has(t.schemas(), "computer.get_state"));
  EXPECT_TRUE(has(t.schemas(), "computer.applescript"));
  EXPECT_THAT(prompt(t), HasSubstr("Do not guess coordinates"));
  config.providers["codex"].image_input = true;
  images->enabled = false;
  EXPECT_FALSE(has(t.schemas(), "computer.screenshot"));
  EXPECT_FALSE(has(t.schemas(), "computer.click"));
  images->enabled = true;
  desktop->facts["framebuffer"] = false;
  EXPECT_FALSE(has(t.schemas(), "computer.screenshot"));
  EXPECT_FALSE(has(t.schemas(), "computer.click"));
  desktop->facts["clipboard"] = false;
  desktop->facts["applescript"] = false;
  EXPECT_FALSE(has(t.schemas(), "computer.clipboard_read"));
  EXPECT_FALSE(has(t.schemas(), "computer.clipboard_write"));
  EXPECT_FALSE(has(t.schemas(), "computer.applescript"));
  desktop->facts["available"] = false;
  for (const auto &s : t.schemas())
    EXPECT_NE(s.at("name").get<std::string>().find("computer."), 0u);
  EXPECT_THAT(prompt(t), HasSubstr("Native desktop control is unavailable"));
}
TEST_F(Prompts, ModesQuestionsAndProjectInstructionsRemainAccurate) {
  files.native.atomic_write(config.cwd / "AGENTS.md", "project says functions.exec stays verbatim");
  auto channel = std::make_shared<Interaction>();
  auto t = tools(channel,
                 [](const std::string &, const std::vector<std::string> &) { return "answer"; });
  for (const auto &mode : {"default", "accept-edits", "bypass", "plan"}) {
    config.permission_mode = mode;
    const auto text = prompt(t);
    EXPECT_THAT(text, HasSubstr("project says functions.exec stays verbatim"));
    EXPECT_THAT(text, HasSubstr(std::string("permission_mode: ") + mode));
    EXPECT_THAT(text, HasSubstr("Explicit deny rules always prevail"));
    EXPECT_THAT(text, HasSubstr(std::string(mode) == "plan" ? "# Plan Mode"
                                                            : "# Collaboration Mode: Default"));
    EXPECT_EQ(has(t.schemas(), "request_user_input"), std::string(mode) == "plan");
    EXPECT_TRUE(has(t.schemas(), "request_user_input_async"));
    if (std::string(mode) == "plan") {
      EXPECT_THAT(text, HasSubstr("denies all shell execution"));
      EXPECT_THAT(text, HasSubstr("read_file for numbered file"));
    }
  }
  auto headless = tools();
  EXPECT_THAT(prompt(headless), HasSubstr("No interactive question tool is available"));
  EXPECT_THAT(prompt(headless), HasSubstr("cannot prompt for approval"));
  auto facts = headless.capabilities();
  facts["approvals"] = true;
  config.permission_mode = "default";
  EXPECT_THAT(assemble_prompt(config, headless.schemas(facts), facts, ""),
              HasSubstr("The application requests required tool approvals"));
  config.approval = "never";
  EXPECT_THAT(assemble_prompt(config, headless.schemas(facts), facts, ""),
              HasSubstr("cannot prompt for approval"));
}
TEST_F(Prompts, RefreshesRuntimeFactsWithoutReloadingProjectDocuments) {
  files.native.atomic_write(config.cwd / "AGENTS.md", "one document load");
  auto t = tools();
  StateStore state(config.home);
  auto session = state.create();
  Script provider;
  int count = 0;
  provider.next = [&](const auto &system, const auto &, const auto &schemas) {
    EXPECT_THAT(system, HasSubstr("one document load"));
    if (++count == 1) {
      EXPECT_TRUE(has(schemas, "computer.screenshot"));
      desktop->facts["available"] = false;
      auto r = answer("");
      r.message.calls = {{"time", "clock.curr_time", Json::object()}};
      return r;
    }
    EXPECT_FALSE(has(schemas, "computer.screenshot"));
    EXPECT_THAT(system, HasSubstr("Native desktop control is unavailable"));
    return answer("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "begin", cancel).text, "done");
  EXPECT_EQ(files.document_reads, 1u);
}
TEST_F(Prompts, TaskPlanRefreshesWithoutReloadingProjectDocuments) {
  files.native.atomic_write(config.cwd / "AGENTS.md", "static document");
  auto t = tools();
  StateStore state(config.home);
  auto session = state.create();
  Script provider;
  size_t requests = 0;
  provider.next = [&](const auto &system, const auto &, const auto &) {
    if (++requests == 1) {
      EXPECT_THAT(system, Not(HasSubstr("Current task plan")));
      auto r = answer("");
      r.message.calls = {
          {"plan",
           "update_plan",
           {{"plan", Json::array({{{"step", "current step"}, {"status", "in_progress"}}})}}}};
      return r;
    }
    EXPECT_THAT(system, HasSubstr("Current task plan"));
    EXPECT_THAT(system, HasSubstr("current step"));
    return answer("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "begin", cancel).text, "done");
  EXPECT_EQ(files.document_reads, 1u);
}
TEST_F(Prompts, HookFeedbackNeverReplacesHumanInstructionDuringCompaction) {
  config.hooks = {
      {"Stop", Json::array({{{"command",
                              "if test ! -f stop-once; then touch stop-once; printf '%s' "
                              "'{\"decision\":\"block\",\"reason\":\"verify results\"}'; fi"}}})}};
  auto t = tools();
  config.context_tokens = codet::test::context_tokens(t, config, 5000);
  StateStore state(config.home);
  auto session = state.create();
  Script provider;
  size_t requests = 0;
  provider.next = [&](const auto &, const auto &messages, const auto &schemas) {
    if (++requests == 1)
      return answer(std::string(12000, 'x'));
    if (schemas.empty()) {
      EXPECT_THAT(messages[0].text,
                  HasSubstr("Latest user instruction (preserve verbatim):\nreal human objective"));
      return answer("Keep real human objective and verify results.");
    }
    EXPECT_EQ(messages[1].text, "real human objective");
    EXPECT_EQ(messages[1].origin, "human");
    return answer("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "real human objective", cancel).text, "done");
  ASSERT_EQ(provider.output_limits.size(), 3u);
  EXPECT_EQ(provider.output_limits[1], config.max_output_tokens);
  EXPECT_EQ(provider.output_limits.back(), config.max_output_tokens);
}
TEST_F(Prompts, CompactionPreservesAsyncSteeringAfterCompletedToolBatch) {
  auto channel = std::make_shared<Interaction>();
  auto t = tools(channel);
  config.context_tokens = codet::test::context_tokens(t, config, 9000);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message old;
  old.role = "system";
  old.text = std::string(3500, 'x');
  session.messages = {old, old};
  state.save(session);
  Script provider;
  int requests = 0;
  provider.next = [&](const auto &system, const auto &messages, const auto &schemas) {
    if (++requests == 1) {
      channel->submit("revised objective: preserve me verbatim");
      auto r = answer(std::string(3000, 'x'));
      r.message.calls = {{"finished-time", "clock.curr_time", Json::object()}};
      return r;
    }
    if (schemas.empty()) {
      EXPECT_THAT(system, HasSubstr("CONTEXT CHECKPOINT COMPACTION"));
      EXPECT_THAT(messages[0].text, Not(HasSubstr("finished-time")));
      EXPECT_THAT(messages[0].text, HasSubstr("revised objective: preserve me verbatim"));
      return answer("Original task and completed clock observation; user revised the objective.");
    }
    EXPECT_EQ(messages.back().text, "revised objective: preserve me verbatim");
    EXPECT_THAT(messages.front().text, StartsWith(std::string(summary_prefix())));
    return answer("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "original task", cancel).text, "done");
  EXPECT_EQ(requests, 3);
}
TEST_F(Prompts, FixedOverheadAndUnshrinkableSummaryFailWithoutRepeatedRequests) {
  auto t = tools();
  StateStore state(config.home);
  auto session = state.create();
  Script provider;
  int requests = 0;
  provider.next = [&](const auto &, const auto &, const auto &) {
    ++requests;
    return answer(std::string(9000, 'x'));
  };
  config.context_tokens = 512;
  Agent agent(config, provider, t, state, {});
  EXPECT_THAT([&] { agent.run(session, "task", cancel); },
              ThrowsMessage<Error>(HasSubstr("Increase context_tokens")));
  EXPECT_EQ(requests, 0);
  session = state.create();
  codet::Message old;
  old.role = "user";
  old.text = std::string(9000, 'x');
  session.messages = {old, old};
  state.save(session);
  config.context_tokens = codet::test::context_tokens(t, config, 5000);
  EXPECT_THROW(agent.run(session, "latest", cancel), Error);
  EXPECT_EQ(requests, 1);
  EXPECT_EQ(state.load(session.id).messages.front().text, old.text);
}
TEST_F(Prompts, OversizedFirstInputFailsWithItsOwnBudgetDiagnostic) {
  auto t = tools();
  config.context_tokens = codet::test::context_tokens(t, config, 1000);
  StateStore state(config.home);
  auto session = state.create();
  Script provider;
  provider.next = [](const auto &, const auto &, const auto &) {
    ADD_FAILURE() << "Oversized first input must not reach the provider";
    return answer("unexpected");
  };
  Agent agent(config, provider, t, state, {});
  const std::string human(3000, 'x');
  EXPECT_THAT([&] { agent.run(session, human, cancel); },
              ThrowsMessage<Error>(HasSubstr("Human input cannot fit without truncation")));
  EXPECT_EQ(state.load_id(session.id).messages.back().text, human);
  EXPECT_TRUE(provider.output_limits.empty());
}
TEST_F(Prompts, AllProviderAdaptersCarryTheComposedInstructions) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  auto t = tools();
  for (const auto &name : {"codex", "openai", "anthropic", "google", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    const auto system = prompt(t);
    const auto body = Json::parse(
        provider.request(config, system, {}, t.schemas(), {{"access_token", "fixture"}}).body);
    const auto carried =
        std::string(name) == "codex"       ? body["input"][0]["content"].get<std::string>()
        : std::string(name) == "anthropic" ? body["system"][0]["text"].get<std::string>()
        : std::string(name) == "google"
            ? body["systemInstruction"]["parts"][0]["text"].get<std::string>()
            : body["messages"][0]["content"].get<std::string>();
    EXPECT_EQ(carried, system) << name;
  }
}
} // namespace
