#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/prompt.hpp"
#include "temp.hpp"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
using namespace codet;
using namespace testing;
namespace {
struct OfflineHttp : HttpTransport {
  std::vector<Json> frames;
  std::vector<HttpRequest> requests;
  bool fail = false;
  HttpResponse perform(const HttpRequest &request, const Cancellation &cancel,
                       const ChunkSink &sink = {}) override {
    cancel.check();
    requests.push_back(request);
    if (fail)
      throw Error("fixture transport failure");
    if (!sink)
      throw Error("unexpected non-streaming request");
    for (const auto &frame : frames) {
      const auto bytes = "data: " + frame.dump() + "\n\n";
      // Split inside keys and UTF-8 too, exercising the real SSE decoder.
      for (size_t at = 0; at < bytes.size(); at += 3)
        sink(bytes.substr(at, 3));
    }
    return {200, ""};
  }
};
struct ScriptedProvider : Provider {
  std::function<ProviderResult(const std::vector<codet::Message> &, const Json &)> next;
  ProviderResult complete(const Config &, const std::string &,
                          const std::vector<codet::Message> &messages, const Json &tools,
                          const Cancellation &cancel, const ChunkSink &,
                          const ItemSink & = {}) override {
    cancel.check();
    return next(messages, tools);
  }
};
struct FakeImages : ImageBackend {
  bool available() const override {
    return true;
  }
  ImageData load(const fs::path &, const std::string &) override {
    throw Error("use validated bytes");
  }
  ImageData load_bytes(const std::string &bytes, const std::string &) override {
    if (bytes == "invalid")
      throw Error("invalid image fixture");
    return {bytes, "image/png", 1, 1};
  }
};
struct SmallCore : Test {
  TempDirectory temp;
  NativeFileSystem files;
  NativeProcesses processes;
  SystemClock clock;
  Cancellation cancel;
  OfflineHttp http;
  Config config;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "repo/nested";
    config.roots = {config.cwd};
    config.provider = "openai";
    config.model = "fixture";
    config.providers["openai"].api_key = "fake";
    config.providers["openai"].wire_api = "chat_completions";
    config.approval = "never";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
  }
  void put(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    files.atomic_write(path, text);
  }
  static ProviderResult answer(const std::string &text) {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = text;
    return result;
  }
};
TEST_F(SmallCore, FragmentedCodexItemsHaveStableIdsPhasesAndLastMessageOutput) {
  config.provider = "codex";
  put(config.home / "auth.json", R"({"codex":{"access_token":"fixture"}})");
  Json progress = {{"type", "message"},
                   {"id", "progress"},
                   {"phase", "commentary"},
                   {"content", Json::array({{{"type", "output_text"}, {"text", "Checking 雪"}}})}};
  Json final = {{"type", "message"},
                {"id", "answer"},
                {"phase", "final_answer"},
                {"content", Json::array({{{"type", "output_text"}, {"text", "Done."}}})}};
  http.frames = {
      {{"type", "response.output_item.added"}, {"output_index", 0}, {"item", progress}},
      {{"type", "response.output_text.delta"},
       {"output_index", 0},
       {"item_id", "progress"},
       {"delta", "Checking 雪"}},
      {{"type", "response.output_item.added"}, {"output_index", 1}, {"item", final}},
      {{"type", "response.output_text.delta"},
       {"output_index", 1},
       {"item_id", "answer"},
       {"delta", "Done."}},
      {{"type", "response.completed"}, {"response", {{"output", Json::array({progress, final})}}}}};
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  std::vector<Json> events;
  Agent agent(config, provider, tools, state, [&](const Json &event) { events.push_back(event); });
  EXPECT_EQ(agent.run(session, "inspect", cancel).text, "Done.");
  const auto restored = state.load(session.id);
  ASSERT_EQ(restored.messages.back().assistant_items.size(), 2u);
  EXPECT_EQ(restored.messages.back().assistant_items[0].phase, "commentary");
  EXPECT_EQ(restored.messages.back().assistant_items[1].id, "answer");
  for (const auto &id : {"progress", "answer"}) {
    size_t starts = 0, updates = 0, ends = 0;
    for (const auto &event : events) {
      const auto item = event.value("item", Json::object());
      if (item.value("id", "") != id)
        continue;
      starts += event.at("type") == "item.started";
      updates += event.at("type") == "item.updated";
      ends += event.at("type") == "item.completed";
    }
    EXPECT_EQ(starts, 1u);
    EXPECT_EQ(updates, 1u);
    EXPECT_EQ(ends, 1u);
  }
  const auto fork = state.fork(restored);
  EXPECT_EQ(Json(fork.messages.back().assistant_items),
            Json(restored.messages.back().assistant_items));
}
TEST_F(SmallCore, NullableProviderItemMetadataUsesStableUntaggedFallback) {
  config.provider = "codex";
  put(config.home / "auth.json", R"({"codex":{"access_token":"fixture"}})");
  Json message = {{"type", "message"},
                  {"id", nullptr},
                  {"phase", nullptr},
                  {"content", Json::array({{{"type", "output_text"}, {"text", "legacy"}}})}};
  http.frames = {
      {{"type", "response.output_item.added"}, {"output_index", 0}, {"item", message}},
      {{"type", "response.output_text.delta"}, {"output_index", 0}, {"delta", "legacy"}},
      {{"type", "response.completed"}, {"response", {{"output", Json::array({message})}}}}};
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  std::vector<Json> events;
  const auto result = provider.complete(config, "system", {}, Json::array(), cancel, {},
                                        [&](const Json &event) { events.push_back(event); });
  ASSERT_EQ(result.message.assistant_items.size(), 1u);
  EXPECT_TRUE(result.message.assistant_items[0].phase.empty());
  EXPECT_FALSE(result.message.assistant_items[0].id.empty());
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].at("item").at("id"), result.message.assistant_items[0].id);
  EXPECT_EQ(events[1].at("item").at("id"), result.message.assistant_items[0].id);
}
TEST_F(SmallCore, UntaggedMessagesAndMissingItemIdsRemainCompatible) {
  ScriptedProvider provider;
  provider.next = [&](const auto &, const auto &) {
    auto result = answer("firstsecond");
    result.message.assistant_items = {{"", "first", ""}, {"", "second", ""}};
    return result;
  };
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "inspect", cancel).text, "second");
  const auto &items = session.messages.back().assistant_items;
  ASSERT_EQ(items.size(), 2u);
  EXPECT_FALSE(items[0].id.empty());
  EXPECT_NE(items[0].id, items[1].id);
  EXPECT_EQ(Json(session.messages).get<std::vector<codet::Message>>().back().text, "firstsecond");
  auto legacy = Json::parse(R"({"role":"assistant","text":"legacy"})").get<codet::Message>();
  EXPECT_TRUE(legacy.assistant_items.empty());
}
TEST_F(SmallCore, AssistantPhasesAgreeThroughoutFragmentedLifecycles) {
  config.provider = "codex";
  put(config.home / "auth.json", R"({"codex":{"access_token":"fixture"}})");
  for (const Json &phase : Json::array(
           {"commentary", "partial_answer", "final_answer", "future", "missing", "", nullptr, 7})) {
    SCOPED_TRACE(phase.dump());
    Json message = {{"type", "message"},
                    {"id", "phase-item"},
                    {"phase", phase},
                    {"content", Json::array({{{"type", "output_text"}, {"text", "done"}}})}};
    if (phase == "missing")
      message.erase("phase");
    http.frames = {
        {{"type", "response.output_item.added"}, {"output_index", 0}, {"item", message}},
        {{"type", "response.output_text.delta"}, {"output_index", 0}, {"delta", "done"}},
        {{"type", "response.completed"}, {"response", {{"output", Json::array({message})}}}}};
    CredentialStore credentials(config.home, files, clock);
    ApiProvider provider(http, credentials, clock);
    Tools tools(config, files, processes, http, clock);
    StateStore state(config.home);
    auto session = state.create();
    std::vector<Json> events;
    Agent agent(config, provider, tools, state,
                [&](const Json &event) { events.push_back(event); });
    EXPECT_EQ(agent.run(session, "inspect", cancel).text, "done");
    const std::string expected =
        phase == "commentary" || phase == "partial_answer" || phase == "final_answer"
            ? phase.get<std::string>()
            : "";
    size_t seen = 0;
    for (const auto &event : events) {
      const auto item = event.value("item", Json::object());
      if (item.value("id", "") != "phase-item")
        continue;
      ++seen;
      EXPECT_EQ(item.value("phase", ""), expected);
      EXPECT_EQ(item.contains("phase"), !expected.empty());
    }
    EXPECT_EQ(seen, 3u);
    const auto saved = state.load(session.id).messages.back();
    EXPECT_EQ(saved.assistant_items.at(0).phase, expected);
    EXPECT_EQ(saved.replay.at(0), message);
  }
}
TEST_F(SmallCore, InvalidDiscoveryRootsKeepGuidanceAndOtherSkillsWithDeduplicatedWarnings) {
  put(config.home / "AGENTS.md", "global-guidance");
  put(config.cwd / "AGENTS.md", "project-guidance");
  put(config.home / "memory", "not a directory");
  put(config.cwd / ".codet", "not a directory parent");
  put(config.cwd / ".claude/skills", "not a directory");
  put(config.cwd / ".agents/skills/good/SKILL.md", "---\nname: good\ndescription: available\n---");
  std::vector<std::string> warnings;
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { warnings.push_back(s); });
  const auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr("global-guidance"));
  EXPECT_THAT(text, HasSubstr("project-guidance"));
  EXPECT_THAT(text, HasSubstr("- good:"));
  EXPECT_THAT(warnings, Contains(HasSubstr("memory")));
  EXPECT_THAT(warnings, Contains(HasSubstr(".claude/skills")));
  EXPECT_THAT(warnings, Contains(HasSubstr(".codet/skills")));
  const auto count = warnings.size();
  EXPECT_EQ(tools.instructions(), text);
  EXPECT_EQ(warnings.size(), count);
}
TEST_F(SmallCore, UnreadableDiscoveryRootsDoNotAbortOtherGuidance) {
  const auto memory = config.home / "memory", skills = config.cwd / ".agents/skills";
  fs::create_directories(memory);
  fs::create_directories(skills);
  struct Restore {
    fs::path first, second;
    ~Restore() {
      std::error_code error;
      fs::permissions(first, fs::perms::owner_all, error);
      fs::permissions(second, fs::perms::owner_all, error);
    }
  } restore{memory, skills};
  fs::permissions(memory, fs::perms::none);
  fs::permissions(skills, fs::perms::none);
  std::error_code error;
  fs::directory_iterator check(skills, error);
  if (!error)
    GTEST_SKIP() << "Execution user can read permission-denied directories";
  put(config.cwd / "AGENTS.md", "retained-guidance");
  put(config.home / "skills/good/SKILL.md", "---\nname: good\ndescription: retained-skill\n---");
  std::vector<std::string> warnings;
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { warnings.push_back(s); });
  const auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr("retained-guidance"));
  EXPECT_THAT(text, HasSubstr("retained-skill"));
  EXPECT_THAT(warnings, Contains(HasSubstr(memory.string())));
  EXPECT_THAT(warnings, Contains(HasSubstr(skills.string())));
}
TEST_F(SmallCore, AliasedUserAndAncestorSkillsRespectAnchorsAndDenials) {
  struct RestoreHome {
    std::optional<std::string> value;
    RestoreHome() {
      if (const auto *home = std::getenv("HOME"))
        value = home;
    }
    ~RestoreHome() {
      if (value)
        ::setenv("HOME", value->c_str(), 1);
      else
        ::unsetenv("HOME");
    }
  } restore;
  const auto real = temp.root / "user", alias = temp.root / "user-alias";
  const auto skill = real / ".agents/skills/user/SKILL.md";
  put(skill, "---\nname: user\ndescription: user-skill\n---");
  fs::create_directory_symlink(real, alias);
  ASSERT_EQ(::setenv("HOME", alias.c_str(), 1), 0);
  config.permission_mode = "plan";
  const auto repo = config.cwd.parent_path();
  fs::create_directory(repo / ".git");
  const auto ancestor = repo / ".agents/skills/ancestor/SKILL.md";
  put(ancestor, "---\nname: ancestor\ndescription: ancestor-skill\n---");
  Tools tools(config, files, processes, http, clock);
  const auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr(files.canonical(skill).string()));
  const auto read = [&](const fs::path &path) {
    return tools.execute({unique_id(), "read_file", {{"path", files.canonical(path).string()}}},
                         cancel);
  };
  EXPECT_FALSE(read(skill).error);
  EXPECT_FALSE(read(ancestor).error);
  config.set_deny({"read_file(*SKILL.md)"});
  EXPECT_TRUE(read(skill).error);
  config.set_deny({});
  // An advertised descendant replaced with a symlink never gains private access.
  const auto descendant = real / ".agents/skills/user";
  config.hooks["PreToolUse"] = Json::array(
      {{{"matcher", "read_file"},
        {"command", "mv '" + descendant.string() + "' '" + descendant.string() + ".old'; ln -s '" +
                        descendant.string() + ".old' '" + descendant.string() + "'"}}});
  EXPECT_TRUE(tools
                  .execute({unique_id(),
                            "read_file",
                            {{"path", (alias / ".agents/skills/user/SKILL.md").string()}}},
                           cancel)
                  .error);
}
TEST_F(SmallCore, InstructionPrecedenceAndAncestorSkillMetadataAreDeterministic) {
  const auto repo = config.cwd.parent_path();
  fs::create_directory(repo / ".git");
  put(config.home / "AGENTS.md", "global-guidance");
  put(repo / "AGENTS.md", "root-guidance");
  put(repo / "CLAUDE.md", "ignored-claude");
  put(config.cwd / "AGENTS.override.md", "local-override");
  put(config.cwd / "AGENTS.md", "ignored-agents");
  put(repo / ".agents/skills/demo/SKILL.md",
      "---\nname: duplicate\ndescription: root-version\n---\nbody");
  put(config.cwd / ".agents/skills/local/SKILL.md",
      "---\nname: 'duplicate'\ndescription: >-\n  nearest version\n  with details\n---\nprivate "
      "body");
  put(config.home / "skills/global/SKILL.md",
      "---\nname: duplicate\ndescription: ignored-global\n---");
  Tools tools(config, files, processes, http, clock);
  const auto text = tools.instructions();
  EXPECT_LT(text.find("global-guidance"), text.find("root-guidance"));
  EXPECT_LT(text.find("root-guidance"), text.find("local-override"));
  EXPECT_THAT(text, Not(HasSubstr("ignored-")));
  EXPECT_THAT(text, HasSubstr("duplicate: nearest version with details"));
  EXPECT_THAT(text, HasSubstr("local/SKILL.md"));
  EXPECT_THAT(text, Not(HasSubstr("root-version")));
  EXPECT_THAT(text, Not(HasSubstr("private body")));
  const auto read =
      tools.execute({unique_id(),
                     "read_file",
                     {{"path", (config.cwd / ".agents/skills/local/SKILL.md").string()}}},
                    cancel);
  EXPECT_FALSE(read.error) << read.content;
}
TEST_F(SmallCore, MalformedMetadataFallsBackAndCatalogIsBounded) {
  std::vector<std::string> warnings;
  put(config.cwd / ".agents/skills/bad/SKILL.md",
      "---\nname: wrong\ndescription: [unsupported]\n---");
  for (int i = 0; i < 105; ++i)
    put(config.cwd / ".agents/skills" / ("skill-" + std::to_string(i)) / "SKILL.md",
        "---\ndescription: " + std::string(2000, 'x') + "\n---");
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { warnings.push_back(s); });
  const auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr("- bad:"));
  EXPECT_THAT(text, Not(HasSubstr("- wrong:")));
  EXPECT_LT(text.size(), 18000u);
  EXPECT_FALSE(warnings.empty());
}
TEST_F(SmallCore, SkillSymlinksCannotGrantInspectionAndExplicitDenialsRemain) {
  const auto outside = temp.root / "outside";
  put(outside / "SKILL.md", "---\nname: leaked\ndescription: outside\n---");
  fs::create_directories(config.cwd / ".agents/skills");
  fs::create_directory_symlink(outside, config.cwd / ".agents/skills/linked");
  Tools tools(config, files, processes, http, clock);
  EXPECT_THAT(tools.instructions(), Not(HasSubstr("leaked")));
  EXPECT_TRUE(
      tools.execute({unique_id(), "read_file", {{"path", (outside / "SKILL.md").string()}}}, cancel)
          .error);
}
TEST_F(SmallCore, EffortMigrationForkAndResolutionPreserveSelectionPrecedence) {
  // A real schema-3 database is migrated without discarding any conversation.
  {
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(
            db,
            "CREATE TABLE sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent TEXT,updated "
            "INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle',provider "
            "TEXT NOT NULL DEFAULT '',model TEXT NOT NULL DEFAULT ''); PRAGMA user_version=3;",
            nullptr, nullptr, nullptr),
        SQLITE_OK);
    sqlite3_close(db);
  }
  StateStore state(config.home);
  auto session = state.create();
  EXPECT_FALSE(session.effort);
  session.provider = config.provider;
  session.model = config.model;
  session.effort = "high";
  state.save(session);
  auto fork = state.fork(state.load(session.id));
  ASSERT_TRUE(fork.effort);
  EXPECT_EQ(*fork.effort, "high");
  config.effort = "low";
  EXPECT_EQ(resolve_effort(config, &fork), "high");
  config.selected_effort = "medium";
  EXPECT_EQ(resolve_effort(config, &fork), "medium");
  config.selected_effort.reset();
  config.model = "changed";
  EXPECT_EQ(resolve_effort(config, &fork), "low");
  config.provider = "codex";
  config.model = "gpt-6.1-sol";
  config.effort = "invalid";
  EXPECT_THROW(validate_effort(config), Error);
  config.model = "unknown-model";
  EXPECT_NO_THROW(validate_effort(config));
}
TEST_F(SmallCore, ManualCompactionSharesBudgetsAndPreservesLatestHumanAndFailureHistory) {
  config.hooks["PreCompact"] = Json::array({{{"command", "cat > compact-hook.json"}}});
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  for (int i = 0; i < 8; ++i) {
    codet::Message message;
    message.role = i % 2 ? "assistant" : "user";
    message.text = std::string(4000, 'a');
    if (i == 6)
      message.text = "latest human steering";
    session.messages.push_back(message);
  }
  state.save(session);
  ScriptedProvider provider;
  provider.next = [&](const auto &, const auto &schemas) {
    EXPECT_TRUE(schemas.empty());
    return answer("summary of completed work");
  };
  std::vector<Json> events;
  Agent agent(config, provider, tools, state, [&](const auto &e) { events.push_back(e); });
  EXPECT_EQ(agent.compact(session, cancel).text, "Conversation compacted.");
  EXPECT_THAT(Json(session.messages).dump(), HasSubstr("latest human steering"));
  EXPECT_EQ(events.back().at("type"), "codet.compacted");
  EXPECT_EQ(Json::parse(files.read(config.cwd / "compact-hook.json")).at("trigger"), "manual");
  codet::Message newer;
  newer.role = "user";
  newer.text = "new work to summarize";
  session.messages.push_back(newer);
  state.save(session);
  const auto before = Json(state.load(session.id).messages);
  provider.next = [&](const auto &, const auto &) -> ProviderResult {
    throw Error("summary failed");
  };
  EXPECT_THROW(agent.compact(session, cancel), Error);
  EXPECT_EQ(Json(state.load(session.id).messages), before);
  cancel.requested = true;
  EXPECT_THROW(agent.compact(session, cancel), Error);
  EXPECT_EQ(Json(state.load(session.id).messages), before);
}
TEST_F(SmallCore, EmptyManualCompactionNeverCallsProvider) {
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  ScriptedProvider provider;
  provider.next = [&](const auto &, const auto &) -> ProviderResult {
    ADD_FAILURE();
    throw Error("unexpected provider");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.compact(session, cancel).text, "No older history to compact.");
}
TEST_F(SmallCore, ExplicitImagesAuthorizeOnlyTheNamedFileAndRoundTripImageOnlyPrompt) {
  FakeImages backend;
  const auto outside = temp.root / "outside.png";
  put(outside, "encoded-image");
  const auto images = input_images(config, files, backend, {outside}, cancel);
  ASSERT_EQ(images.size(), 1u);
  EXPECT_EQ(config.roots, std::vector<fs::path>{config.cwd});
  Tools tools(config, files, processes, http, clock);
  StateStore state(config.home);
  auto session = state.create();
  ScriptedProvider provider;
  provider.next = [&](const auto &messages, const auto &) {
    EXPECT_TRUE(messages.back().text.empty());
    EXPECT_EQ(messages.back().images.size(), 1u);
    return answer("image received");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "", cancel, Json(), true, images).text, "image received");
  config.add_denied("view_image");
  EXPECT_THROW(input_images(config, files, backend, {outside}, cancel), Error);
  config.set_deny({});
  EXPECT_THROW(input_images(config, files, backend, std::vector<fs::path>(5, outside), cancel),
               Error);
  config.providers["openai"].image_input = false;
  EXPECT_THROW(input_images(config, files, backend, {outside}, cancel), Error);
}
TEST_F(SmallCore, MultipleImagesRespectCombinedBudgetBeforePersistence) {
  FakeImages backend;
  const auto first = temp.root / "first.png", second = temp.root / "second.png";
  put(first, std::string(5 * 1024 * 1024, 'a'));
  put(second, std::string(5 * 1024 * 1024, 'b'));
  EXPECT_THROW(input_images(config, files, backend, {first, second}, cancel), Error);
  EXPECT_FALSE(fs::exists(config.home / "artifacts"));
}
} // namespace
TEST_F(SmallCore, ImageInspectionAcceptsCaseAliasesAndHardLinksWithValidatedHandles) {
  config.permission_mode = "plan";
  put(config.cwd / "CaseImage.png", "pixels");
  fs::create_hard_link(config.cwd / "CaseImage.png", config.cwd / "HardLink.png");
  Tools tools(config, files, processes, http, clock, {}, {}, {}, {}, {},
              std::make_shared<FakeImages>());
  for (const auto *name : {"CaseImage.png", "caseimage.png", "HardLink.png"}) {
    if (!fs::exists(config.cwd / name))
      continue;
    const auto result = tools.execute({unique_id(), "view_image", {{"path", name}}}, cancel);
    EXPECT_FALSE(result.error) << name << ": " << result.content;
    EXPECT_EQ(result.images.size(), 1u);
  }
  config.set_deny({"view_image(*HardLink*)"});
  EXPECT_TRUE(tools.execute({unique_id(), "view_image", {{"path", "HardLink.png"}}}, cancel).error);
}
