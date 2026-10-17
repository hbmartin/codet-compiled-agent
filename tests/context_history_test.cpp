#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/interaction.hpp"
#include "codet/prompt.hpp"
#include "prompt_support.hpp"
#include "temp.hpp"
#include <fstream>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sqlite3.h>
#include <thread>
using namespace codet;
using namespace testing;
namespace {
struct Offline : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected network");
  }
};
struct InspectFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t calls = 0, bytes = 0, largest = 0;
  bool fail_results = false;
  std::optional<fs::path> unavailable;
  mutable std::function<void()> after_open;
  struct Handle : ReadHandle {
    std::unique_ptr<ReadHandle> input;
    const InspectFiles &owner;
    Handle(std::unique_ptr<ReadHandle> file, const InspectFiles &o)
        : input(std::move(file)), owner(o) {}
    fs::path path() const override {
      return input->path();
    }
    uint64_t size() const override {
      return input->size();
    }
    void seek(uint64_t n) override {
      input->seek(n);
    }
    size_t read(char *p, size_t n) override {
      ++owner.calls;
      owner.largest = std::max(owner.largest, n);
      const auto got = input->read(p, n);
      owner.bytes += got;
      return got;
    }
  };
  std::unique_ptr<ReadHandle> open_read(const fs::path &p) const override {
    auto handle = native.open_read(p);
    if (after_open) {
      auto callback = std::move(after_open);
      after_open = {};
      callback();
    }
    return std::make_unique<Handle>(std::move(handle), *this);
  }
  std::string read(const fs::path &p, size_t n) const override {
    return native.read(p, n);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    if (fail_results && p.parent_path().parent_path().filename() == "results")
      throw Error("injected result archive persistence failure");
    native.atomic_write(p, s, secret);
  }
  void atomic_stream(const fs::path &p, const std::function<void(const Writer &)> &f,
                     bool secret) override {
    native.atomic_stream(p, f, secret);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    const auto value = native.canonical(p);
    if (unavailable && value == *unavailable)
      throw fs::filesystem_error("injected discovery failure", p,
                                 std::make_error_code(std::errc::io_error));
    return value;
  }
};
struct Scripted : Provider {
  std::function<ProviderResult(const Config &, const std::string &,
                               const std::vector<codet::Message> &, const Json &)>
      script;
  ProviderResult complete(const Config &c, const std::string &s,
                          const std::vector<codet::Message> &m, const Json &schemas,
                          const Cancellation &, const ChunkSink &, const ItemSink & = {}) override {
    EXPECT_LE(provider_history_bytes(c, m), prompt_budget(c, s, schemas).available());
    return script(c, s, m, schemas);
  }
};
ProviderResult text_result(std::string text) {
  ProviderResult out;
  out.message.role = "assistant";
  out.message.text = std::move(text);
  return out;
}
struct History : Test {
  TempDirectory temp;
  Config config;
  InspectFiles files;
  NativeProcesses processes;
  Offline http;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "work[1]";
    config.roots = {config.cwd};
    config.approval = "never";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
  }
  ToolResult read(Tools &t, const fs::path &p, Json args = Json::object()) {
    args["path"] = p.string();
    return t.execute({unique_id(), "read_file", args}, cancel);
  }
  void put(const fs::path &p, const std::string &s) {
    fs::create_directories(p.parent_path());
    files.native.atomic_write(p, s);
  }
};
TEST_F(History, PrivateInspectionIsAvailableHeadlesslyInEveryModeButWorkspaceInspectionIsPlanOnly) {
  const auto memory = config.home / "memory/note.md";
  const auto skill = config.home / "skills/demo";
  put(memory, "private memory");
  put(skill / "SKILL.md", "skill instructions");
  put(skill / "support/data.txt", "support");
  put(config.home / "credentials.json", "secret");
  put(config.cwd / "ordinary.txt", "workspace");
  for (const auto *mode : {"default", "plan", "accept-edits", "bypass"}) {
    config.permission_mode = mode;
    Tools t(config, files, processes, http, clock);
    t.instructions();
    t.use_session("active");
    EXPECT_FALSE(read(t, memory).error) << mode;
    EXPECT_FALSE(read(t, skill / "support/data.txt").error) << mode;
    EXPECT_EQ(read(t, config.cwd / "ordinary.txt").error, std::string(mode) != "plan");
    EXPECT_TRUE(read(t, config.home / "credentials.json").error);
    config.set_deny({"read_file(*note.md)"});
    EXPECT_TRUE(read(t, memory).error) << mode;
    config.set_deny({});
  }
}
TEST_F(History, OnlyAdvertisedSkillsAndReferencedCapturesGainPrivateAccess) {
  put(config.home / "skills/demo/SKILL.md", "advertised");
  Tools t(config, files, processes, http, clock);
  t.instructions();
  t.use_session("active");
  put(config.home / "skills/late/SKILL.md", "unadvertised");
  EXPECT_TRUE(read(t, config.home / "skills/late/SKILL.md").error);
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "done";
  result.text =
      Json(
          {{"content", std::string(30000, 'x')}, {"is_error", false}, {"metadata", Json::object()}})
          .dump();
  const auto path = t.retain_result(result);
  EXPECT_FALSE(read(t, path, {{"byte_offset", 0}, {"byte_limit", 100}}).error);
  Tools foreign(config, files, processes, http, clock);
  foreign.use_session("foreign");
  EXPECT_TRUE(read(foreign, path).error);
  foreign.reference_results({result});
  EXPECT_FALSE(read(foreign, path).error); // Explicit inherited result reference grants access.
  config.set_deny({"read_file(*results*)"});
  EXPECT_TRUE(read(t, path).error);
}
TEST_F(History, HooksCannotExchangePrivateFilesOrAncestorDirectoriesForSymlinks) {
  const auto memory = config.home / "memory/note.md";
  const auto foreign = temp.root / "foreign";
  put(memory, "original");
  put(foreign / "note.md", "foreign secret");
  config.hooks["PreToolUse"] = Json::array(
      {{{"matcher", "read_file"},
        {"command", "mv '" + memory.string() + "' '" + memory.string() + ".old'; ln -s '" +
                        (foreign / "note.md").string() + "' '" + memory.string() + "'"}}});
  Tools t(config, files, processes, http, clock);
  EXPECT_TRUE(read(t, memory).error);
  fs::remove(memory);
  fs::rename(memory.string() + ".old", memory);
  config.hooks["PreToolUse"][0]["command"] =
      "mv '" + memory.parent_path().string() + "' '" + memory.parent_path().string() +
      ".old'; ln -s '" + foreign.string() + "' '" + memory.parent_path().string() + "'";
  EXPECT_TRUE(read(t, memory).error);
  EXPECT_EQ(files.bytes, 0u);
}
TEST_F(History, FinalValidationHonorsNewDeniesAndConsumesTheOpenedHandle) {
  const auto memory = config.home / "memory/note.md";
  put(memory, "original permitted bytes");
  Tools t(config, files, processes, http, clock);
  files.after_open = [&] { config.set_deny({"read_file(*note.md)"}); };
  EXPECT_TRUE(read(t, memory).error);
  EXPECT_EQ(files.bytes, 0u);
  config.set_deny({});
  files.after_open = [&] { files.native.atomic_write(memory, "replacement bytes"); };
  const auto result = read(t, memory);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_THAT(result.content, HasSubstr("original permitted bytes"));
  EXPECT_THAT(result.content, Not(HasSubstr("replacement bytes")));
}
TEST_F(History, BytePagesReturnActualUtf8BoundariesAndUseBoundedReads) {
  const auto memory = config.home / "memory/long.md";
  put(memory, "a雪🌲z" + std::string(2 * 1024 * 1024, 'x'));
  Tools t(config, files, processes, http, clock);
  auto page = read(t, memory, {{"byte_offset", 0}, {"byte_limit", 3}});
  ASSERT_FALSE(page.error) << page.content;
  EXPECT_EQ(page.content, "a");
  EXPECT_EQ(page.metadata["next_byte_offset"], 1);
  page = read(t, memory, {{"byte_offset", 1}, {"byte_limit", 7}});
  EXPECT_EQ(page.content, "雪🌲");
  EXPECT_EQ(page.metadata["next_byte_offset"], 8);
  page = read(t, memory, {{"byte_offset", 8}, {"byte_limit", 1048576}});
  EXPECT_GT(page.content.size(), config.tool_output_limit / 2);
  EXPECT_LE(page.content.size(), config.tool_output_limit);
  EXPECT_EQ(page.metadata["next_byte_offset"], 8 + page.content.size());
  EXPECT_LE(files.largest, 4096u);
  EXPECT_LT(files.bytes, config.tool_output_limit + 10000);
  EXPECT_TRUE(read(t, memory, {{"byte_offset", 0}, {"offset", 1}}).error);
  EXPECT_TRUE(read(t, memory, {{"byte_offset", -1}}).error);
  EXPECT_TRUE(read(t, memory, {{"byte_limit", 0}}).error);
  EXPECT_FALSE(read(t, memory, {{"byte_offset", 3000000}}).error);
}
TEST_F(History, DefaultBudgetProjectsLargeBatchesWithoutRerunningOrLosingDurableResults) {
  EXPECT_EQ(config.context_tokens, 65536u);
  config.permission_mode = "bypass";
  put(config.cwd / "source", std::string(45000, 'x'));
  StateStore state(config.home);
  auto session = state.create();
  Tools t(config, files, processes, http, clock);
  Scripted provider;
  size_t calls = 0;
  provider.script = [&](const Config &, const std::string &, const auto &messages,
                        const Json &schemas) {
    EXPECT_FALSE(schemas.empty()); // No compaction for a single ordinary output.
    if (++calls == 1) {
      auto out = text_result("");
      out.message.calls = {
          {"one",
           "exec_command",
           {{"cmd", "cat source"}, {"login", false}, {"max_output_tokens", 20000}}},
          {"two",
           "exec_command",
           {{"cmd", "cat source"}, {"login", false}, {"max_output_tokens", 20000}}},
          {"three",
           "exec_command",
           {{"cmd", "cat source"}, {"login", false}, {"max_output_tokens", 20000}}},
          {"four",
           "exec_command",
           {{"cmd", "cat source"}, {"login", false}, {"max_output_tokens", 20000}}}};
      return out;
    }
    size_t references = 0;
    for (const auto &m : messages)
      if (m.role == "tool") {
        const Json result = Json::parse(m.text);
        EXPECT_FALSE(result["is_error"]);
        EXPECT_EQ(Json::parse(result["content"].get<std::string>())["exit_code"], 0);
        if (!m.result_file.empty()) {
          ++references;
          const auto path =
              artifact_directory(config) / result["metadata"]["result_path"].get<std::string>();
          const codet::Message full = Json::parse(files.native.read(path)).get<codet::Message>();
          EXPECT_GE(full.text.size(), 45000u);
          EXPECT_FALSE(read(t, path, {{"byte_limit", 80}}).error);
        }
      }
    EXPECT_GT(references, 0u);
    return text_result("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "read the source once per requested call", cancel).text, "done");
  EXPECT_EQ(calls, 2u);
  const auto saved = state.load_id(session.id);
  for (const auto &m : saved.messages)
    if (m.role == "tool")
      EXPECT_TRUE(m.previewed);
}
TEST_F(History, SummaryEscapingEmptyAndExhaustionPreserveDurableHistory) {
  for (int failure = 0; failure < 3; ++failure) {
    StateStore state(config.home);
    auto session = state.create();
    codet::Message old;
    old.role = "assistant";
    old.text = std::string(20000, 'x');
    session.messages = {old, old};
    state.save(session);
    Tools t(config, files, processes, http, clock);
    config.context_tokens = codet::test::context_tokens(t, config, 7000);
    Scripted provider;
    size_t requests = 0;
    provider.script = [&](const Config &c, const std::string &system, const auto &,
                          const Json &schemas) {
      ++requests;
      EXPECT_TRUE(schemas.empty());
      EXPECT_EQ(c.max_output_tokens, 8192u);
      EXPECT_THAT(system, HasSubstr("UTF-8 bytes of summary prose"));
      auto out = text_result(failure == 0 ? std::string(8192, '\\') : "");
      if (failure == 2)
        out.stop_reason = "MAX_TOKENS";
      return out;
    };
    Agent agent(config, provider, t, state, {});
    EXPECT_THAT([&] { agent.run(session, "human objective", cancel); },
                ThrowsMessage<Error>(HasSubstr("history retained")));
    EXPECT_EQ(requests, failure == 0 ? 2u : 1u);
    EXPECT_EQ(state.load_id(session.id).messages.front().text, old.text);
  }
}
TEST_F(History, PlanViewIsBoundedPrioritizedAndOversizedLegacyPlansRemainReadable) {
  Tools t(config, files, processes, http, clock);
  t.use_session("active");
  Json plan = Json::array();
  for (int i = 0; i < 100; ++i)
    plan.push_back({{"step", std::string(300, 'x')},
                    {"status", i == 99  ? "in_progress"
                               : i < 80 ? "completed"
                                        : "pending"}});
  auto result = t.execute({"plan", "update_plan", {{"plan", plan}}}, cancel);
  ASSERT_FALSE(result.error) << result.content;
  const auto view = t.dynamic_context();
  EXPECT_LE(view.size(), 8192u);
  EXPECT_THAT(view, HasSubstr("100. [in_progress]"));
  EXPECT_LT(view.find("100."), view.find("81."));
  const auto saved = files.native.read(config.home / "plans/active.json");
  plan[0]["step"] = std::string(1048576, 'x');
  EXPECT_TRUE(t.execute({"big-plan", "update_plan", {{"plan", plan}}}, cancel).error);
  EXPECT_EQ(files.native.read(config.home / "plans/active.json"), saved);
  put(config.home / "plans/legacy.json", plan.dump());
  t.use_session("legacy");
  EXPECT_LT(t.dynamic_context().size(), 1024u);
  EXPECT_FALSE(read(t, config.home / "plans/legacy.json", {{"byte_limit", 100}}).error);
}
TEST_F(History, LedgerPreservesEveryObservationAndRetryIdsWithUnknownLegacyChronology) {
  const auto root = artifact_directory(config);
  const auto artifact = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  codet::Message one;
  one.role = "tool";
  one.message_id = "first";
  one.tool_call_id = "shot";
  one.images = {artifact, artifact};
  auto two = one;
  two.message_id = "second";
  two.images.resize(1);
  const auto index = archive_image_history(root, files, "active", {one, two});
  EXPECT_EQ(image_history_position(root, files, "active")["observations"], 3);
  const auto before = files.native.read(index);
  archive_image_history(root, files, "active", {one, two});
  EXPECT_EQ(files.native.read(index), before);
  EXPECT_THAT(before, HasSubstr("first:0"));
  EXPECT_THAT(before, HasSubstr("first:1"));
  EXPECT_THAT(before, HasSubstr("second:0"));
  EXPECT_THAT(before, Not(HasSubstr(root.string())));
  put(artifact_index_path(root, "legacy"), Json(artifact).dump() + "\n");
  archive_image_history(root, files, "legacy", {two});
  const auto legacy = files.native.read(artifact_index_path(root, "legacy"));
  EXPECT_THAT(legacy, HasSubstr("\"chronology\":\"unknown\""));
  EXPECT_EQ(image_history_position(root, files, "legacy")["legacy_unknown"], 1);
  Tools t(config, files, processes, http, clock);
  t.use_session("active");
  EXPECT_THAT(t.dynamic_context(), HasSubstr("\"observations\":3"));
  EXPECT_FALSE(read(t, index, {{"offset", 4}, {"limit", 1}}).error);
  EXPECT_TRUE(read(t, artifact_index_path(root, "legacy")).error);
}
TEST_F(History, RelocationAndForkRebindOnlyExistingLedgersAndReplayRelativeImages) {
  const auto artifact = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  StateStore state(config.home);
  auto session = state.create();
  codet::Message m;
  m.role = "user";
  m.message_id = unique_id();
  m.images = {artifact};
  session.messages = {m};
  state.save(session);
  const auto plain = state.fork(session);
  EXPECT_FALSE(fs::exists(artifact_index_path(artifact_directory(config), plain.id)));
  archive_image_history(artifact_directory(config), files, session.id, session.messages);
  session.messages[0].artifact_index = artifact_index_reference(session.id);
  state.save(session);
  const auto moved = temp.root / "relocated";
  fs::copy(config.home, moved, fs::copy_options::recursive);
  config.home = moved;
  StateStore relocated(config.home);
  const auto loaded = relocated.load_id(session.id);
  EXPECT_FALSE(fs::path(loaded.messages[0].images[0].path).is_absolute());
  EXPECT_EQ(image_data_url(loaded.messages[0].images[0], config), "data:image/png;base64,cGl4ZWxz");
  const auto child = relocated.fork(loaded);
  EXPECT_EQ(child.messages[0].artifact_index, artifact_index_reference(child.id));
  EXPECT_EQ(image_history_position(artifact_directory(config), files, child.id)["observations"], 1);
}
TEST_F(History, ReconciliationCannotRestoreArchivedAttachmentsFromCompletedJournals) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"shot", "computer.screenshot", Json::object()}};
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "shot";
  result.message_id = "stable";
  result.text = R"({"content":"done","is_error":false,"metadata":{}})";
  result.images = {persist_image(config, files, {"pixels", "image/png", 1, 1}, "high")};
  session.messages = {assistant, result};
  state.save(session);
  state.tool_started(session.id, assistant.calls[0]);
  state.tool_finished(session.id, assistant.calls[0], Json(result));
  archive_image_history(artifact_directory(config), files, session.id, session.messages);
  session.messages[1].images.clear();
  session.messages[1].images_archived = true;
  state.save(session);
  auto resumed = state.load_id(session.id);
  state.reconcile(resumed);
  EXPECT_TRUE(resumed.messages[1].images.empty());
  EXPECT_TRUE(resumed.messages[1].images_archived);
}
TEST_F(History, ExcerptWorkIsLinearAndAllEscapingDepthsStayWithinBudget) {
  const std::string text = "雪🌲\n\t\"\\" + std::string(1000000, '\\');
  for (unsigned depth = 1; depth <= 3; ++depth) {
    ContextMetrics metrics;
    auto page = context_excerpt(text, 4096, depth, &metrics);
    std::string encoded = page.text;
    for (unsigned i = 0; i < depth; ++i) {
      encoded = Json(encoded).dump();
      encoded = encoded.substr(1, encoded.size() - 2);
    }
    EXPECT_LE(encoded.size(), 4096u);
    EXPECT_EQ(metrics.materializations, 1u);
    EXPECT_EQ(metrics.scanned_bytes, 2 * text.size());
    EXPECT_GT(page.omitted, 900000u);
  }
}
TEST_F(History, InvalidSecondaryDisplaysDoNotBlockExplicitOrAutomaticValidSelection) {
  const std::vector<CaptureRect> displays{{0, 0, 0, 0}, {0, 0, 100, 100}};
  EXPECT_EQ(capture_area(displays, {{"display_id", 2}}).display, 1u);
  EXPECT_THROW(capture_area(displays, {{"display_id", 1}}), Error);
  const auto area = capture_area(displays, {{"x", 5}, {"y", 5}, {"width", 20}, {"height", 20}});
  EXPECT_EQ(area.display, 1u);
  EXPECT_FALSE(area.warnings.empty());
}

TEST_F(History, SmallBudgetCompactionProtectsTheNewestBatchAndFollowingFeedback) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message old;
  old.role = "assistant";
  old.text = std::string(20000, 'x');
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"finished", "clock.curr_time", Json::object()}};
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "finished";
  result.text = Json({{"content", std::string(30000, 'y')},
                      {"is_error", false},
                      {"metadata", {{"action_completed", true}}}})
                    .dump();
  result.images = {persist_image(config, files, {"pixels", "image/png", 1, 1}, "high")};
  codet::Message feedback;
  feedback.role = "user";
  feedback.origin = "hook";
  feedback.text = "verify this completed observation";
  session.messages = {old, old, assistant, result, feedback};
  state.save(session);
  Tools t(config, files, processes, http, clock);
  config.context_tokens = codet::test::context_tokens(t, config, 10000);
  Scripted provider;
  size_t requests = 0;
  provider.script = [&](const Config &c, const std::string &, const auto &messages,
                        const Json &schemas) {
    ++requests;
    EXPECT_EQ(c.max_output_tokens, 8192u);
    if (schemas.empty())
      return text_result("Older history summarized.");
    bool call = false, outcome = false, hook = false;
    for (const auto &m : messages) {
      if (m.role != "tool")
        EXPECT_TRUE(m.images.empty());
      if (!m.calls.empty()) {
        EXPECT_EQ(m.calls[0].id, "finished");
        call = true;
      }
      if (m.role == "tool") {
        EXPECT_EQ(m.tool_call_id, "finished");
        EXPECT_FALSE(m.images_archived);
        EXPECT_EQ(m.images.size(), 1u);
        outcome = true;
      }
      if (m.origin == "hook") {
        EXPECT_EQ(m.text, feedback.text);
        hook = true;
      }
    }
    EXPECT_TRUE(call && outcome && hook);
    EXPECT_EQ(messages.back().text, "latest human verbatim");
    return text_result("done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "latest human verbatim", cancel).text, "done");
  EXPECT_EQ(requests, 2u);
  auto restored = state.load_id(session.id);
  state.reconcile(restored);
  for (const auto &m : restored.messages)
    if (m.role == "tool")
      EXPECT_EQ(m.images.size(), 1u);
  EXPECT_EQ(image_history_position(artifact_directory(config), files, session.id)["observations"],
            1);
}
TEST_F(History, ImpossibleToolArgumentsFailBeforeDispatchAndRemainNeverStarted) {
  config.permission_mode = "bypass";
  StateStore state(config.home);
  auto session = state.create();
  Tools t(config, files, processes, http, clock);
  config.context_tokens = codet::test::context_tokens(t, config, 1300);
  Scripted provider;
  provider.script = [&](const Config &, const std::string &, const auto &, const Json &) {
    auto result = text_result("");
    result.message.calls = {
        {"effect", "exec_command", {{"cmd", "touch never-run; #" + std::string(6000, 'x')}}}};
    return result;
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_THAT([&] { agent.run(session, "begin", cancel); },
              ThrowsMessage<Error>(HasSubstr("before dispatch")));
  EXPECT_FALSE(fs::exists(config.cwd / "never-run"));
  const auto restored = state.load_id(session.id);
  EXPECT_EQ(Json::parse(restored.messages.back().text)["metadata"]["recovery"], "not_started");
}
TEST_F(History, FailedCompactionPublicationPreservesHistoryAndRetryDoesNotDuplicateObservations) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message old;
  old.role = "assistant";
  old.text = std::string(15000, 'x');
  old.images = {persist_image(config, files, {"pixels", "image/png", 1, 1}, "high")};
  session.messages = {old, old};
  state.save(session);
  Tools t(config, files, processes, http, clock);
  config.context_tokens = codet::test::context_tokens(t, config, 7000);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db,
                         "CREATE TRIGGER fail_compact BEFORE UPDATE OF messages ON sessions WHEN "
                         "instr(NEW.messages,'compaction')>0 BEGIN SELECT RAISE(FAIL,'injected "
                         "compact persistence'); END;",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  Scripted provider;
  provider.script = [&](const Config &, const std::string &, const auto &, const Json &schemas) {
    return text_result(schemas.empty() ? "summary" : "done");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_THAT([&] { agent.run(session, "continue", cancel); },
              ThrowsMessage<Error>(HasSubstr("injected compact persistence")));
  EXPECT_EQ(state.load_id(session.id).messages.front().text, old.text);
  EXPECT_EQ(image_history_position(artifact_directory(config), files, session.id)["observations"],
            2);
  ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER fail_compact", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_EQ(agent.run(session, "continue again", cancel).text, "done");
  EXPECT_EQ(image_history_position(artifact_directory(config), files, session.id)["observations"],
            2);
}
TEST_F(History, FailureHookCarriesCanonicalIdentityAndPublicOperation) {
  config.hooks["PostToolUseFailure"] =
      Json::array({{{"matcher", "Bash"}, {"command", "cat > failure.json"}}});
  Tools t(config, files, processes, http, clock);
  EXPECT_TRUE(t.execute({"bad", "exec_command", {{"cmd", ""}}}, cancel).error);
  const auto input = Json::parse(files.native.read(config.cwd / "failure.json"));
  EXPECT_EQ(input["tool"], "exec_command");
  EXPECT_EQ(input["tool_name"], "Bash");
}

TEST_F(History, PrivateDirectoriesKeepTheirChildrenInPlanMode) {
  config.permission_mode = "plan";
  put(config.home / "memory/note.md", "memory");
  put(config.home / "skills/demo/SKILL.md", "skill");
  put(config.home / "skills/demo/support.txt", "support");
  Tools t(config, files, processes, http, clock);
  t.instructions();
  EXPECT_THAT(read(t, config.home / "memory").content, HasSubstr("note.md"));
  EXPECT_THAT(read(t, config.home / "skills/demo").content, HasSubstr("support.txt"));
}
TEST_F(History, HooksCannotReplaceHomeOrAdvertisedSkillAncestors) {
  const auto foreign = temp.root / "foreign";
  put(config.home / "memory/note.md", "allowed");
  put(foreign / "memory/note.md", "foreign secret");
  config.hooks["PreToolUse"] =
      Json::array({{{"matcher", "read_file"},
                    {"command", "mv '" + config.home.string() + "' '" + config.home.string() +
                                    ".old'; ln -s '" + foreign.string() + "' '" +
                                    config.home.string() + "'"}}});
  Tools t(config, files, processes, http, clock);
  EXPECT_TRUE(read(t, config.home / "memory/note.md").error);
  EXPECT_EQ(files.bytes, 0u);
  fs::remove(config.home);
  fs::rename(config.home.string() + ".old", config.home);
  const auto skills = config.cwd / ".agents/skills";
  put(skills / "demo/SKILL.md", "advertised");
  put(skills / "demo/secret.txt", "allowed");
  put(foreign / "demo/secret.txt", "foreign secret");
  t.instructions();
  config.hooks["PreToolUse"][0]["command"] = "mv '" + skills.string() + "' '" + skills.string() +
                                             ".old'; ln -s '" + foreign.string() + "' '" +
                                             skills.string() + "'";
  EXPECT_TRUE(read(t, skills / "demo/secret.txt").error);
  EXPECT_EQ(files.bytes, 0u);
}
TEST_F(History, GeneratedLookingNamesDoNotShadowExistingWorkspaceFiles) {
  config.permission_mode = "plan";
  struct Images : ImageBackend {
    std::string decoded;
    bool available() const override {
      return true;
    }
    ImageData load(const fs::path &, const std::string &) override {
      throw Error("Path reopen is forbidden");
    }
    ImageData load_bytes(const std::string &data, const std::string &) override {
      decoded = data;
      return {"rendered", "image/png", 1, 1};
    }
  };
  auto images = std::make_shared<Images>();
  Tools t(config, files, processes, http, clock, {}, {}, {}, {}, {}, images);
  const auto name = std::string(64, 'a') + ".png";
  put(config.cwd / name, "workspace pixels");
  for (const auto &spelling : {name, "./" + name, (config.cwd / name).string()}) {
    const auto result = t.execute({unique_id(), "view_image", {{"path", spelling}}}, cancel);
    EXPECT_FALSE(result.error) << result.content;
    EXPECT_EQ(images->decoded, "workspace pixels");
  }
  codet::Message m;
  m.role = "tool";
  m.text = R"({"content":"retained","is_error":false,"metadata":{}})";
  const fs::path archive = t.retain_result(m);
  put(config.cwd / archive.filename(), "workspace text");
  for (const auto &spelling : {archive.filename().string(), "./" + archive.filename().string(),
                               (config.cwd / archive.filename()).string()})
    EXPECT_THAT(read(t, spelling).content, HasSubstr("workspace text"));
}

TEST_F(History, AllFiveAdaptersPreserveProjectionReferencesAndPagingControls) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"read", "read_file", {{"path", "note.md"}}}};
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "read";
  result.text = Json({{"content", "visible page"},
                      {"is_error", false},
                      {"metadata",
                       {{"next_byte_offset", 12345},
                        {"result_path", "private-reference.txt"},
                        {"projection_truncated", true},
                        {"scan_limited", true},
                        {"action_completed", true}}}})
                    .dump();
  for (const auto *name : {"codex", "openai", "anthropic", "google", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    const auto body = provider
                          .request(config, "composed instructions", {assistant, result},
                                   Json::array(), {{"access_token", "fixture"}})
                          .body;
    EXPECT_THAT(body, HasSubstr("next_byte_offset")) << name;
    EXPECT_THAT(body, HasSubstr("12345")) << name;
    EXPECT_THAT(body, HasSubstr("private-reference.txt")) << name;
    EXPECT_THAT(body, HasSubstr("action_completed")) << name;
    EXPECT_THAT(body, HasSubstr("scan_limited")) << name;
  }
}

TEST_F(History, CompleteResultArchivesExceedRawCaptureCapsAndRebindWithoutFollowingStalePaths) {
  config.tool_output_limit = 16 * 1024 * 1024;
  Tools t(config, files, processes, http, clock);
  t.use_session("active");
  codet::Message m;
  m.role = "tool";
  m.tool_call_id = "large";
  m.text = std::string(9 * 1024 * 1024, 'x');
  const fs::path archive = t.retain_result(m);
  EXPECT_GT(fs::file_size(archive), 8u * 1024 * 1024);
  EXPECT_EQ(Json::parse(files.native.read(archive))["text"], m.text);
  EXPECT_FALSE(fs::path(m.result_file).is_absolute());
  EXPECT_FALSE(fs::exists(config.home / "tool-output"));
  auto page = read(t, archive, {{"byte_offset", 8 * 1024 * 1024}, {"byte_limit", 30}});
  EXPECT_FALSE(page.error);
  EXPECT_EQ(page.metadata["next_byte_offset"], 8 * 1024 * 1024 + 30);
  const auto moved = temp.root / "new-artifacts";
  fs::copy(artifact_directory(config), moved, fs::copy_options::recursive);
  config.artifact_root = moved;
  Tools relocated(config, files, processes, http, clock);
  relocated.use_session("active");
  relocated.reference_results({m});
  EXPECT_FALSE(read(relocated, m.result_file, {{"byte_limit", 30}}).error);
  EXPECT_TRUE(read(relocated, archive).error);
  config.set_deny({"read_file(*results*)"});
  EXPECT_TRUE(read(relocated, m.result_file).error);
}
TEST_F(History, FailedResultArchivePersistenceDoesNotReplayCompletedEffects) {
  config.permission_mode = "bypass";
  put(config.cwd / "source", std::string(45000, 'x'));
  StateStore state(config.home);
  auto session = state.create();
  Tools t(config, files, processes, http, clock);
  config.context_tokens = codet::test::context_tokens(t, config, 6000);
  Scripted provider;
  size_t requests = 0;
  provider.script = [&](const Config &, const std::string &, const auto &, const Json &) {
    if (++requests == 1) {
      auto out = text_result("");
      out.message.calls = {
          {"once", "exec_command", {{"cmd", "printf x >> effect; cat source"}, {"login", false}}}};
      return out;
    }
    return text_result("done");
  };
  Agent agent(config, provider, t, state, {});
  files.fail_results = true;
  EXPECT_THAT([&] { agent.run(session, "read once", cancel); },
              ThrowsMessage<Error>(HasSubstr("result archive persistence")));
  EXPECT_EQ(files.native.read(config.cwd / "effect"), "x");
  const auto saved = state.load_id(session.id);
  EXPECT_EQ(std::count_if(saved.messages.begin(), saved.messages.end(),
                          [](const auto &m) { return m.role == "tool"; }),
            1);
  files.fail_results = false;
  EXPECT_EQ(agent.run(session, "continue from the saved result", cancel).text, "done");
  EXPECT_EQ(files.native.read(config.cwd / "effect"), "x");
  EXPECT_GE(requests, 2u);
  EXPECT_LE(requests, 3u);
}

TEST_F(History, LegacyIndexesMigrateOnResumeAndForkWithoutNewAttachments) {
  const auto root = artifact_directory(config);
  auto artifact = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  artifact.path = (temp.root / "stale/artifacts" / fs::path(artifact.path).filename()).string();
  const auto legacy = Json(artifact).dump() + "\n";
  put(artifact_index_path(root, "resumed"), legacy);
  Tools t(config, files, processes, http, clock);
  t.use_session("resumed");
  EXPECT_THAT(files.native.read(artifact_index_path(root, "resumed")),
              HasSubstr("\"chronology\":\"unknown\""));
  EXPECT_THAT(files.native.read(artifact_index_path(root, "resumed")),
              Not(HasSubstr("stale/artifacts")));
  EXPECT_EQ(image_history_position(root, files, "resumed")["legacy_unknown"], 1);
  StateStore state(config.home);
  auto parent = state.create();
  codet::Message summary;
  summary.role = "system";
  summary.text = "legacy summary";
  summary.artifact_index = artifact_index_reference(parent.id);
  parent.messages = {summary};
  put(artifact_index_path(root, parent.id), legacy);
  state.save(parent);
  const auto child = state.fork(parent);
  EXPECT_EQ(files.native.read(artifact_index_path(root, parent.id)), legacy);
  EXPECT_THAT(files.native.read(artifact_index_path(root, child.id)),
              HasSubstr("\"chronology\":\"unknown\""));
  EXPECT_THAT(files.native.read(artifact_index_path(root, child.id)),
              Not(HasSubstr("stale/artifacts")));
  EXPECT_EQ(image_history_position(root, files, child.id)["legacy_unknown"], 1);
}
} // namespace

TEST_F(History, JsonBytePagesRemainVerbatimThroughSaveForkAndProviderFormatting) {
  config.permission_mode = "plan";
  const std::string source = "{\n  \"name\": \"example\",\n  \"version\": \"1\"\n}\n";
  put(config.cwd / "package.json", source);
  Tools t(config, files, processes, http, clock);
  auto page = read(t, config.cwd / "package.json", {{"byte_offset", 0}});
  ASSERT_FALSE(page.error) << page.content;
  EXPECT_EQ(page.content, source);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message call;
  call.role = "assistant";
  call.calls = {{"json", "read_file", {{"path", "package.json"}, {"byte_offset", 0}}}};
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = "json";
  result.text =
      Json({{"content", page.content}, {"is_error", false}, {"metadata", page.metadata}}).dump();
  session.messages = {call, result};
  state.save(session);
  const auto child = state.fork(state.load_id(session.id));
  EXPECT_EQ(Json::parse(child.messages.back().text)["content"], source);
  EXPECT_EQ(Json::parse(child.messages.back().text)["metadata"]["next_byte_offset"], source.size());
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  std::function<bool(const Json &)> contains_page = [&](const Json &value) {
    if (value.is_string())
      return value.get<std::string>().find(source + "\n<codet_result>") != std::string::npos;
    if (value.is_structured())
      for (const auto &item : value)
        if (contains_page(item))
          return true;
    return false;
  };
  for (const auto *name : {"codex", "openai", "anthropic", "google", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    const auto body = Json::parse(
        provider
            .request(config, "system", child.messages, Json::array(), {{"access_token", "fixture"}})
            .body);
    EXPECT_TRUE(contains_page(body)) << name;
  }
}
TEST_F(History, DirectoryPagesRespectContextBudgetAndResumeInSortedOrder) {
  config.permission_mode = "plan";
  const auto dir = config.cwd / "listing";
  fs::create_directories(dir);
  for (int i = 0; i < 200; ++i)
    put(dir / (std::to_string(1000 + i) + std::string(60, 'x')), "");
  Tools t(config, files, processes, http, clock);
  t.inspection_budget(2000);
  size_t offset = 1, total = 0;
  std::string previous;
  do {
    const auto page = read(t, dir, {{"offset", offset}, {"limit", 25}});
    ASSERT_FALSE(page.error) << page.content;
    EXPECT_LT(page.content.size(), 2000u);
    codet::Message encoded;
    encoded.role = "tool";
    encoded.message_id = "00000000-0000-0000-0000-000000000000";
    encoded.tool_call_id = encoded.message_id;
    encoded.previewed = true;
    encoded.text =
        Json({{"content", page.content}, {"is_error", page.error}, {"metadata", page.metadata}})
            .dump();
    EXPECT_LE(Json(encoded).dump().size(), 2000u);
    EXPECT_EQ(page.metadata["scan_limited"], false);
    const auto entries = Json::parse(page.content)["entries"];
    ASSERT_FALSE(entries.empty());
    for (const auto &entry : entries) {
      const auto name = entry["name"].get<std::string>();
      EXPECT_GT(name, previous);
      previous = name;
      ++total;
    }
    offset = page.metadata["next_entry_offset"].get<size_t>();
    if (page.metadata["eof"] == true)
      break;
  } while (total < 250);
  EXPECT_EQ(total, 200u);
}
TEST_F(History, DirectoryScanLimitHasFinitePartialPagesAndFilteredEof) {
  config.permission_mode = "plan";
  const auto dir = config.cwd / "bounded-listing";
  fs::create_directories(dir);
  for (size_t i = 0; i < 9999; ++i)
    std::ofstream(dir / ("entry-" + std::to_string(i)));
  Tools tools(config, files, processes, http, clock);
  tools.inspection_budget(2000);
  for (size_t size = 9999; size <= 10001; ++size) {
    SCOPED_TRACE(size);
    if (size != 9999)
      std::ofstream(dir / ("entry-" + std::to_string(size - 1)));
    const auto first = read(tools, dir, {{"limit", 2}});
    ASSERT_FALSE(first.error) << first.content;
    EXPECT_EQ(first.metadata["next_entry_offset"], 3);
    EXPECT_EQ(first.metadata["eof"], false);
    EXPECT_EQ(first.metadata["scan_limited"], size > 10000);
    const auto tail = read(tools, dir, {{"offset", 9999}});
    ASSERT_FALSE(tail.error) << tail.content;
    EXPECT_EQ(Json::parse(tail.content)["entries"].size(), size == 9999 ? 1u : 2u);
    EXPECT_EQ(tail.metadata["eof"], true);
    EXPECT_EQ(tail.metadata["truncated"], size > 10000);
    EXPECT_EQ(tail.metadata["scan_limited"], size > 10000);
    const auto end = read(tools, dir, {{"offset", tail.metadata["next_entry_offset"]}});
    EXPECT_FALSE(end.error) << end.content;
    EXPECT_TRUE(Json::parse(end.content)["entries"].empty());
    EXPECT_EQ(end.metadata["eof"], true);
    const auto past = read(tools, dir, {{"offset", 20000}});
    EXPECT_EQ(past.metadata["eof"], true);
    EXPECT_EQ(past.metadata["scan_limited"], size > 10000);
  }
  config.set_deny({"read_file(*entry-*)"});
  const auto empty = read(tools, dir);
  EXPECT_FALSE(empty.error) << empty.content;
  EXPECT_TRUE(Json::parse(empty.content)["entries"].empty());
  EXPECT_EQ(empty.metadata["eof"], true);
  EXPECT_EQ(empty.metadata["truncated"], true);
  EXPECT_EQ(empty.metadata["scan_limited"], true);
}
TEST_F(History, DirectoryScanMetadataSurvivesBoundedProjection) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("scan-projection");
  codet::Message call, result;
  call.role = "assistant";
  call.calls = {{"listing", "read_file", {{"path", "."}}}};
  result.role = "tool";
  result.tool_call_id = "listing";
  result.text = Json({{"content", std::string(30000, 'x')},
                      {"is_error", false},
                      {"metadata",
                       {{"scan_limited", true},
                        {"eof", true},
                        {"truncated", true},
                        {"next_entry_offset", 10001}}}})
                    .dump();
  std::vector<codet::Message> messages{call, result};
  const auto projected = project_history(messages, 5000, tools);
  const auto metadata = Json::parse(projected.back().text)["metadata"];
  EXPECT_EQ(metadata["scan_limited"], true);
  EXPECT_FALSE(metadata.contains("eof"));
  EXPECT_FALSE(metadata.contains("next_entry_offset"));
  EXPECT_EQ(metadata["inspect_archived_page"], true);
  EXPECT_LE(Json(projected).dump().size(), 5000u);
}
TEST_F(History, OptionalCanonicalizationFailureKeepsOtherInstructionAndSkillContext) {
  put(config.cwd / "AGENTS.md", "retained instructions");
  const auto bad = config.cwd / ".agents/skills";
  put(bad / "bad/SKILL.md", "---\nname: bad\ndescription: omitted-skill\n---");
  put(config.home / "skills/good/SKILL.md", "---\nname: good\ndescription: retained-skill\n---");
  files.unavailable = files.native.canonical(bad);
  std::vector<std::string> warnings;
  Tools tools(config, files, processes, http, clock, {}, {},
              [&](const auto &s) { warnings.push_back(s); });
  const auto text = tools.instructions();
  EXPECT_THAT(text, HasSubstr("retained instructions"));
  EXPECT_THAT(text, HasSubstr("retained-skill"));
  EXPECT_THAT(text, Not(HasSubstr("omitted-skill")));
  EXPECT_THAT(warnings, Contains(HasSubstr("Discovery path unavailable")));
}
TEST_F(History, PastEndLineCountsAndCaseAndHardLinkInspectionUseActualFiles) {
  config.permission_mode = "plan";
  put(config.cwd / "CaseFile", "a\nb");
  Tools t(config, files, processes, http, clock);
  EXPECT_EQ(read(t, config.cwd / "CaseFile", {{"offset", 5}}).metadata["total_lines"], 2);
  fs::create_hard_link(config.cwd / "CaseFile", config.cwd / "hard-link");
  const auto link = config.cwd / "hard-link";
  const bool matches = files.native.open_read(link)->path() == files.native.canonical(link);
  const auto result = read(t, link);
  EXPECT_EQ(result.error, !matches) << result.content;
  if (!matches)
    EXPECT_THAT(result.content, HasSubstr("Inspection target changed"));
  if (fs::exists(config.cwd / "casefile"))
    EXPECT_FALSE(read(t, config.cwd / "casefile").error);
}
TEST_F(History, PlanPreviewStopsOnceAndStaysInsideEightKiB) {
  Tools t(config, files, processes, http, clock);
  t.use_session("bounded-plan");
  Json plan = Json::array();
  for (size_t i = 0; i < 100; ++i)
    plan.push_back({{"step", std::string(70, 'x')},
                    {"status", i == 0   ? "in_progress"
                               : i < 98 ? "pending"
                                        : "completed"}});
  ASSERT_FALSE(t.execute({"plan", "update_plan", {{"plan", plan}}}, cancel).error);
  const auto view = t.dynamic_context();
  EXPECT_LE(view.size(), 8192u);
  const auto notice = view.find("Additional steps omitted");
  ASSERT_NE(notice, std::string::npos);
  EXPECT_EQ(view.find("Additional steps omitted", notice + 1), std::string::npos);
  EXPECT_THAT(view.substr(notice), Not(HasSubstr("[completed]")));
}
TEST_F(History, StablePreviewsPreserveRawJournalAndCaptureReferencesAcrossCompactionAndFork) {
  StateStore state(config.home);
  auto session = state.create();
  Tools t(config, files, processes, http, clock);
  t.use_session(session.id);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.message_id = unique_id();
  assistant.calls = {{"output", "exec_command", {{"cmd", "test"}}}};
  const std::string capture = "01234567-1234-1234-1234-0123456789ab.txt";
  put(config.home / "tool-output" / capture, "complete raw capture");
  codet::Message result;
  result.role = "tool";
  result.message_id = unique_id();
  result.tool_call_id = "output";
  result.text = Json({{"content", std::string(30000, 'x')},
                      {"is_error", true},
                      {"metadata",
                       {{"spill_path", capture},
                        {"capture_error", "partial capture"},
                        {"post_action_error", "hook failed"},
                        {"action_completed", true}}}})
                    .dump();
  session.messages = {assistant, result};
  state.save(session);
  state.tool_started(session.id, assistant.calls.front());
  state.tool_finished(session.id, assistant.calls.front(), Json(result));
  auto preview = project_history(session.messages, 5000, t);
  const auto stable = session.messages.back().text;
  const auto meta = Json::parse(stable)["metadata"];
  EXPECT_EQ(meta["spill_path"], capture);
  EXPECT_EQ(meta["capture_error"], "partial capture");
  EXPECT_EQ(meta["post_action_error"], "hook failed");
  codet::Message extra;
  extra.role = "user";
  extra.text = "additional user input";
  extra.message_id = unique_id();
  session.messages.push_back(extra);
  EXPECT_EQ(project_history(session.messages, 10000, t)[1].text, stable);
  state.save(session);
  auto loaded = state.load_id(session.id);
  state.reconcile(loaded);
  EXPECT_EQ(loaded.messages[1].text, stable);
  state.remember_references(session.id, loaded.messages);
  loaded.messages = {extra}; // Simulate retirement of tool messages after a committed summary.
  state.save(loaded);
  const auto child = state.fork(loaded);
  Tools resumed(config, files, processes, http, clock);
  resumed.use_session(child.id);
  resumed.restore_references(state.references(child.id));
  const auto reference = resumed.execute({"raw", "read_file", {{"path", capture}}}, cancel);
  EXPECT_FALSE(reference.error) << reference.content;
  EXPECT_THAT(reference.content, HasSubstr("complete raw capture"));
  config.set_deny({"read_file(*tool-output*)"});
  EXPECT_TRUE(resumed.execute({"denied", "read_file", {{"path", capture}}}, cancel).error);
  config.set_deny({});
  const auto old_root = artifact_directory(config);
  const auto moved = temp.root / "relocated-artifacts";
  fs::copy(old_root, moved, fs::copy_options::recursive);
  const auto moved_captures = temp.root / "relocated-captures";
  fs::copy(config.home / "tool-output", moved_captures, fs::copy_options::recursive);
  config.artifact_root = moved;
  config.capture_root = moved_captures;
  StateStore relocated(config.home, moved);
  Tools moved_tools(config, files, processes, http, clock);
  moved_tools.use_session(child.id);
  moved_tools.restore_references(relocated.references(child.id));
  EXPECT_FALSE(read(moved_tools, capture).error);
  EXPECT_FALSE(read(moved_tools, preview[1].result_file).error);
  EXPECT_TRUE(read(moved_tools, old_root / preview[1].result_file).error);
}
TEST_F(History, OversizedCompletedPatchCanCheckpointAfterSmallerContextResume) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message user;
  user.role = "user";
  user.origin = "human";
  user.text = "preserve this steering";
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"large", "apply_patch", {{"patch", std::string(130000, 'x')}}}};
  codet::Message done;
  done.role = "tool";
  done.tool_call_id = "large";
  done.text = Json({{"content", "completed"},
                    {"is_error", false},
                    {"metadata", {{"action_completed", true}, {"recovery", "uncertain"}}}})
                  .dump();
  session.messages = {user, assistant, done};
  state.save(session);
  Tools t(config, files, processes, http, clock);
  config.context_tokens = codet::test::context_tokens(t, config, 12000);
  Scripted provider;
  size_t summaries = 0, normal = 0;
  provider.script = [&](const Config &, const auto &, const auto &messages, const Json &schemas) {
    if (schemas.empty()) {
      ++summaries;
      return text_result("Large patch was completed; inspect uncertainty before further action.");
    }
    ++normal;
    EXPECT_THAT(Json(messages).dump(), HasSubstr("continue"));
    EXPECT_THAT(Json(messages).dump(), HasSubstr("uncertain"));
    for (const auto &m : messages)
      EXPECT_TRUE(m.calls.empty());
    return text_result("continued");
  };
  Agent agent(config, provider, t, state, {});
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "continued");
  EXPECT_EQ(summaries, 1u);
  EXPECT_EQ(normal, 1u);
  EXPECT_FALSE(state.references(session.id).empty());
}
TEST_F(History, AdvisorySummaryTargetUsesActualSizeAndRetriesOnlyHardOverflow) {
  for (int mode = 0; mode < 3; ++mode) {
    StateStore state(config.home);
    auto session = state.create();
    codet::Message old;
    old.role = "assistant";
    old.text = std::string(20000, 'x');
    codet::Message human;
    human.role = "user";
    human.origin = "human";
    human.text = "latest";
    session.messages = {old, human};
    state.save(session);
    Tools t(config, files, processes, http, clock);
    config.context_tokens = codet::test::context_tokens(t, config, 5000);
    Scripted provider;
    size_t calls = 0;
    provider.script = [&](const Config &, const auto &, const auto &, const Json &) {
      ++calls;
      auto out = text_result(mode == 0 || (mode == 1 && calls == 2) ? std::string(800, 'a')
                                                                    : std::string(9000, 'x'));
      out.usage.input = 11;
      return out;
    };
    Agent agent(config, provider, t, state, {});
    if (mode < 2) {
      const auto result = agent.compact(session, cancel);
      EXPECT_EQ(result.usage.input, mode == 0 ? 11 : 22);
    } else {
      EXPECT_THROW(agent.compact(session, cancel), Error);
      EXPECT_EQ(state.load_id(session.id).messages.front().text, old.text);
    }
    EXPECT_EQ(calls, mode == 0 ? 1u : 2u);
  }
}
TEST_F(History, LargeNewPatchDispatchesOnceAndFourPlanListingsStayRecoverable) {
  for (bool listing : {false, true}) {
    config.permission_mode = listing ? "plan" : "bypass";
    Tools t(config, files, processes, http, clock);
    config.context_tokens = codet::test::context_tokens(t, config, 14000);
    StateStore state(config.home);
    auto session = state.create();
    if (listing)
      for (int i = 0; i < 200; ++i)
        put(config.cwd / "entries" / (std::to_string(1000 + i) + std::string(60, 'x')), "");
    Scripted provider;
    size_t normal = 0;
    provider.script = [&](const Config &, const auto &, const auto &, const Json &schemas) {
      if (schemas.empty())
        return text_result(
            "Completed the requested operation; inspect retained results as needed.");
      if (++normal > 1)
        return text_result("done");
      auto out = text_result("");
      if (listing)
        for (int i = 0; i < 4; ++i)
          out.message.calls.push_back(
              {"listing-" + std::to_string(i), "read_file", {{"path", "entries"}}});
      else
        out.message.calls = {{"patch",
                              "apply_patch",
                              {{"patch", "*** Begin Patch\n*** Add File: big\n+" +
                                             std::string(130000, 'x') + "\n*** End Patch\n"}}}};
      return out;
    };
    Agent agent(config, provider, t, state, {});
    EXPECT_EQ(agent.run(session, "perform requested work once", cancel).text, "done");
    EXPECT_EQ(normal, 2u);
    if (!listing)
      EXPECT_EQ(files.native.read(config.cwd / "big").size(), 130001u);
  }
}
TEST_F(History, SchemaFourBackfillsJournalReferencesEvenWhenTranscriptWasCompacted) {
  std::string id;
  const std::string capture = "01234567-1234-1234-1234-0123456789ab.txt";
  {
    StateStore state(config.home);
    auto session = state.create();
    id = session.id;
    ToolCall call{"old-output", "exec_command", {{"cmd", "test"}}};
    state.tool_started(id, call);
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = call.id;
    result.text =
        Json({{"content", "preview"}, {"is_error", false}, {"metadata", {{"spill_path", capture}}}})
            .dump();
    state.tool_finished(id, call, Json(result));
  }
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "DROP TABLE session_references; PRAGMA user_version=4;", nullptr,
                         nullptr, nullptr),
            SQLITE_OK);
  sqlite3_close(db);
  StateStore upgraded(config.home);
  EXPECT_THAT(upgraded.references(id), ElementsAre(capture));
  put(config.home / "tool-output" / capture, "legacy capture");
  Tools resumed(config, files, processes, http, clock);
  resumed.use_session(id);
  resumed.restore_references(upgraded.references(id));
  EXPECT_FALSE(read(resumed, capture).error);
  fs::remove(config.home / "tool-output" / capture);
  EXPECT_NO_THROW(resumed.restore_references(upgraded.references(id)));
  EXPECT_TRUE(read(resumed, capture).error);
}
TEST_F(History, SummaryRetryRunsHookOnceAndCancellationPreservesDurableHistory) {
  config.hooks["PreCompact"] = Json::array({{{"command", "printf x >> compact-count"}}});
  for (bool interrupted : {false, true}) {
    StateStore state(config.home);
    auto session = state.create();
    codet::Message old;
    old.role = "assistant";
    old.text = std::string(20000, 'x');
    codet::Message human;
    human.role = "user";
    human.origin = "human";
    human.text = "latest";
    session.messages = {old, human};
    state.save(session);
    const auto before = Json(state.load_id(session.id).messages);
    Tools t(config, files, processes, http, clock);
    config.context_tokens = codet::test::context_tokens(t, config, 5500);
    Scripted provider;
    size_t calls = 0;
    provider.script = [&](const Config &, const auto &, const auto &, const Json &) {
      if (interrupted) {
        cancel.requested = true;
        return text_result("cancelled summary");
      }
      return text_result(++calls == 1 ? std::string(9000, 'x') : "fits");
    };
    Agent agent(config, provider, t, state, {});
    if (interrupted) {
      EXPECT_THAT([&] { agent.compact(session, cancel); },
                  ThrowsMessage<Error>(HasSubstr("Interrupted")));
      EXPECT_EQ(Json(state.load_id(session.id).messages), before);
      cancel.requested = false;
    } else {
      EXPECT_NO_THROW(agent.compact(session, cancel));
      EXPECT_EQ(calls, 2u);
    }
  }
  EXPECT_EQ(files.native.read(config.cwd / "compact-count"), "xx");
}
TEST_F(History, ReferencePublicationRollsBackAndRetryDoesNotUseUncommittedCache) {
  StateStore state(config.home);
  auto session = state.create();
  codet::Message capture, result;
  capture.result_file = "01234567-1234-1234-1234-0123456789ab.txt";
  result.result_file = "results/" + session.id + "/archived.json";
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(db,
                   "CREATE TRIGGER ref_fail BEFORE INSERT ON session_references "
                   "WHEN NEW.kind='result' BEGIN SELECT RAISE(FAIL,'reference failure'); END;",
                   nullptr, nullptr, nullptr),
      SQLITE_OK);
  EXPECT_THROW(state.remember_references(session.id, {capture, result}), Error);
  EXPECT_TRUE(state.references(session.id).empty());
  ASSERT_EQ(sqlite3_exec(db, "DROP TRIGGER ref_fail", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_NO_THROW(state.remember_references(session.id, {capture, result}));
  EXPECT_EQ(state.references(session.id).size(), 2u);
}
TEST_F(History, SchemaFiveSkipsCorruptRowsWithoutChangingThemOrLosingValidReferences) {
  const std::string capture = "01234567-1234-1234-1234-0123456789ab.txt";
  std::string good;
  {
    StateStore state(config.home);
    auto session = state.create("good");
    good = session.id;
    codet::Message user;
    user.role = "user";
    user.text = "preserved";
    user.result_file = "results/valid/message.json";
    session.messages = {user};
    state.save(session);
    ToolCall call{"valid-journal", "exec_command", {{"cmd", "test"}}};
    state.tool_started(good, call);
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = call.id;
    result.text = Json({{"content", "preview"}, {"metadata", {{"spill_path", capture}}}}).dump();
    state.tool_finished(good, call, Json(result));
  }
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  const auto exec = [&](const char *sql) {
    EXPECT_EQ(sqlite3_exec(db, sql, nullptr, nullptr, nullptr), SQLITE_OK) << sqlite3_errmsg(db);
  };
  exec("DROP TABLE session_references; PRAGMA user_version=4;"
       "INSERT INTO sessions(id,name,updated,messages) "
       "VALUES('syntax','syntax',1,'[{'),('shape','shape',1,'{}'),('journal','journal',1,'[]');"
       "INSERT INTO tools "
       "VALUES('journal','syntax','read_file','{}','{','complete'),('journal','shape','read_file','"
       "{}','[]','complete');");
  const auto rows = [&] {
    sqlite3_stmt *statement = nullptr;
    EXPECT_EQ(
        sqlite3_prepare_v2(db,
                           "SELECT messages FROM sessions WHERE id IN ('syntax','shape') UNION ALL "
                           "SELECT result FROM tools WHERE session='journal' ORDER BY 1",
                           -1, &statement, nullptr),
        SQLITE_OK);
    std::vector<std::string> values;
    while (sqlite3_step(statement) == SQLITE_ROW)
      values.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
    sqlite3_finalize(statement);
    return values;
  };
  const auto before = rows();
  {
    StateStore upgraded(config.home);
    EXPECT_EQ(upgraded.load(good).messages.front().text, "preserved");
    EXPECT_THAT(upgraded.references(good),
                UnorderedElementsAre(capture, "results/valid/message.json"));
    EXPECT_THROW(upgraded.load("syntax"), Error);
    EXPECT_THROW(upgraded.load("shape"), Error);
  }
  EXPECT_EQ(rows(), before);
  sqlite3_close(db);
  StateStore reopened(config.home);
  EXPECT_EQ(reopened.list().size(), 4u);
  EXPECT_EQ(reopened.references(good).size(), 2u);
}
TEST_F(History, SchemaFiveDatabaseFailureRollsBackAndCanRetry) {
  { StateStore initial(config.home); }
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  // An orphan journal row injects a real foreign-key failure during reference publication.
  ASSERT_EQ(sqlite3_exec(db,
                         "DROP TABLE session_references; PRAGMA user_version=4;"
                         "INSERT INTO tools "
                         "VALUES('missing','orphan','read_file','{}','{\"role\":\"tool\",\"result_"
                         "file\":\"results/valid/message.json\"}','complete');",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  EXPECT_THROW(StateStore failed(config.home), Error);
  sqlite3_stmt *q = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &q, nullptr), SQLITE_OK);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(q, 0), 4);
  sqlite3_finalize(q);
  ASSERT_EQ(sqlite3_prepare_v2(db,
                               "SELECT count(*) FROM sqlite_master WHERE name='session_references'",
                               -1, &q, nullptr),
            SQLITE_OK);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(q, 0), 0);
  sqlite3_finalize(q);
  ASSERT_EQ(
      sqlite3_exec(db, "DELETE FROM tools WHERE session='missing'", nullptr, nullptr, nullptr),
      SQLITE_OK);
  sqlite3_close(db);
  EXPECT_NO_THROW(StateStore retry(config.home));
}

TEST_F(History, FreshImagesSurviveFirstRequestCompactionAndRetry) {
  config.provider = "openai";
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 20000);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message old;
  old.role = "assistant";
  old.text = std::string(18000, 'o');
  session.messages = {old};
  state.save(session);
  const auto image = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  Scripted provider;
  size_t summaries = 0, normal = 0;
  provider.script = [&](const auto &, const auto &, const auto &messages, const Json &schemas) {
    if (schemas.empty()) {
      ++summaries;
      return text_result("old work completed");
    }
    ++normal;
    size_t images = 0;
    for (const auto &m : messages)
      images += m.images.size();
    EXPECT_EQ(images, 1u);
    if (normal == 1)
      throw Error("fixture main request failure");
    return text_result("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_THROW(agent.run(session, "inspect image", cancel, Json(), true, {image}), Error);
  auto resumed = state.load_id(session.id);
  EXPECT_EQ(agent.run(resumed, "try again", cancel).text, "done");
  EXPECT_EQ(summaries, 1u);
  EXPECT_EQ(normal, 2u);
}
TEST_F(History, SoftSummaryFailureFallsBackButStopsAndHardOverflowDoNot) {
  config.provider = "openai";
  for (int mode = 0; mode < 5; ++mode) {
    Tools tools(config, files, processes, http, clock);
    config.context_tokens = test::context_tokens(tools, config, mode == 3 ? 10000 : 20000);
    StateStore state(config.home);
    auto session = state.create();
    codet::Message old;
    old.role = "assistant";
    old.text = std::string(18000, 'o');
    session.messages = {old};
    state.save(session);
    if (mode == 4)
      config.hooks["PreCompact"] = Json::array({{{"command", "printf '{\"continue\":false}'"}}});
    Scripted provider;
    size_t main = 0, summaries = 0;
    provider.script = [&](const auto &, const auto &, const auto &, const Json &schemas) {
      if (schemas.empty()) {
        ++summaries;
        if (mode == 1)
          return text_result("");
        throw Error("fixture summary failure", mode == 2 ? 130 : 1);
      }
      ++main;
      return text_result("done");
    };
    Agent agent(config, provider, tools, state, {});
    if (mode < 2) {
      EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
      EXPECT_EQ(main, 1u);
    } else {
      EXPECT_THROW(agent.run(session, "continue", cancel), Error);
      EXPECT_EQ(main, 0u);
    }
    EXPECT_EQ(state.load_id(session.id).messages.front().text, old.text);
    EXPECT_LE(summaries, 1u);
    config.hooks = Json::object();
  }
}
TEST_F(History, FreshFilePageUsesRetainedBudgetAndNeverSkipsItsMiddle) {
  config.permission_mode = "plan";
  config.provider = "openai";
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 20000);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message old;
  old.role = "assistant";
  old.text = std::string(9000, 'o');
  session.messages = {old};
  state.save(session);
  const std::string source = std::string(3900, 'a') + "MIDDLE_SENTINEL" + std::string(4100, 'z');
  put(config.cwd / "source", source);
  Scripted provider;
  size_t main = 0;
  provider.script = [&](const auto &, const auto &, const auto &messages, const Json &schemas) {
    if (schemas.empty())
      return text_result("old work done");
    if (++main == 1) {
      auto out = text_result("");
      out.message.calls = {{"page", "read_file", {{"path", "source"}, {"byte_offset", 0}}}};
      return out;
    }
    for (const auto &m : messages)
      if (m.role == "tool") {
        const Json value = Json::parse(m.text);
        const std::string page = value.at("content");
        EXPECT_EQ(page, source.substr(0, page.size()));
        EXPECT_EQ(value.at("metadata").at("next_byte_offset"), page.size());
        EXPECT_FALSE(value.at("metadata").value("projection_truncated", false));
        EXPECT_GT(page.size(), 4500u);
      }
    return text_result("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "read source", cancel).text, "done");
  const auto before = Json(state.load_id(session.id).messages[3]).dump();
  EXPECT_EQ(agent.run(session, "continue", cancel).text, "done");
  EXPECT_EQ(Json(state.load_id(session.id).messages[3]).dump(), before);
}
TEST_F(History, AssistantBudgetCountsWireRepresentationOnceAndDecoratorsForwardItems) {
  config.provider = "codex";
  codet::Message reply;
  reply.role = "assistant";
  reply.text = std::string(10000, 'x');
  reply.assistant_items = {{"answer", reply.text, "final_answer"}};
  reply.replay =
      Json::array({{{"type", "message"},
                    {"role", "assistant"},
                    {"content", Json::array({{{"type", "output_text"}, {"text", reply.text}}})}}});
  CredentialStore credentials(config.home, files, clock);
  ApiProvider api(http, credentials, clock);
  const Json wire = Json::parse(
      api.request(config, "", {reply}, Json::array(), {{"access_token", "fixture"}}).body);
  EXPECT_EQ(provider_history_bytes(config, {reply}), wire.at("input").dump().size());
  EXPECT_LT(provider_history_bytes(config, {reply}), 11000u);
  struct Emitting : Provider {
    ProviderResult complete(const Config &, const std::string &,
                            const std::vector<codet::Message> &, const Json &, const Cancellation &,
                            const ChunkSink &, const ItemSink &items) override {
      if (items)
        items({{"fixture", true}});
      return text_result("done");
    }
  } emitting;
  struct Decorator : Provider {
    Provider &inner;
    explicit Decorator(Provider &p) : inner(p) {}
    ProviderResult complete(const Config &c, const std::string &s,
                            const std::vector<codet::Message> &m, const Json &t,
                            const Cancellation &cancel, const ChunkSink &text,
                            const ItemSink &items) override {
      return inner.complete(c, s, m, t, cancel, text, items);
    }
  } decorator(emitting);
  size_t events = 0;
  EXPECT_EQ(
      decorator.complete(config, "", {}, Json::array(), cancel, {}, [&](const Json &) { ++events; })
          .message.text,
      "done");
  EXPECT_EQ(events, 1u);
}
TEST_F(History, CheckpointStateCoalescesRecoveryAndIgnoresHistoricalExecutionIds) {
  Tools tools(config, files, processes, http, clock);
  codet::Message old;
  old.role = "tool";
  old.tool_call_id = "poll";
  old.text = Json({{"content", ""}, {"metadata", {{"session_id", 42}}}}).dump();
  codet::Message uncertain = old;
  uncertain.tool_call_id = "uncertain";
  uncertain.text = Json({{"content", ""}, {"metadata", {{"recovery", "uncertain"}}}}).dump();
  const auto facts = tools.execution_state({old, old, uncertain, uncertain});
  ASSERT_EQ(facts.size(), 1u);
  EXPECT_EQ(facts[0].at("call_id"), "uncertain");
  codet::Message compact;
  compact.checkpoint_state = facts;
  EXPECT_EQ(tools.execution_state({compact}), facts);
  uncertain.text = Json({{"content", "done"}, {"metadata", {{"exit_code", 0}}}}).dump();
  EXPECT_TRUE(tools.execution_state({compact, uncertain}).empty());
}
TEST_F(History, ArchiveRetriesReusePublishedContentWithoutSkippingValidation) {
  Tools tools(config, files, processes, http, clock);
  tools.use_session("archive");
  codet::Message original;
  original.role = "assistant";
  original.text = "immutable original";
  original.message_id = unique_id();
  auto first = original;
  const auto path = tools.retain_result(first);
  files.fail_results = true;
  auto retry = original;
  EXPECT_EQ(tools.retain_result(retry), path);
  original.text = "changed content";
  EXPECT_THROW(tools.retain_result(original), Error);
}
TEST_F(History, ConcurrentSchemaUpgradesSerializeAndRecheckVersion) {
  fs::create_directories(config.home);
  sqlite3 *db = nullptr;
  ASSERT_EQ(sqlite3_open((config.home / "state.sqlite3").c_str(), &db), SQLITE_OK);
  ASSERT_EQ(
      sqlite3_exec(
          db,
          "CREATE TABLE sessions(id TEXT PRIMARY KEY,name TEXT NOT NULL,parent TEXT,"
          "updated INTEGER NOT NULL,messages TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'idle',"
          "provider TEXT NOT NULL DEFAULT '',model TEXT NOT NULL DEFAULT '');"
          "CREATE TABLE tools(session TEXT,call_id TEXT,name TEXT,args TEXT,result TEXT,status "
          "TEXT);"
          "PRAGMA user_version=3; BEGIN IMMEDIATE;",
          nullptr, nullptr, nullptr),
      SQLITE_OK);
  auto open = [&] {
    StateStore state(config.home);
    return state.list().size();
  };
  auto first = std::async(std::launch::async, open);
  auto second = std::async(std::launch::async, open);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  ASSERT_EQ(sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr), SQLITE_OK);
  sqlite3_close(db);
  EXPECT_EQ(first.get(), 0u);
  EXPECT_EQ(second.get(), 0u);
  EXPECT_NO_THROW(StateStore reopened(config.home));
}
TEST_F(History, LiveExecutionStateRetiresTheTerminalPoll) {
  config.permission_mode = "bypass";
  Tools tools(config, files, processes, http, clock);
  const auto running = tools.execute(
      {"start", "exec_command", {{"cmd", "sleep .6"}, {"login", false}, {"yield_time_ms", 250}}},
      cancel);
  ASSERT_FALSE(running.error);
  ASSERT_TRUE(running.metadata.contains("session_id"));
  const auto id = running.metadata.at("session_id");
  const auto active = tools.execution_state({});
  ASSERT_EQ(active.size(), 1u);
  EXPECT_EQ(active[0].at("session_id"), id);
  const auto finished = tools.execute({"finish", "write_stdin", {{"session_id", id}}}, cancel);
  EXPECT_FALSE(finished.error);
  EXPECT_EQ(finished.metadata.at("exit_code"), 0);
  EXPECT_TRUE(tools.execution_state({}).empty());
}
TEST_F(History, CheckpointArchivesOverflowStateAndPreservesUnseenToolImagesWithoutOrphanResults) {
  Tools tools(config, files, processes, http, clock);
  config.context_tokens = test::context_tokens(tools, config, 12000);
  StateStore state(config.home);
  auto session = state.create();
  codet::Message call;
  call.role = "assistant";
  call.calls = {{"huge", "apply_patch", {{"patch", std::string(130000, 'x')}}}};
  session.messages.push_back(call);
  for (int i = 0; i < 100; ++i) {
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = i ? "uncertain-" + std::to_string(i) : "huge";
    if (i)
      session.messages[0].calls.push_back(
          {result.tool_call_id, "apply_patch", {{"patch", "pending"}}});
    result.text = Json({{"content", "unknown completion"},
                        {"is_error", true},
                        {"metadata", {{"recovery", "uncertain"}}}})
                      .dump();
    if (!i)
      result.images = {persist_image(config, files, {"pixels", "image/png", 1, 1}, "high")};
    session.messages.push_back(result);
  }
  state.save(session);
  Scripted provider;
  size_t images = 0;
  provider.script = [&](const auto &, const auto &, const auto &messages, const Json &schemas) {
    if (schemas.empty())
      return text_result("Inspect uncertain effects before doing more work.");
    for (const auto &message : messages) {
      EXPECT_NE(message.role, "tool");
      EXPECT_TRUE(message.calls.empty());
      images += message.images.size();
    }
    return text_result("done");
  };
  Agent agent(config, provider, tools, state, {});
  EXPECT_EQ(agent.run(session, "continue carefully", cancel).text, "done");
  EXPECT_EQ(images, 1u);
  const auto saved = state.load_id(session.id);
  ASSERT_TRUE(saved.messages[0].checkpoint_state.is_object());
  EXPECT_EQ(tools.execution_state(saved.messages).size(), 100u);
  auto another = saved;
  codet::Message older;
  older.role = "assistant";
  older.text = "more completed work";
  another.messages.insert(another.messages.begin() + 1, older);
  state.save(another);
  EXPECT_EQ(agent.compact(another, cancel).text, "Conversation compacted.");
  EXPECT_EQ(tools.execution_state(another.messages).size(), 100u);
  const auto child = state.fork(another);
  const auto moved = temp.root / "moved-artifacts";
  fs::copy(artifact_directory(config), moved, fs::copy_options::recursive);
  config.artifact_root = moved;
  Tools relocated(config, files, processes, http, clock);
  relocated.use_session(child.id);
  relocated.restore_references(state.references(child.id));
  EXPECT_EQ(relocated.execution_state(child.messages).size(), 100u);
}
