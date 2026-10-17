#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/interaction.hpp"
#include "codet/prompt.hpp"
#include "prompt_support.hpp"
#include "temp.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sqlite3.h>
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
    return native.canonical(p);
  }
};
struct Scripted : Provider {
  std::function<ProviderResult(const Config &, const std::string &,
                               const std::vector<codet::Message> &, const Json &)>
      script;
  ProviderResult complete(const Config &c, const std::string &s,
                          const std::vector<codet::Message> &m, const Json &schemas,
                          const Cancellation &, const ChunkSink &) override {
    EXPECT_LE(Json(m).dump().size(), prompt_budget(c, s, schemas).available());
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
  EXPECT_LE(page.content.size(), config.tool_output_limit / 8);
  EXPECT_EQ(page.metadata["next_byte_offset"], 8 + page.content.size());
  EXPECT_LE(files.largest, 4096u);
  EXPECT_LT(files.bytes, 20000u);
  EXPECT_TRUE(read(t, memory, {{"byte_offset", 0}, {"offset", 1}}).error);
  EXPECT_TRUE(read(t, memory, {{"byte_offset", -1}}).error);
  EXPECT_TRUE(read(t, memory, {{"byte_limit", 0}}).error);
  EXPECT_FALSE(read(t, memory, {{"byte_offset", 3000000}}).error);
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
  EXPECT_EQ(requests, 2u);
}

TEST_F(History, LegacyIndexesMigrateOnResumeAndForkWithoutNewAttachments) {
  const auto root = artifact_directory(config);
  auto artifact = persist_image(config, files, {"pixels", "image/png", 1, 1}, "high");
  artifact.path = (temp.root / "stale/artifacts" / fs::path(artifact.path).filename()).string();
  const auto legacy = Json(artifact).dump() + "\n";
  put(artifact_index_path(root, "resumed"), legacy);
  Tools t(config, files, processes, http, clock); t.use_session("resumed");
  EXPECT_THAT(files.native.read(artifact_index_path(root, "resumed")), HasSubstr("\"chronology\":\"unknown\""));
  EXPECT_THAT(files.native.read(artifact_index_path(root, "resumed")), Not(HasSubstr("stale/artifacts")));
  EXPECT_EQ(image_history_position(root, files, "resumed")["legacy_unknown"], 1);
  StateStore state(config.home); auto parent = state.create();
  codet::Message summary; summary.role = "system"; summary.text = "legacy summary";
  summary.artifact_index = artifact_index_reference(parent.id); parent.messages = {summary};
  put(artifact_index_path(root, parent.id), legacy); state.save(parent);
  const auto child = state.fork(parent);
  EXPECT_EQ(files.native.read(artifact_index_path(root, parent.id)), legacy);
  EXPECT_THAT(files.native.read(artifact_index_path(root, child.id)), HasSubstr("\"chronology\":\"unknown\""));
  EXPECT_THAT(files.native.read(artifact_index_path(root, child.id)), Not(HasSubstr("stale/artifacts")));
  EXPECT_EQ(image_history_position(root, files, child.id)["legacy_unknown"], 1);
}
} // namespace
