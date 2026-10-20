#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/support.hpp"
#include "temp.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
MATCHER_P(TrustedReference, identifier, "an available owned reference") {
  return arg.identifier == identifier && arg.error.empty() && arg.identity.has_value();
}
struct RepairFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t reads = 0, opens = 0, resolutions = 0;
  mutable std::function<void()> on_canonical;
  mutable std::function<void()> on_read;
  std::function<void()> on_backup;
  std::function<void(const fs::path &)> on_write, before_write;
  mutable std::function<void(const fs::path &)> on_open;
  std::string read(const fs::path &p, size_t n) const override {
    if (p.string().find("/results/") != std::string::npos) {
      ++reads;
      if (on_read)
        on_read();
    }
    return native.read(p, n);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    if (before_write)
      before_write(p);
    native.atomic_write(p, s, secret);
    if (on_write)
      on_write(p);
    if (p.filename().string().find("-backup-") != std::string::npos && on_backup)
      on_backup();
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    ++resolutions;
    if (on_canonical)
      on_canonical();
    return native.canonical(p);
  }
  std::unique_ptr<ReadHandle> open_read(const fs::path &p) const override {
    ++opens;
    if (on_open)
      on_open(p);
    return native.open_read(p);
  }
};
struct RepairHttp : HttpTransport {
  std::string stream;
  bool empty_body = false;
  size_t requests = 0;
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &sink) override {
    ++requests;
    sink(stream);
    return {200, empty_body ? "" : stream};
  }
};
struct Pr18 : Test {
  TempDirectory temp;
  Config config;
  RepairFiles files;
  RepairHttp http;
  NativeProcesses processes;
  SystemClock clock;
  Cancellation cancel;
  void SetUp() override {
    config.home = temp.root / "home";
    config.cwd = temp.root / "work";
    config.state_root = temp.root / "state";
    config.roots = {config.cwd};
    config.approval = "never";
    config.provider = "openai";
    config.model = "fixture";
    config.providers["openai"].api_key = "fixture";
    config.providers["openai"].wire_api = "chat_completions";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
  }
  struct Db {
    sqlite3 *p = nullptr;
    explicit Db(const fs::path &path) {
      if (sqlite3_open(path.c_str(), &p) != SQLITE_OK)
        throw Error("Cannot open test database");
      exec("PRAGMA busy_timeout=1");
    }
    ~Db() {
      sqlite3_close(p);
    }
    void exec(const std::string &sql) {
      if (sqlite3_exec(p, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        throw Error(sqlite3_errmsg(p));
    }
    std::string value(const std::string &sql) {
      sqlite3_stmt *q = nullptr;
      if (sqlite3_prepare_v2(p, sql.c_str(), -1, &q, nullptr) != SQLITE_OK)
        throw Error(sqlite3_errmsg(p));
      std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> handle(q, sqlite3_finalize);
      if (sqlite3_step(q) != SQLITE_ROW)
        throw Error("Missing test row");
      const auto *text = sqlite3_column_text(q, 0);
      return text ? reinterpret_cast<const char *>(text) : "";
    }
  };
  codet::Message checkpoint(Tools &tools) {
    codet::Message record;
    record.role = "system";
    record.origin = "recovery";
    record.text = Json::array({{{"call_id", "old"}, {"recovery", "uncertain"}}}).dump();
    record.message_id = digest(record.text);
    tools.retain_result(record);
    codet::Message message;
    message.role = "system";
    message.text = "summary";
    message.checkpoint_state = {{"records", 1}, {"recovery_record", record.result_file}};
    return message;
  }
  size_t archives() const {
    size_t n = 0;
    const auto root = artifact_directory(config) / "results";
    if (fs::exists(root))
      for (const auto &entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
          ++n;
    return n;
  }
};
std::string chat_stream(Json function, std::string id = "call") {
  return "data: " +
         Json({{"choices",
                Json::array(
                    {{{"delta",
                       {{"tool_calls",
                         Json::array(
                             {{{"index", 0}, {"id", id}, {"function", std::move(function)}}})}}},
                      {"finish_reason", "tool_calls"}}})}})
             .dump() +
         "\n\ndata: [DONE]\n\n";
}
} // namespace
TEST_F(Pr18, CaptureReferencesAreValidatedAgainstTheConfiguredRootInEveryRepairMode) {
  config.capture_root = temp.root / "custom-captures";
  StateStore state(files, config.state_root);
  auto session = state.create();
  const auto capture = "capture-00000000-0000-0000-0000-000000000001.txt";
  codet::Message message;
  message.role = "assistant";
  message.text = "surviving capture preview";
  message.result_file = capture;
  session.messages = {message};
  state.save(session);
  Tools tools(config, files, processes, http, clock);
  const auto inspection = state.repair(session.id, false, false, false, tools);
  ASSERT_EQ(inspection["issues"].size(), 1u);
  const auto provider = state.repair(session.id, false, true, false, tools);
  EXPECT_EQ(provider["issues"], inspection["issues"]);
  auto report = state.repair(session.id, false, false, true, tools);
  EXPECT_EQ(report["issues"], inspection["issues"]);
  auto saved = state.load_id(session.id, false);
  EXPECT_TRUE(result_name(saved.messages[0].result_file));
  EXPECT_THAT(tools.execution_state(saved.messages).dump(), HasSubstr(capture));
  EXPECT_NO_THROW(state.repair(session.id, false, false, true, tools));
  const auto path = config.capture_root / capture;
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "available", true);
  EXPECT_THAT(tools.observe_result_reference(path.string()), TrustedReference(capture));
  EXPECT_THAT(tools.observe_result_reference(capture), TrustedReference(capture));
  fs::remove(path);
  fs::create_symlink(config.cwd, path);
  EXPECT_FALSE(tools.observe_result_reference(capture).error.empty());
}
TEST_F(Pr18, CaptureEvictionProducesDurablePartialHistoryAcrossForkAndCompaction) {
  StateStore state(files, config.state_root);
  Tools tools(config, files, processes, http, clock);
  auto session = state.create();
  codet::Message message;
  message.role = "assistant";
  message.text = "surviving observation";
  {
    OutputSpool capture(capture_directory(config), 8, 8, 1);
    capture.append("original");
    capture.close();
    message.result_file = capture.path().filename().string();
  }
  session.messages = {message};
  state.save(session);
  {
    OutputSpool evict(capture_directory(config), 8, 8, 1);
    evict.append("new");
    evict.close();
  }
  ASSERT_FALSE(fs::exists(capture_directory(config) / message.result_file));
  auto report = state.repair(session.id, false, false, true, tools);
  EXPECT_FALSE(report["issues"].empty());
  auto saved = state.load_id(session.id, false);
  auto child = state.fork(saved);
  EXPECT_THAT(tools.execution_state(child.messages).dump(), HasSubstr(message.result_file));
  codet::Message compacted;
  compacted.role = "system";
  compacted.text = "new summary";
  compacted.checkpoint_state = tools.execution_state(child.messages);
  child.messages = {compacted};
  state.replace_history(child);
  EXPECT_THAT(tools.execution_state(state.load_id(child.id, false).messages).dump(),
              HasSubstr(message.result_file));
}
TEST_F(Pr18, OwnedAvailableAndUnreadableReferencesKeepTheirExistingTrustPolicies) {
  Tools tools(config, files, processes, http, clock);
  const auto ref = fs::path("results") / "session" / "record.json";
  const auto path = artifact_directory(config) / ref;
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "data", true);
  ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
  EXPECT_THAT(tools.observe_result_reference(path.string()), TrustedReference(ref.string()));
  EXPECT_NO_THROW(checked_artifact_identity(files, artifact_directory(config), path,
                                            ArtifactPolicy::owned_regular));
  EXPECT_THROW(checked_artifact_identity(files, artifact_directory(config), path,
                                         ArtifactPolicy::private_owned_regular),
               Error);
  ASSERT_EQ(::chmod(path.c_str(), 0), 0);
  if (::geteuid() != 0)
    EXPECT_FALSE(tools.observe_result_reference(ref.string()).error.empty());
  ASSERT_EQ(::chmod(path.c_str(), 0600), 0);
  if (::geteuid() == 0) {
    ASSERT_EQ(::chown(path.c_str(), 12345, 12345), 0);
    EXPECT_FALSE(tools.observe_result_reference(ref.string()).error.empty());
    ASSERT_EQ(::chown(path.c_str(), 0, 0), 0);
  }
}
TEST_F(Pr18, RecoveryRepairPreservesUserJsonResultPathAndNormalizesOwnedToolMetadata) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  codet::Message assistant;
  assistant.role = "assistant";
  for (const auto &name : {"read_file", "exec_command"})
    assistant.calls.push_back({name, name, Json::object()});
  s.messages.push_back(assistant);
  for (const auto &call : assistant.calls) {
    codet::Message m;
    m.role = "tool";
    m.tool_call_id = call.id;
    const auto ref = fs::path("results") / s.id / (unique_id() + ".json");
    const auto path = artifact_directory(config) / ref;
    fs::create_directories(path.parent_path());
    files.atomic_write(path, "archive", true);
    m.result_file = path.string();
    m.text = Json({{"content", Json({{"result_path", "user-data"}, {"keep", 42}}).dump()},
                   {"metadata", {{"result_path", path.string()}}},
                   {"is_error", false}})
                 .dump();
    s.messages.push_back(m);
  }
  state.save(s);
  state.repair(s.id, false, false, true, tools);
  const auto saved = state.load_id(s.id, false);
  const auto user = Json::parse(Json::parse(saved.messages[1].text)["content"].get<std::string>());
  EXPECT_EQ(user["result_path"], "user-data");
  EXPECT_EQ(user["keep"], 42);
  const auto owned = Json::parse(saved.messages[2].text);
  EXPECT_EQ(owned["metadata"]["result_path"], saved.messages[2].result_file);
  EXPECT_EQ(Json::parse(owned["content"].get<std::string>())["result_path"],
            saved.messages[2].result_file);
}
TEST_F(Pr18, ArchiveReadsAndBackupsAllowUnrelatedCommitsWithoutRetry) {
  for (const bool during_backup : {false, true}) {
    SCOPED_TRACE(during_backup);
    files.reads = 0;
    config.state_root = temp.root / (during_backup ? "backup" : "read");
    StateStore state(files, config.state_root);
    auto s = state.create(), other = state.create();
    Tools tools(config, files, processes, http, clock);
    tools.use_session(s.id);
    s.messages = {checkpoint(tools)};
    state.save(s);
    Db writer(config.state_root / "state.sqlite3");
    size_t writes = 0, backups = 0;
    const auto commit = [&] {
      if (writes++ == 0)
        writer.exec("UPDATE sessions SET status='concurrent' WHERE id='" + other.id + "'");
    };
    files.on_backup = [&] {
      ++backups;
      if (during_backup)
        commit();
    };
    files.on_read = during_backup ? std::function<void()>{} : commit;
    auto report = state.repair(s.id, false, false, true, tools);
    EXPECT_EQ(backups, 1u);
    EXPECT_FALSE(report.contains("backups"));
    EXPECT_EQ(writer.value("SELECT status FROM sessions WHERE id='" + other.id + "'"),
              "concurrent");
    EXPECT_EQ(files.reads, 1u); // One decoded archive per Tools instance, including its retry.
    files.on_backup = {};
    files.on_read = {};
  }
}
TEST_F(Pr18, SecondInvalidationStopsWithoutRepairWritesAndListsBothBackups) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  codet::Message m;
  m.role = "assistant";
  m.text = "unchanged";
  m.result_file = "results/old/missing.json";
  s.messages = {m};
  state.save(s);
  Db writer(config.state_root / "state.sqlite3");
  const auto before =
      writer.value("SELECT payload FROM session_messages WHERE session='" + s.id + "'");
  size_t backups = 0;
  files.on_backup = [&] {
    ++backups;
    writer.exec("UPDATE sessions SET updated=updated+1 WHERE id='" + s.id + "'");
  };
  Tools tools(config, files, processes, http, clock);
  try {
    state.repair(s.id, false, false, true, tools);
    FAIL();
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("changed twice"));
    const std::string text = e.what();
    const auto first = text.find("; backup:");
    ASSERT_NE(first, std::string::npos);
    EXPECT_NE(text.find("; backup:", first + 1), std::string::npos);
  }
  EXPECT_EQ(backups, 2u);
  EXPECT_EQ(archives(), 0u);
  EXPECT_EQ(writer.value("SELECT payload FROM session_messages WHERE session='" + s.id + "'"),
            before);
}
TEST_F(Pr18, RetryUsesNewTargetRowsAndBacksUpTheAppliedAttempt) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  codet::Message m;
  m.role = "assistant";
  m.text = "old";
  s.messages = {m};
  state.save(s);
  Db writer(config.state_root / "state.sqlite3");
  size_t count = 0;
  files.on_backup = [&] {
    if (count++ == 0)
      writer.exec("UPDATE session_messages SET payload=json_set(payload,'$.text','external "
                  "change') WHERE session='" +
                  s.id + "'");
  };
  Tools tools(config, files, processes, http, clock);
  const auto report = state.repair(s.id, false, false, true, tools);
  EXPECT_EQ(state.load_id(s.id, false).messages[0].text, "external change");
  EXPECT_THAT(files.native.read(report["backup"].get<std::string>()), HasSubstr("external change"));
}
TEST_F(Pr18, ChangedFilesInvalidatePreparationAndRemoveOnlySupersededCandidates) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  codet::Message m;
  m.role = "assistant";
  m.text = "preview";
  const auto ref = fs::path("results") / s.id / (unique_id() + ".json");
  const auto path = artifact_directory(config) / ref;
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "original", true);
  m.result_file = ref.string();
  s.messages = {m};
  state.save(s);
  size_t backups = 0;
  files.on_backup = [&] {
    if (backups++ == 0)
      fs::remove(path);
  };
  Tools tools(config, files, processes, http, clock);
  auto report = state.repair(s.id, false, false, true, tools);
  EXPECT_EQ(backups, 2u);
  EXPECT_EQ(report["issues"].size(), 1u);
  EXPECT_EQ(archives(), 1u);
  EXPECT_THAT(tools.execution_state(state.load_id(s.id, false).messages).dump(),
              HasSubstr(ref.string()));
}
TEST_F(Pr18, ProviderRowsAndRevisionUseTheSameActiveSessionSnapshot) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  s.provider_state = {{"provider", "anthropic"}, {"messages", Json::array()}};
  state.save(s);
  Db reader(config.state_root / "state.sqlite3"), writer(config.state_root / "state.sqlite3");
  sqlite3_stmt *q = nullptr;
  ASSERT_EQ(
      sqlite3_prepare_v2(reader.p, "SELECT id,name FROM sessions WHERE id=?", -1, &q, nullptr),
      SQLITE_OK);
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> handle(q, sqlite3_finalize);
  sqlite3_bind_text(q, 1, s.id.c_str(), -1, SQLITE_TRANSIENT);
  ASSERT_EQ(sqlite3_step(q), SQLITE_ROW);
  const auto query = "SELECT revision FROM provider_revisions WHERE session='" + s.id + "'";
  const auto token = reader.value(query);
  const auto payload = reader.value("SELECT payload FROM provider_parts WHERE position=0");
  writer.exec("UPDATE provider_parts SET payload=json_set(payload,'$.model','external')");
  EXPECT_NE(writer.value(query), token);
  EXPECT_EQ(reader.value(query), token);
  EXPECT_EQ(reader.value("SELECT payload FROM provider_parts WHERE position=0"), payload);
  handle.reset();
  EXPECT_NE(reader.value(query), token);
  EXPECT_EQ(state.load_id(s.id, false).provider_state["model"], "external");
}
TEST_F(Pr18, ToolAssemblyErrorsKeepArgumentsOrAssociatedFramesWithOneRequest) {
  for (const auto &provider : {"openai", "anthropic"})
    for (const bool empty : {false, true})
      for (const auto &kind : {"arguments", "name", "id"}) {
        SCOPED_TRACE(std::string(provider) + kind + (empty ? " empty" : " full"));
        config.provider = provider;
        config.providers[provider].api_key = "fixture";
        http.empty_body = empty;
        http.requests = 0;
        const std::string mode = kind;
        const auto function = Json({{"name", mode == "name" ? "" : "read_file"},
                                    {"arguments", mode == "arguments" ? "{broken" : "{}"}});
        std::string offending;
        if (config.provider == "openai") {
          http.stream = chat_stream(function, mode == "id" ? "" : "call");
          offending = http.stream.substr(6, http.stream.find("\n\n") - 6);
        } else {
          offending = Json({{"type", "content_block_start"},
                            {"index", 0},
                            {"content_block",
                             {{"type", "tool_use"},
                              {"id", mode == "id" ? "" : "call"},
                              {"name", mode == "name" ? "" : "read_file"},
                              {"input", Json::object()}}}})
                          .dump();
          http.stream = "data: " + offending + "\n\n";
          if (mode == "arguments")
            http.stream +=
                "data: " +
                Json({{"type", "content_block_delta"},
                      {"index", 0},
                      {"delta", {{"type", "input_json_delta"}, {"partial_json", "{broken"}}}})
                    .dump() +
                "\n\n";
          http.stream += "data: {\"type\":\"message_stop\"}\n\n";
        }
        CredentialStore credentials(config.home, files, clock);
        ApiProvider api(http, credentials, clock);
        try {
          api.complete(config, "", {}, Json::array(), cancel, {});
          FAIL();
        } catch (const ProviderError &e) {
          EXPECT_THAT(e.what(), HasSubstr("Malformed"));
          EXPECT_EQ(e.body, !empty ? http.stream : (mode == "arguments" ? "{broken" : offending));
        }
        EXPECT_EQ(http.requests, 1u);
      }
}
TEST_F(Pr18, ConsumerJsonExceptionsAndCancellationRemainDistinctFromProviderDecoding) {
  config.providers["openai"].wire_api = "responses";
  http.stream = "data: {\"type\":\"response.output_text.delta\",\"delta\":\"text\"}\n\n";
  CredentialStore credentials(config.home, files, clock);
  ApiProvider api(http, credentials, clock);
  EXPECT_THROW(api.complete(config, "", {}, Json::array(), cancel,
                            [](const auto &) { (void)Json::parse("{bad"); }),
               Json::exception);
  EXPECT_THROW(api.complete(config, "", {}, Json::array(), cancel, {},
                            [](const auto &) { (void)Json::parse("{bad"); }),
               Json::exception);
  try {
    api.complete(config, "", {}, Json::array(), cancel, [&](const auto &) {
      cancel.requested = true;
      (void)Json::parse("{bad");
    });
    FAIL();
  } catch (const Error &e) {
    EXPECT_EQ(e.code, 130);
  }
  EXPECT_EQ(http.requests, 3u);
}

TEST_F(Pr18, RestoredPrivateArchivesAreRecheckedAfterPermissionsAreCorrected) {
  Tools tools(config, files, processes, http, clock);
  auto message = checkpoint(tools);
  const auto path = artifact_directory(config) /
                    message.checkpoint_state.at("recovery_record").get<std::string>();
  ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
  EXPECT_THAT(tools.execution_state({message}).dump(), HasSubstr("mode 0600"));
  ASSERT_EQ(::chmod(path.c_str(), 0600), 0);
  const auto facts = tools.execution_state({message});
  ASSERT_EQ(facts.size(), 1u);
  EXPECT_EQ(facts[0]["call_id"], "old");
}
TEST_F(Pr18, CacheChargesTheExistingSerializedArchiveTextConservatively) {
  Tools tools(config, files, processes, http, clock);
  std::vector<codet::Message> messages;
  for (const auto *call : {"first", "second"}) {
    codet::Message record;
    record.role = "system";
    record.origin = "recovery";
    record.text = std::string(5 * 1024 * 1024, ' ') +
                  Json::array({{{"call_id", call}, {"recovery", "uncertain"}}}).dump();
    record.message_id = digest(record.text);
    tools.retain_result(record);
    codet::Message m;
    m.role = "system";
    m.checkpoint_state = {{"recovery_record", record.result_file}};
    messages.push_back(m);
  }
  EXPECT_EQ(tools.execution_state({messages[0]}).size(), 1u);
  EXPECT_EQ(tools.execution_state({messages[1]}).size(), 1u);
  EXPECT_EQ(tools.execution_state({messages[0]}).size(), 1u);
  EXPECT_EQ(files.reads, 3u);
}
TEST_F(Pr18, BackupWriteFailureAfterPublicationStillNamesTheRetainedBackup) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  files.on_backup = [] { throw Error("after backup publication"); };
  Tools tools(config, files, processes, http, clock);
  try {
    state.repair(s.id, false, true, false, tools);
    FAIL();
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("after backup publication; backup:"));
  }
  size_t n = 0;
  for (const auto &entry : fs::directory_iterator(config.state_root / "repairs"))
    if (entry.is_regular_file())
      ++n;
  EXPECT_EQ(n, 1u);
}
TEST_F(Pr18, DeferredRepairCommitFailureRollsBackRowsAndRemovesNewCandidates) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  codet::Message m;
  m.role = "assistant";
  m.text = "surviving";
  m.result_file = "results/old/missing.json";
  s.messages = {m};
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  const auto before = raw.value("SELECT payload FROM session_messages");
  raw.exec("CREATE TABLE parent(id INTEGER PRIMARY KEY);CREATE TABLE poison(id INTEGER REFERENCES "
           "parent(id) DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER refuse AFTER INSERT ON "
           "session_messages BEGIN INSERT INTO poison VALUES(1); END");
  Tools tools(config, files, processes, http, clock);
  try {
    state.repair(s.id, false, false, true, tools);
    FAIL();
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("backup:"));
  }
  EXPECT_EQ(raw.value("SELECT payload FROM session_messages"), before);
  EXPECT_EQ(archives(), 0u);
  raw.exec("DROP TRIGGER refuse");
  EXPECT_NO_THROW(state.save(s));
  EXPECT_EQ(raw.value("SELECT payload FROM session_messages"), before);
}

TEST_F(Pr18, DecodedArchiveObservationsRetainTheirIdentityOnMissAndHit) {
  Tools tools(config, files, processes, http, clock);
  const auto checkpoint_message = checkpoint(tools);
  const auto ref = checkpoint_message.checkpoint_state.at("recovery_record").get<std::string>();
  const auto path = artifact_directory(config) / ref;
  const auto original = files.native.read(path);
  for (const bool cached : {false, true}) {
    SCOPED_TRACE(cached);
    const auto observation = tools.observe_execution({checkpoint_message, checkpoint_message});
    ASSERT_EQ(observation.archives.size(), 1u);
    ASSERT_EQ(observation.facts.size(), 1u);
    EXPECT_TRUE(tools.reference_current(observation.archives.at(ref)));
    const auto reads = files.reads;
    files.native.atomic_write(path, "replacement after decoding", true);
    EXPECT_FALSE(tools.reference_current(observation.archives.at(ref)));
    EXPECT_EQ(observation.facts[0]["call_id"], "old");
    EXPECT_EQ(files.reads, reads);
    files.native.atomic_write(path, original, true);
    // Populate this identity before testing a cache hit.
    tools.execution_state({checkpoint_message});
  }
}
TEST_F(Pr18, ProviderOnlyRepairReportsProblemsWithoutDependingOnFileStability) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  s.messages = {checkpoint(tools)};
  codet::Message result;
  result.role = "assistant";
  result.text = "surviving";
  result.result_file = "results/old/missing.json";
  s.messages.push_back(result);
  state.save(s);
  const auto inspection = state.repair(s.id, false, false, false, tools);
  const auto archive = artifact_directory(config) /
                       s.messages[0].checkpoint_state.at("recovery_record").get<std::string>();
  size_t backups = 0;
  files.on_backup = [&] {
    ++backups;
    fs::remove(archive);
  };
  const auto report = state.repair(s.id, false, true, false, tools);
  files.on_backup = {};
  EXPECT_EQ(report["issues"], inspection["issues"]);
  EXPECT_EQ(backups, 1u);
  EXPECT_TRUE(report["changed"]);
  EXPECT_FALSE(report.contains("backups"));
  EXPECT_EQ(state.load_id(s.id, false).messages.size(), 2u);
}
TEST_F(Pr18, TrustedAnchorAliasesPreserveSubordinateSymlinkChecks) {
  const auto real = temp.root / "real";
  const auto alias = temp.root / "alias";
  fs::create_directories(real);
  fs::create_directory_symlink(real, alias);
  config.state_root = alias / "state";
  Tools tools(config, files, processes, http, clock);
  const auto ref = fs::path("results") / "session" / "archive.json";
  const auto path = artifact_directory(config) / ref;
  fs::create_directories(path.parent_path());
  files.atomic_write(path, "available", true);
  EXPECT_THAT(tools.observe_result_reference(path.string()), TrustedReference(ref.string()));
  EXPECT_THAT(tools.observe_result_reference(files.canonical(path).string()),
              TrustedReference(ref.string()));
  const auto capture =
      capture_directory(config) / "capture-00000000-0000-0000-0000-000000000001.txt";
  fs::create_directories(capture.parent_path());
  files.atomic_write(capture, "capture", true);
  EXPECT_THAT(tools.observe_result_reference(files.canonical(capture).string()),
              TrustedReference(capture.filename().string()));
  fs::create_symlink(path, path.parent_path() / "unsafe.json");
  EXPECT_FALSE(
      tools.observe_result_reference((path.parent_path() / "unsafe.json").string()).error.empty());
  EXPECT_FALSE(
      tools.observe_result_reference(files.canonical(path.parent_path()).string() + "/unsafe.json")
          .error.empty());
}
TEST_F(Pr18, CustomRootCandidatesAreRemovedAfterStalenessAndDeferredCommitFailure) {
  for (const bool deferred : {false, true}) {
    SCOPED_TRACE(deferred);
    config.state_root = temp.root / (deferred ? "deferred" : "stale");
    config.artifact_root = temp.root / (deferred ? "custom-deferred" : "custom-stale");
    StateStore state(files, config.state_root); // Deliberately differs from Tools' artifact root.
    auto s = state.create();
    codet::Message m;
    m.role = "assistant";
    m.text = "surviving";
    m.result_file = "results/old/missing.json";
    s.messages = {m};
    state.save(s);
    Db raw(config.state_root / "state.sqlite3");
    if (deferred)
      raw.exec("CREATE TABLE parent(id INTEGER PRIMARY KEY);CREATE TABLE poison(id INTEGER "
               "REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER refuse AFTER "
               "INSERT ON session_messages BEGIN INSERT INTO poison VALUES(1); END");
    else
      files.on_backup = [&] {
        raw.exec("UPDATE sessions SET updated=updated+1 WHERE id='" + s.id + "'");
      };
    Tools tools(config, files, processes, http, clock);
    EXPECT_THROW(state.repair(s.id, false, false, true, tools), Error);
    files.on_backup = {};
    EXPECT_EQ(archives(), 0u);
    EXPECT_EQ(state.load_id(s.id, false).messages[0].result_file, m.result_file);
  }
}
TEST_F(Pr18, CandidateCleanupProtectsExistingReferencedAndReplacedArchives) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  codet::Message m;
  m.role = "system";
  m.origin = "recovery";
  m.text = "[]";
  m.message_id = digest(m.text);
  std::vector<RecoveryCandidate> candidates;
  tools.retain_recovery_result(m, candidates);
  ASSERT_EQ(candidates.size(), 1u);
  const auto path = candidates[0].file.path;
  codet::Message reused = m;
  reused.result_file.clear();
  tools.retain_recovery_result(reused, candidates);
  EXPECT_EQ(candidates.size(), 1u);
  s.messages = {m};
  state.save(s);
  EXPECT_NO_THROW(state.discard_recovery_record(candidates[0], tools));
  EXPECT_TRUE(fs::exists(path));
  // A new candidate whose name is replaced must not delete the replacement.
  codet::Message another = m;
  another.message_id = unique_id();
  another.result_file.clear();
  tools.retain_recovery_result(another, candidates);
  const auto replacement = candidates.back().file.path;
  files.native.atomic_write(replacement, "external replacement", true);
  EXPECT_THROW(state.discard_recovery_record(candidates.back(), tools), Error);
  EXPECT_TRUE(fs::exists(replacement));
}
TEST_F(Pr18, CandidateCleanupHandlesWriterFailureAfterPublication) {
  StateStore state(files, config.state_root);
  Tools tools(config, files, processes, http, clock);
  codet::Message m;
  m.role = "assistant";
  m.text = "partial";
  std::vector<RecoveryCandidate> candidates;
  files.on_write = [](const fs::path &p) {
    if (p.extension() == ".json")
      throw Error("after candidate publication");
  };
  EXPECT_THROW(tools.retain_recovery_result(m, candidates), Error);
  files.on_write = {};
  ASSERT_EQ(candidates.size(), 1u);
  ASSERT_TRUE(fs::exists(candidates[0].file.path));
  EXPECT_NO_THROW(state.discard_recovery_record(candidates[0], tools));
  EXPECT_FALSE(fs::exists(candidates[0].file.path));
}
TEST_F(Pr18, FinalRepairValidationDoesNotReopenOrReadArchives) {
  StateStore state(files, config.state_root);
  Tools tools(config, files, processes, http, clock);
  auto s = state.create();
  tools.use_session(s.id);
  s.messages = {checkpoint(tools)};
  state.save(s);
  size_t reads_after_backup = 0, opens_after_backup = 0;
  files.on_backup = [&] {
    files.on_read = [&] { ++reads_after_backup; };
    files.on_open = [&](const fs::path &) { ++opens_after_backup; };
  };
  EXPECT_NO_THROW(state.repair(s.id, false, false, true, tools));
  files.on_backup = {};
  files.on_read = {};
  files.on_open = {};
  EXPECT_EQ(reads_after_backup, 0u);
  EXPECT_EQ(opens_after_backup, 0u);
}
TEST_F(Pr18, FragmentedStructuralErrorsKeepIdentityFramesAndEmptyArgumentsStayInvalid) {
  for (const bool empty_body : {false, true})
    for (const auto *field : {"id", "name"}) {
      SCOPED_TRACE(std::string(field) + (empty_body ? " empty" : " full"));
      const auto first =
          Json({{"choices",
                 Json::array(
                     {{{"delta",
                        {{"tool_calls",
                          Json::array({{{"index", 0},
                                        {"id", std::string(field) == "id" ? "" : "call"},
                                        {"function",
                                         {{"name", std::string(field) == "name" ? "" : "zero"},
                                          {"arguments", "{"}}}}})}}}}})}})
              .dump();
      const auto last =
          Json({{"choices",
                 Json::array(
                     {{{"delta",
                        {{"tool_calls",
                          Json::array({{{"index", 0}, {"function", {{"arguments", "}"}}}}})}}},
                       {"finish_reason", "tool_calls"}}})}})
              .dump();
      http.stream = "data: " + first + "\n\ndata: " + last + "\n\ndata: [DONE]\n\n";
      http.empty_body = empty_body;
      http.requests = 0;
      CredentialStore credentials(config.home, files, clock);
      ApiProvider api(http, credentials, clock);
      try {
        api.complete(config, "", {}, Json::array(), cancel, {});
        FAIL();
      } catch (const ProviderError &e) {
        EXPECT_THAT(e.what(), HasSubstr("missing id or name"));
        EXPECT_EQ(e.body, empty_body ? first : http.stream);
      }
      EXPECT_EQ(http.requests, 1u);
    }
  CredentialStore credentials(config.home, files, clock);
  ApiProvider api(http, credentials, clock);
  http.stream = chat_stream({{"name", "zero"}, {"arguments", "{}"}});
  EXPECT_EQ(api.complete(config, "", {}, Json::array(), cancel, {}).message.calls[0].arguments,
            Json::object());
  http.stream = chat_stream({{"name", "zero"}, {"arguments", ""}});
  EXPECT_THROW(api.complete(config, "", {}, Json::array(), cancel, {}), ProviderError);
}
TEST_F(Pr18, RepairRetriesArchiveSwapAfterDecodingOnCacheMissAndHit) {
  struct LiveProcesses : Processes {
    std::function<void()> on_status;
    size_t starts = 0;
    ProcessResult run(const ProcessSpec &, const Cancellation &) override {
      return {};
    }
    std::string start(const ProcessSpec &) override {
      ++starts;
      return "live";
    }
    ProcessResult poll(const std::string &) override {
      return {0, "", false, true};
    }
    ProcessStatus status(const std::string &id) override {
      if (on_status)
        on_status();
      return {id, true, 0, ""};
    }
    void stop(const std::string &) override {}
  } live;
  for (const bool cached : {false, true}) {
    SCOPED_TRACE(cached);
    config.state_root = temp.root / (cached ? "hit" : "miss");
    StateStore state(files, config.state_root);
    auto s = state.create();
    config.approval = "on-request";
    Tools tools(config, files, live, http, clock, [](const PermissionRequest &) { return true; });
    tools.use_session(s.id);
    s.messages = {checkpoint(tools)};
    state.save(s);
    const auto started = tools.execute(
        {"new", "exec_command", {{"cmd", "fixture"}, {"yield_time_ms", 250}}}, cancel);
    ASSERT_TRUE(started.metadata.contains("session_id"))
        << started.content << started.metadata.dump();
    const auto path = artifact_directory(config) /
                      s.messages[0].checkpoint_state.at("recovery_record").get<std::string>();
    if (cached)
      (void)tools.execution_state(s.messages);
    bool swapped = false;
    live.on_status = [&] {
      if (!swapped) {
        files.native.atomic_write(path, "invalid replacement after archive decoding", true);
        swapped = true;
      }
    };
    const auto report = state.repair(s.id, false, false, true, tools);
    live.on_status = {};
    EXPECT_TRUE(swapped);
    EXPECT_EQ(report["backups"].size(), 2u);
    EXPECT_THAT(report["recovery_facts"].dump(), HasSubstr("recovery_unavailable"));
    EXPECT_THAT(state.load_id(s.id, false).messages[0].checkpoint_state.dump(),
                HasSubstr("recovery_unavailable"));
  }
  EXPECT_EQ(live.starts, 2u); // Historical repair never dispatches another effect.
}

TEST_F(Pr18, InvalidRevisionTokensAreReportedAndRepairedEvenWithNoProviderRows) {
  StateStore state(files, config.state_root);
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (const bool rows : {false, true})
    for (const bool recovery : {false, true}) {
      SCOPED_TRACE(rows);
      SCOPED_TRACE(recovery);
      auto s = state.create();
      if (rows) {
        s.provider_state = {{"provider", "anthropic"}, {"messages", Json::array()}};
        state.save(s);
      }
      raw.exec("INSERT INTO provider_revisions(session,revision) VALUES('" + s.id +
               "','invalid') ON CONFLICT(session) DO UPDATE SET revision='invalid'");
      const auto inspection = state.repair(s.id, false, false, false, tools);
      ASSERT_EQ(inspection["issues"].size(), 1u);
      EXPECT_THROW(state.load_id(s.id, false), Error);
      const auto report = state.repair(s.id, false, !recovery, recovery, tools);
      EXPECT_EQ(report["issues"], inspection["issues"]);
      const auto backup = Json::parse(files.native.read(report["backup"].get<std::string>()));
      EXPECT_EQ(backup["provider_revisions"][0]["revision"], "invalid");
      EXPECT_NE(raw.value("SELECT revision FROM provider_revisions WHERE session='" + s.id + "'"),
                "invalid");
      EXPECT_NO_THROW(state.save(s)); // Check the next write before a load could reset trust.
      auto loaded = state.load_id(s.id, false);
      EXPECT_NO_THROW(state.save(loaded));
      EXPECT_TRUE(state.repair(s.id, false, false, false, tools)["issues"].empty());
      EXPECT_NO_THROW(state.repair(s.id, false, true, false, tools));
    }
}
TEST_F(Pr18, TokenRepairRollsBackAndPreservesItsBackupOnDeferredCommitFailure) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("INSERT INTO provider_revisions VALUES('" + s.id +
           "','invalid');"
           "CREATE TABLE token_parent(id INTEGER PRIMARY KEY);"
           "CREATE TABLE token_poison(id INTEGER REFERENCES token_parent(id) DEFERRABLE INITIALLY "
           "DEFERRED);"
           "CREATE TRIGGER refuse_token AFTER UPDATE ON provider_revisions "
           "BEGIN INSERT INTO token_poison VALUES(1); END");
  std::string error;
  try {
    state.repair(s.id, false, true, false, tools);
  } catch (const Error &e) {
    error = e.what();
  }
  EXPECT_THAT(error, HasSubstr("backup:"));
  EXPECT_EQ(raw.value("SELECT revision FROM provider_revisions WHERE session='" + s.id + "'"),
            "invalid");
  raw.exec("DROP TRIGGER refuse_token");
  EXPECT_NO_THROW(state.repair(s.id, false, true, false, tools));
  EXPECT_NO_THROW(state.save(s));
}
TEST_F(Pr18, AbsoluteCheckpointsNormalizeReadOnlyAndRegisterAcrossForkSaveAndRelocation) {
  const auto real = temp.root / "real";
  const auto alias = temp.root / "alias";
  fs::create_directories(real);
  fs::create_directory_symlink(real, alias);
  config.artifact_root = alias / "artifacts";
  StateStore state(files, config.state_root, artifact_directory(config));
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  codet::Message archive;
  archive.role = "system";
  archive.origin = "recovery";
  archive.text = Json::array({{{"call_id", "old"}, {"recovery", "uncertain"}}}).dump();
  archive.message_id = digest(archive.text);
  std::vector<RecoveryCandidate> candidates;
  tools.retain_recovery_result(archive, candidates);
  const auto absolute = files.canonical(candidates[0].file.path).string();
  codet::Message checkpoint;
  checkpoint.role = "system";
  checkpoint.text = "summary";
  checkpoint.checkpoint_state = {{"recovery_record", absolute}};
  s.messages = {checkpoint};
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  // Reproduce a legacy persisted absolute reference without using the corrected writer.
  raw.exec("UPDATE session_messages SET payload='" + Json(checkpoint).dump() + "' WHERE session='" +
           s.id +
           "';"
           "DELETE FROM session_references WHERE session='" +
           s.id + "'");
  auto loaded = state.load_id(s.id, false);
  EXPECT_EQ(loaded.messages[0].checkpoint_state["recovery_record"], archive.result_file);
  EXPECT_THAT(raw.value("SELECT payload FROM session_messages WHERE session='" + s.id + "'"),
              HasSubstr(absolute));
  raw.exec("CREATE TABLE checkpoint_parent(id INTEGER PRIMARY KEY);"
           "CREATE TABLE checkpoint_poison(id INTEGER REFERENCES checkpoint_parent(id) "
           "DEFERRABLE INITIALLY DEFERRED);"
           "CREATE TRIGGER reject_checkpoint AFTER INSERT ON session_messages BEGIN "
           "INSERT INTO checkpoint_poison VALUES(1); END");
  EXPECT_THROW(state.save(loaded), Error);
  EXPECT_THAT(raw.value("SELECT payload FROM session_messages WHERE session='" + s.id + "'"),
              HasSubstr(absolute));
  raw.exec("DROP TRIGGER reject_checkpoint");
  EXPECT_NO_THROW(state.save(loaded)); // Verify the next save before reloading.
  auto child = state.fork(loaded);
  EXPECT_THAT(state.references(child.id), Contains(archive.result_file));
  EXPECT_NO_THROW(state.discard_recovery_record(candidates[0], tools));
  EXPECT_TRUE(fs::exists(candidates[0].file.path));
  state.save(loaded);
  EXPECT_THAT(state.references(s.id), Contains(archive.result_file));
  EXPECT_THAT(raw.value("SELECT payload FROM session_messages WHERE session='" + s.id + "'"),
              Not(HasSubstr(absolute)));
  const auto moved = temp.root / "moved";
  fs::create_directories(moved);
  fs::copy(config.state_root, moved / "state", fs::copy_options::recursive);
  fs::copy(real / "artifacts", moved / "artifacts", fs::copy_options::recursive);
  config.state_root = moved / "state";
  config.artifact_root = moved / "artifacts";
  StateStore relocated(config.state_root, config.artifact_root);
  Tools moved_tools(config, files, processes, http, clock);
  auto moved_child = relocated.load_id(child.id, false);
  EXPECT_THAT(moved_tools.execution_state(moved_child.messages).dump(), HasSubstr("old"));
  EXPECT_NO_THROW(relocated.save(moved_child));
}
TEST_F(Pr18, UnsafeAndUnavailableAbsoluteCheckpointsRetainTheirOriginalPointers) {
  StateStore state(files, config.state_root, artifact_directory(config));
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  auto m = checkpoint(tools);
  const auto relative = m.checkpoint_state.at("recovery_record").get<std::string>();
  const auto path = artifact_directory(config) / relative;
  const auto bytes = files.native.read(path);
  for (int mode : {0, 1, 2}) {
    SCOPED_TRACE(mode);
    fs::remove(path);
    if (mode == 1) {
      files.atomic_write(path, bytes, true);
      ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
    }
    if (mode == 2) {
      auto outside = temp.root / "outside.json";
      files.atomic_write(outside, bytes, true);
      fs::create_symlink(outside, path);
    }
    m.checkpoint_state = {{"recovery_record", path.string()}};
    s.messages = {m};
    state.save(s);
    auto loaded = state.load_id(s.id, false);
    EXPECT_EQ(loaded.messages[0].checkpoint_state["recovery_record"], path.string());
    EXPECT_THAT(tools.execution_state(loaded.messages).dump(), HasSubstr("recovery_unavailable"));
  }
}
TEST_F(Pr18, CandidateCleanupAcceptsAbsenceAndContinuesAfterThrowingDiagnostics) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  std::vector<RecoveryCandidate> candidates;
  for (int i = 0; i < 3; ++i) {
    codet::Message m;
    m.role = "system";
    m.text = std::to_string(i);
    tools.retain_recovery_result(m, candidates);
  }
  fs::remove(candidates[0].file.path);
  EXPECT_NO_THROW(state.discard_recovery_record(candidates[0], tools));
  files.atomic_write(candidates[1].file.path, "replacement", true);
  size_t diagnostics = 0;
  cleanup_recovery_candidates(state, tools, candidates, [&](const std::string &) {
    ++diagnostics;
    throw std::bad_alloc();
  });
  EXPECT_EQ(diagnostics, 1u);
  EXPECT_TRUE(fs::exists(candidates[1].file.path));
  EXPECT_FALSE(fs::exists(candidates[2].file.path));
}
TEST_F(Pr18, ReferenceInsertionSkipsKnownPathsAndDoesNotPublishNestedKnowledge) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Db raw(config.state_root / "state.sqlite3");
  codet::Message first;
  first.result_file = "results/owner/first.json";
  codet::Message second;
  second.result_file = "results/owner/second.json";
  state.remember_references(s.id, {first});
  raw.exec("CREATE TRIGGER reject_known BEFORE INSERT ON session_references WHEN "
           "NEW.path='results/owner/first.json' BEGIN SELECT RAISE(ABORT,'known reinserted'); END");
  EXPECT_NO_THROW(state.remember_references(s.id, {first, second}));
  EXPECT_THAT(state.references(s.id), Contains(second.result_file));
  codet::Message third;
  third.result_file = "results/owner/third.json";
  raw.exec("CREATE TABLE ref_parent(id INTEGER PRIMARY KEY);CREATE TABLE ref_poison(id INTEGER "
           "REFERENCES ref_parent(id) DEFERRABLE INITIALLY DEFERRED);"
           "CREATE TRIGGER reject_commit AFTER INSERT ON session_references WHEN "
           "NEW.path='results/owner/third.json' BEGIN INSERT INTO ref_poison VALUES(1); END");
  s.messages = {third};
  EXPECT_THROW(state.save(s), Error);
  raw.exec("DROP TRIGGER reject_commit");
  EXPECT_NO_THROW(state.save(s));
  EXPECT_THAT(state.references(s.id), Contains(third.result_file));
}

TEST_F(Pr18, RecoveryCandidateFailuresBeforeWriteAndDuringIdentityCaptureRemainSafe) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Tools tools(config, files, processes, http, clock);
  tools.use_session(s.id);
  codet::Message m;
  m.role = "system";
  m.text = "recovery";
  std::vector<RecoveryCandidate> candidates;
  files.before_write = [](const fs::path &) { throw Error("before archive publication"); };
  EXPECT_THROW(tools.retain_recovery_result(m, candidates), Error);
  files.before_write = {};
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_FALSE(fs::exists(candidates[0].file.path));
  EXPECT_NO_THROW(state.discard_recovery_record(candidates[0], tools));
  candidates.clear();
  m.message_id.clear();
  files.on_write = [](const fs::path &path) {
    if (::chmod(path.c_str(), 0644))
      throw Error("chmod fixture failed");
  };
  EXPECT_THROW(tools.retain_recovery_result(m, candidates), Error);
  files.on_write = {};
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_TRUE(fs::exists(candidates[0].file.path));
  EXPECT_THROW(state.discard_recovery_record(candidates[0], tools), Error);
  EXPECT_TRUE(state.load_id(s.id, false).messages.empty());
}
TEST_F(Pr18, MessageCorruptionStillFailsAsDataCorruption) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  s.messages = {codet::Message{"user", "saved"}};
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  for (const auto *payload : {"{", "{}", "null"}) {
    SCOPED_TRACE(payload);
    raw.exec("UPDATE session_messages SET payload='" + std::string(payload) + "' WHERE session='" +
             s.id + "'");
    try {
      state.load_id(s.id, false);
      FAIL() << "malformed message loaded";
    } catch (const Error &e) {
      EXPECT_THAT(e.what(), HasSubstr("Corrupt session messages"));
    }
  }
  raw.exec("UPDATE session_messages SET payload=CAST(x'7b7d007b7d' AS TEXT) WHERE session='" +
           s.id + "'");
  EXPECT_THROW(state.load_id(s.id, false), Error);
  raw.exec("UPDATE session_messages SET position=4 WHERE session='" + s.id + "'");
  try {
    state.load_id(s.id, false);
    FAIL() << "noncontiguous message loaded";
  } catch (const Error &e) {
    EXPECT_THAT(e.what(), HasSubstr("noncontiguous"));
  }
}

TEST_F(Pr18, RevisionAndProviderCorruptionAreDiscoveredIndependently) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  s.provider_state = {{"provider", "anthropic"}, {"messages", Json::array()}};
  state.save(s);
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("UPDATE provider_parts SET payload='broken';UPDATE provider_revisions SET "
           "revision='invalid'");
  Tools tools(config, files, processes, http, clock);
  auto report = state.repair(s.id, false, false, false, tools);
  ASSERT_EQ(report["issues"].size(), 2u);
  auto fixed = state.repair(s.id, false, true, false, tools);
  EXPECT_EQ(fixed["issues"], report["issues"]);
  EXPECT_NO_THROW(state.save(s));
}

namespace {
struct Pr21 : Pr18 {
  Session legacy(StateStore &state, Tools &tools, Db &raw, bool missing_id = false) {
    auto session = state.create();
    tools.use_session(session.id);
    auto message = checkpoint(tools);
    message.message_id = missing_id ? "" : "checkpoint";
    const auto ref = message.checkpoint_state.at("recovery_record").get<std::string>();
    message.checkpoint_state["recovery_record"] =
        files.canonical(artifact_directory(config) / ref).string();
    session.messages = {message};
    state.save(session);
    // Return the legacy stored view, not save's canonical publication.
    session.messages = {message};
    raw.exec("UPDATE session_messages SET payload='" + Json(message).dump() + "' WHERE session='" +
             session.id + "';DELETE FROM session_references WHERE session='" + session.id + "'");
    return session;
  }
  std::string row(Db &raw, const Session &s) {
    return raw.value("SELECT payload FROM session_messages WHERE session='" + s.id +
                     "' AND position=0");
  }
  void append(StateStore &state, Session &s) {
    codet::Message next;
    next.role = "user";
    next.message_id = unique_id();
    next.text = "continue";
    const auto first = s.messages.size();
    s.messages.push_back(next);
    state.append_messages(s, first);
  }
};
struct CheckpointReply : Provider {
  size_t requests = 0;
  ProviderResult complete(const Config &, const std::string &, const std::vector<codet::Message> &,
                          const Json &, const Cancellation &, const ChunkSink &,
                          const ItemSink &) override {
    ++requests;
    ProviderResult result;
    result.message.role = "assistant";
    result.message.text = "fixture summary or reply";
    return result;
  }
};
} // namespace
TEST_F(Pr21, ReconciliationUsesStableProvenanceAfterArchiveTrustChanges) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (int mode : {0, 1, 2}) {
    SCOPED_TRACE(mode);
    auto original = legacy(state, tools, raw);
    const auto path =
        fs::path(original.messages[0].checkpoint_state.at("recovery_record").get<std::string>());
    auto loaded = state.load_id(original.id, false);
    auto copied = loaded;
    const auto before = files.opens;
    if (mode == 0)
      fs::remove(path);
    else if (mode == 1)
      ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
    else {
      fs::remove(path);
      fs::create_symlink(config.cwd, path);
    }
    EXPECT_NO_THROW(state.reconcile(loaded));
    EXPECT_NO_THROW(state.reconcile(copied));
    EXPECT_EQ(files.opens, before);
    EXPECT_FALSE(absolute_checkpoint_reference(loaded.messages[0]));
    EXPECT_THAT(tools.execution_state(loaded.messages).dump(), HasSubstr("recovery_unavailable"));
    EXPECT_THAT(row(raw, loaded), Not(HasSubstr(path.string())));
    fs::remove(path);
  }
}
TEST_F(Pr21, FallbackIdsDoNotDependOnArchiveAvailabilityAndLoadsRemainReadOnly) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw, true);
  auto expected = original.messages;
  portable_messages(expected, original.id);
  ensure_message_ids(expected);
  auto available = state.load_id(original.id, false);
  EXPECT_EQ(available.messages[0].message_id, expected[0].message_id);
  EXPECT_THAT(row(raw, original), HasSubstr(*absolute_checkpoint_reference(original.messages[0])));
  const auto path = fs::path(*absolute_checkpoint_reference(original.messages[0]));
  ASSERT_EQ(::chmod(path.c_str(), 0644), 0);
  EXPECT_EQ(state.load_id(original.id, false).messages[0].message_id, expected[0].message_id);
  fs::remove(path);
  EXPECT_EQ(state.load_id(original.id, false).messages[0].message_id, expected[0].message_id);
  EXPECT_NO_THROW(state.reconcile(available));
  EXPECT_EQ(state.load_id(original.id, false).messages[0].message_id, expected[0].message_id);
}
TEST_F(Pr21, AppendRewritesOnlyPendingRowsWithoutReopeningArchives) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  auto loaded = state.load_id(original.id, false);
  auto copy = loaded;
  const auto reference =
      loaded.messages[0].checkpoint_state.at("recovery_record").get<std::string>();
  raw.exec("CREATE TABLE checkpoint_writes(n INTEGER);CREATE TRIGGER count_checkpoint_updates "
           "AFTER UPDATE ON session_messages BEGIN INSERT INTO checkpoint_writes VALUES(1); END");
  const auto opens = files.opens;
  const auto bytes = state.serialized_bytes();
  files.on_open = [](const fs::path &) { throw Error("unexpected archive reopen"); };
  append(state, loaded);
  EXPECT_EQ(raw.value("SELECT count(*) FROM checkpoint_writes"), "1");
  EXPECT_THAT(row(raw, loaded), HasSubstr(reference));
  EXPECT_THAT(row(raw, loaded),
              Not(HasSubstr(*absolute_checkpoint_reference(original.messages[0]))));
  EXPECT_THAT(state.references(loaded.id), Contains(reference));
  append(state, loaded);
  EXPECT_EQ(raw.value("SELECT count(*) FROM checkpoint_writes"), "1");
  EXPECT_LT(state.serialized_bytes() - bytes, 3000u);
  EXPECT_EQ(files.opens, opens);
  // A copied snapshot may still carry the pending rewrite. Its identical durable row is accepted.
  state.revise_messages(copy, {});
  EXPECT_EQ(raw.value("SELECT count(*) FROM checkpoint_writes"), "1");
  files.on_open = {};
}
TEST_F(Pr21, NormalTurnPersistsLegacyCheckpointBeforeRelocationAndCompaction) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto s = legacy(state, tools, raw);
  const auto absolute = *absolute_checkpoint_reference(s.messages[0]);
  CheckpointReply provider;
  Agent agent(config, provider, tools, state, {});
  EXPECT_NO_THROW(agent.run(s, "continue", cancel));
  ASSERT_EQ(provider.requests, 1u);
  EXPECT_THAT(row(raw, s), Not(HasSubstr(absolute)));
  const auto moved = temp.root / "moved";
  fs::create_directories(moved);
  fs::copy(config.state_root, moved / "state", fs::copy_options::recursive);
  fs::rename(artifact_directory(config), moved / "artifacts");
  config.state_root = moved / "state";
  config.artifact_root = moved / "artifacts";
  StateStore relocated(files, config.state_root, config.artifact_root);
  Tools moved_tools(config, files, processes, http, clock);
  auto resumed = relocated.load_id(s.id, false);
  EXPECT_THAT(moved_tools.execution_state(resumed.messages).dump(), HasSubstr("old"));
  Agent moved_agent(config, provider, moved_tools, relocated, {});
  EXPECT_NO_THROW(moved_agent.compact(resumed, cancel));
  auto child = relocated.fork(resumed);
  EXPECT_THAT(moved_tools.execution_state(child.messages).dump(), HasSubstr("old"));
}
TEST_F(Pr21, DeferredFailureRetainsPendingRowsForTheNextWriteBeforeReload) {
  for (bool revise : {false, true}) {
    SCOPED_TRACE(revise);
    StateStore state(files, config.state_root, artifact_directory(config));
    Tools tools(config, files, processes, http, clock);
    Db raw(config.state_root / "state.sqlite3");
    auto original = legacy(state, tools, raw);
    auto loaded = state.load_id(original.id, false);
    const auto before = row(raw, loaded);
    raw.exec("CREATE TABLE cp_parent(id INTEGER PRIMARY KEY);CREATE TABLE cp_poison(id INTEGER "
             "REFERENCES cp_parent(id) DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER refuse_cp "
             "AFTER UPDATE ON session_messages BEGIN INSERT INTO cp_poison VALUES(1); END");
    if (revise)
      EXPECT_THROW(state.revise_messages(loaded, {}), Error);
    else {
      EXPECT_THROW(append(state, loaded), Error);
      loaded.messages.resize(1);
    }
    EXPECT_EQ(row(raw, loaded), before);
    raw.exec("DROP TRIGGER refuse_cp;DROP TABLE cp_poison;DROP TABLE cp_parent");
    fs::remove(fs::path(*absolute_checkpoint_reference(original.messages[0])));
    // No reload may repair the in-memory pending state before this retry.
    if (revise)
      EXPECT_NO_THROW(state.revise_messages(loaded, {}));
    else
      EXPECT_NO_THROW(append(state, loaded));
    EXPECT_NE(row(raw, loaded), before);
  }
}
TEST_F(Pr21, PendingNormalizationRejectsExternalEditsWithoutAppendingOrOverwriting) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto s = legacy(state, tools, raw);
  auto loaded = state.load_id(s.id, false);
  raw.exec(
      "UPDATE session_messages SET payload=replace(payload,'summary','external') WHERE session='" +
      s.id + "'");
  const auto external = row(raw, s);
  EXPECT_THROW(append(state, loaded), Error);
  EXPECT_EQ(row(raw, s), external);
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + s.id + "'"), "1");
  loaded.messages.resize(1);
  EXPECT_THROW(state.reconcile(loaded), Error);
  auto replacement = state.load_id(s.id, false);
  EXPECT_NO_THROW(append(state, replacement));
}
TEST_F(Pr21, LazyAnchorRetriesFailuresAndDoesNotBreakDatabaseOperations) {
  StateStore state(files, config.state_root, artifact_directory(config));
  EXPECT_EQ(files.resolutions, 0u);
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  // Use a fresh store so its anchor has not yet been established.
  StateStore fresh(files, config.state_root, artifact_directory(config));
  files.on_canonical = [] { throw Error("injected artifact permission failure"); };
  EXPECT_NO_THROW(fresh.list());
  auto unavailable = fresh.load_id(original.id, false);
  EXPECT_TRUE(absolute_checkpoint_reference(unavailable.messages[0]));
  EXPECT_NO_THROW(fresh.repair(original.id, false, true, false, tools));
  files.on_canonical = {};
  auto available = fresh.load_id(original.id, false);
  EXPECT_FALSE(absolute_checkpoint_reference(available.messages[0]));
  const auto loop = temp.root / "loop";
  fs::create_symlink(loop, loop);
  EXPECT_NO_THROW({
    StateStore broken(config.state_root, loop);
    broken.list();
    broken.load_id(original.id);
  });
}
TEST_F(Pr21, RepeatedReferencesValidateOnceAndNewPreparationRechecksUnavailableArchives) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  auto second = original.messages[0];
  second.message_id = "second";
  original.messages.push_back(second);
  state.save(original);
  raw.exec("UPDATE session_messages SET payload='" + Json(second).dump() + "' WHERE session='" +
           original.id + "'");
  const auto before = files.opens;
  auto loaded = state.load_id(original.id, false);
  EXPECT_EQ(files.opens - before, 1u);
  // Both rows have the same explicit ID because the fixture writes the same legacy payload.
  const auto ref = loaded.messages[0].checkpoint_state.at("recovery_record").get<std::string>();
  fs::remove(artifact_directory(config) / ref);
  auto unavailable = state.load_id(original.id, false);
  EXPECT_TRUE(absolute_checkpoint_reference(unavailable.messages[0]));
  EXPECT_THAT(tools.execution_state(loaded.messages).dump(), HasSubstr("recovery_unavailable"));
}
TEST_F(Pr21, ReceiptAndForkUseTheInjectedFilesystem) {
  StateStore state(files, config.state_root, artifact_directory(config));
  auto s = state.create();
  s.messages = {codet::Message{"assistant", "saved"}};
  state.save(s);
  files.before_write = [](const fs::path &) { throw Error("injected receipt write"); };
  EXPECT_THROW(state.preserve_response(s, s.messages[0]), Error);
  files.before_write = {};
  const auto receipt = state.preserve_response(s, s.messages[0]);
  EXPECT_TRUE(fs::exists(receipt));
  const auto index = artifact_index_path(artifact_directory(config), s.id);
  fs::create_directories(index.parent_path());
  files.atomic_write(
      index, Json({{"kind", "header"}, {"version", 2}, {"session_id", s.id}}).dump() + "\n", true);
  s.messages[0].artifact_index = artifact_index_reference(s.id);
  state.save(s);
  files.on_open = [](const fs::path &) { throw Error("injected fork open"); };
  EXPECT_THROW(state.fork(s), Error);
  files.on_open = {};
  EXPECT_NO_THROW(state.fork(s));
}

TEST_F(Pr21, PinnedAnchorRejectsReplacementAndFullReplacementSupersedesPendingRows) {
  const auto real = temp.root / "real";
  const auto alias = temp.root / "alias";
  fs::create_directories(real);
  fs::create_directory_symlink(real, alias);
  config.artifact_root = alias / "artifacts";
  StateStore state(files, config.state_root, config.artifact_root);
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  auto loaded = state.load_id(original.id, false);
  const auto other = temp.root / "other";
  fs::create_directories(other);
  fs::copy(real / "artifacts", other / "artifacts", fs::copy_options::recursive);
  fs::remove(alias);
  fs::create_directory_symlink(other, alias);
  auto changed = state.load_id(original.id, false);
  EXPECT_TRUE(absolute_checkpoint_reference(changed.messages[0]));
  // The prior projection remains comparable, but never authorizes reads under a changed anchor.
  EXPECT_THAT(tools.execution_state(loaded.messages).dump(), HasSubstr("recovery_unavailable"));
  loaded.messages = {codet::Message{"user", "replacement"}};
  EXPECT_NO_THROW(state.replace_history(loaded));
  EXPECT_NO_THROW(append(state, loaded));
  EXPECT_THAT(row(raw, loaded), HasSubstr("replacement"));
  EXPECT_THAT(row(raw, loaded), Not(HasSubstr("recovery_record")));
}
TEST_F(Pr21, ChangedCheckpointPreparationRunsBeforeTheWriterLock) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto s = state.create(), other = state.create();
  tools.use_session(s.id);
  auto cp = checkpoint(tools);
  cp.message_id = "new-checkpoint";
  cp.checkpoint_state["recovery_record"] =
      (artifact_directory(config) / cp.checkpoint_state.at("recovery_record").get<std::string>())
          .string();
  s.messages = {cp};
  size_t commits = 0;
  files.on_open = [&](const fs::path &) {
    raw.exec("UPDATE sessions SET status='during-prepare' WHERE id='" + other.id + "'");
    ++commits;
  };
  EXPECT_NO_THROW(state.append_messages(s, 0));
  EXPECT_EQ(commits, 1u);
  EXPECT_EQ(raw.value("SELECT status FROM sessions WHERE id='" + other.id + "'"), "during-prepare");
  files.on_open = {};
}

TEST_F(Pr21, NewCheckpointFallbackIdsUseTheLexicallyPortableViewBeforeProjection) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  auto s = state.create();
  tools.use_session(s.id);
  auto cp = checkpoint(tools);
  cp.checkpoint_state["recovery_record"] =
      (artifact_directory(config) / cp.checkpoint_state.at("recovery_record").get<std::string>())
          .string();
  cp.result_file = (capture_directory(config) / ("capture-" + unique_id() + ".txt")).string();
  s.messages = {cp};
  EXPECT_NO_THROW(state.save(s));
  EXPECT_NO_THROW(state.reconcile(s));
  const auto loaded = state.load_id(s.id, false);
  EXPECT_EQ(loaded.messages[0].message_id, s.messages[0].message_id);
  EXPECT_EQ(loaded.messages[0].checkpoint_state, s.messages[0].checkpoint_state);
}

namespace {
struct Pr22 : Pr21 {};
struct VirtualBackupFiles : RepairFiles {
  std::map<fs::path, std::string> backups;
  bool unreadable_backup = false;
  void atomic_write(const fs::path &path, const std::string &bytes, bool secret) override {
    if (path.filename().string().find("-backup-") == std::string::npos) {
      RepairFiles::atomic_write(path, bytes, secret);
      return;
    }
    backups[path] = bytes;
    throw Error("published virtual backup");
  }
  std::string read(const fs::path &path, size_t limit) const override {
    if (const auto found = backups.find(path); found != backups.end()) {
      if (unreadable_backup)
        throw Error("verification unavailable");
      return found->second;
    }
    return RepairFiles::read(path, limit);
  }
};
} // namespace
TEST_F(Pr22, FailedPreparationDoesNotReplaceProvenanceOrInventStoredFingerprints) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (const auto *mode : {"append", "revise", "replace"}) {
    SCOPED_TRACE(mode);
    auto original = legacy(state, tools, raw);
    raw.exec("UPDATE session_messages SET payload='" + Json(original.messages[0]).dump(2) +
             "' WHERE session='" + original.id + "'");
    const auto path = fs::path(*absolute_checkpoint_reference(original.messages[0]));
    const auto archive = files.read(path, 1024 * 1024);
    fs::remove(path);
    auto loaded = state.load_id(original.id, false);
    const auto provenance = loaded.checkpoint_projection;
    files.atomic_write(path, archive, true);
    const bool append_mode = std::string(mode) == "append";
    if (append_mode)
      loaded.messages.push_back(codet::Message{"user", "next"});
    const auto messages = Json(loaded.messages);
    raw.exec("CREATE TRIGGER refuse_prepared BEFORE " +
             std::string(std::string(mode) == "revise" ? "UPDATE" : "INSERT") +
             " ON session_messages BEGIN SELECT RAISE(ABORT,'prepared write failure'); END");
    const auto write = [&] {
      if (append_mode)
        state.append_messages(loaded, 1);
      else if (std::string(mode) == "revise")
        state.revise_messages(loaded, {0});
      else
        state.replace_history(loaded);
    };
    EXPECT_THROW(write(), Error);
    EXPECT_EQ(loaded.checkpoint_projection, provenance);
    EXPECT_EQ(Json(loaded.messages), messages);
    raw.exec("DROP TRIGGER refuse_prepared");
    EXPECT_NO_THROW(write()); // No load may hide speculative provenance before retry.
    EXPECT_NO_THROW(state.revise_messages(loaded, {}));
    EXPECT_FALSE(absolute_checkpoint_reference(state.load_id(loaded.id, false).messages[0]));
  }
}
TEST_F(Pr22, CanonicalToolIdsAndMessagesArePublishedTogetherWithExplicitIdsPreserved) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  auto s = state.create();
  tools.use_session(s.id);
  auto cp = checkpoint(tools);
  cp.role = "tool";
  cp.tool_call_id = "call";
  cp.result_file = "results/owner/output.json";
  cp.text = Json({{"content", Json({{"result_path", "/old/root/output.json"}}).dump()}}).dump();
  cp.checkpoint_state["recovery_record"] =
      (artifact_directory(config) / cp.checkpoint_state.at("recovery_record").get<std::string>())
          .string();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.message_id = "stable-assistant";
  assistant.calls = {{"call", "exec_command", Json::object()}};
  s.messages = {assistant, cp};
  s.provider_state = {{"provider", "anthropic"}, {"messages", Json::array()}};
  const auto provider = s.provider_state;
  auto expected = s.messages;
  portable_messages(expected, s.id);
  ensure_message_ids(expected);
  ASSERT_NO_THROW(state.save(s));
  EXPECT_EQ(s.messages[1].message_id, expected[1].message_id);
  EXPECT_EQ(s.messages[0].message_id, "stable-assistant");
  EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(s.messages));
  EXPECT_EQ(s.provider_state, provider);
  EXPECT_NO_THROW(state.reconcile(s));
}
TEST_F(Pr22, LegacyBlobsProjectReadOnlyAndMaterializeOnAppend) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw, true);
  const auto blob = Json(original.messages).dump(2);
  raw.exec("UPDATE sessions SET message_format=0,messages='" + blob + "' WHERE id='" + original.id +
           "';DELETE FROM session_messages WHERE session='" + original.id + "'");
  auto loaded = state.load_id(original.id, false);
  EXPECT_FALSE(absolute_checkpoint_reference(loaded.messages[0]));
  EXPECT_EQ(raw.value("SELECT messages FROM sessions WHERE id='" + original.id + "'"), blob);
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + original.id + "'"),
            "0");
  EXPECT_NO_THROW(append(state, loaded));
  EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + original.id + "'"), "1");
  EXPECT_EQ(Json(state.load_id(original.id, false).messages), Json(loaded.messages));
  EXPECT_THAT(
      state.references(original.id),
      Contains(loaded.messages[0].checkpoint_state.at("recovery_record").get<std::string>()));
}
TEST_F(Pr22, ReconciliationAndToolPublicationPrepareArchivesBeforeTheirWriterLock) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto unrelated = state.create();
  size_t concurrent_writes = 0;
  const auto observe = [&](const fs::path &) {
    raw.exec("UPDATE sessions SET status='preparation' WHERE id='" + unrelated.id + "'");
    ++concurrent_writes;
  };
  auto original = legacy(state, tools, raw);
  const auto path = fs::path(*absolute_checkpoint_reference(original.messages[0]));
  const auto archive = files.read(path, 1024 * 1024);
  fs::remove(path);
  auto loaded = state.load_id(original.id, false);
  files.atomic_write(path, archive, true);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{"pending", "exec_command", Json::object()}};
  loaded.messages.push_back(assistant);
  state.append_messages(loaded, 1);
  // Restore a legacy reference and reload while unavailable, forcing recovery-branch preparation.
  raw.exec("UPDATE session_messages SET payload='" + Json(original.messages[0]).dump() +
           "' WHERE session='" + original.id + "' AND position=0");
  fs::remove(path);
  loaded = state.load_id(original.id, false);
  files.atomic_write(path, archive, true);
  bool wrote_during_recovery = false;
  files.on_open = [&](const fs::path &p) {
    if (!wrote_during_recovery) {
      wrote_during_recovery = true;
      observe(p);
    }
  };
  EXPECT_NO_THROW(state.reconcile(loaded));
  EXPECT_EQ(concurrent_writes, 1u);
  files.on_open = {};
  for (bool transcript : {false, true}) {
    auto s = state.create();
    codet::Message call_message;
    call_message.role = "assistant";
    ToolCall call{unique_id(), "exec_command", Json::object()};
    call_message.calls = {call};
    s.messages = {call_message};
    state.save(s);
    state.tool_started(s.id, call);
    auto result = original.messages[0];
    result.role = "tool";
    result.tool_call_id = call.id;
    files.on_open = observe;
    if (transcript)
      EXPECT_NO_THROW(state.complete_tool(s, call, result));
    else
      EXPECT_NO_THROW(state.tool_finished(s.id, call, Json(result)));
    files.on_open = {};
  }
  EXPECT_EQ(concurrent_writes, 3u);
}
TEST_F(Pr22, SnapshotCopiesStayIndependentAndPendingRevisionWritesEachPositionOnce) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  auto loaded = state.load_id(original.id, false), copy = loaded;
  const auto pending = copy.checkpoint_projection;
  raw.exec("CREATE TABLE pr22_writes(position INTEGER);CREATE TRIGGER pr22_count AFTER UPDATE ON "
           "session_messages BEGIN INSERT INTO pr22_writes VALUES(NEW.position); END");
  loaded.messages[0].text = "revised summary";
  state.revise_messages(loaded, {0});
  EXPECT_NE(loaded.checkpoint_projection, pending);
  EXPECT_EQ(copy.checkpoint_projection, pending);
  EXPECT_EQ(raw.value("SELECT count(*) FROM pr22_writes"), "1");
  EXPECT_THROW(state.revise_messages(copy, {}),
               Error); // This snapshot predates a real content edit.
  EXPECT_NO_THROW(append(state, loaded));
  EXPECT_EQ(raw.value("SELECT count(*) FROM pr22_writes"), "1");
  EXPECT_EQ(loaded.messages[0].text, "revised summary");
}
TEST_F(Pr22, ConstWriteAdaptersLeaveTheOriginalSnapshotUntouched) {
  StateStore state(files, config.state_root, artifact_directory(config));
  auto s = state.create();
  s.messages = {codet::Message{"user", "missing id"}};
  const auto immutable = s;
  state.save(immutable);
  EXPECT_TRUE(immutable.messages[0].message_id.empty());
  EXPECT_FALSE(state.load_id(s.id, false).messages[0].message_id.empty());
}
TEST_F(Pr22, PublishedVirtualBackupsAreReportedWithoutNativeFilesystemQueries) {
  for (bool unreadable : {false, true}) {
    VirtualBackupFiles virtual_files;
    virtual_files.unreadable_backup = unreadable;
    StateStore state(virtual_files, config.state_root, artifact_directory(config));
    Tools tools(config, virtual_files, processes, http, clock);
    auto s = state.create();
    try {
      state.repair(s.id, false, true, false, tools);
      FAIL();
    } catch (const Error &error) {
      ASSERT_EQ(virtual_files.backups.size(), 1u);
      EXPECT_THAT(error.what(), HasSubstr("published virtual backup"));
      EXPECT_THAT(error.what(), HasSubstr(virtual_files.backups.begin()->first.string()));
      if (unreadable)
        EXPECT_THAT(error.what(), HasSubstr("publication unconfirmed"));
    }
  }
}

TEST_F(Pr22, DeferredFailureDoesNotPublishANewlyEstablishedMapping) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  const auto path = fs::path(*absolute_checkpoint_reference(original.messages[0]));
  const auto archive = files.read(path, 1024 * 1024);
  fs::remove(path);
  auto loaded = state.load_id(original.id, false);
  const auto provenance = loaded.checkpoint_projection;
  files.atomic_write(path, archive, true);
  raw.exec(
      "CREATE TABLE pr22_parent(id PRIMARY KEY);CREATE TABLE pr22_poison(id REFERENCES "
      "pr22_parent(id) DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER pr22_deferred AFTER UPDATE "
      "ON session_messages BEGIN INSERT INTO pr22_poison VALUES(1); END");
  EXPECT_THROW(state.revise_messages(loaded, {0}), Error);
  EXPECT_EQ(loaded.checkpoint_projection, provenance);
  EXPECT_THAT(row(raw, loaded), HasSubstr(path.string()));
  raw.exec("DROP TRIGGER pr22_deferred");
  EXPECT_NO_THROW(state.revise_messages(loaded, {0}));
  EXPECT_NO_THROW(append(state, loaded));
  EXPECT_FALSE(absolute_checkpoint_reference(loaded.messages[0]));
}

TEST_F(Pr22, RepeatedConstAdaptersAcceptTheirIdenticalCommittedCheckpointRewrite) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  auto loaded = state.load_id(original.id, false);
  loaded.messages[0].text = "revised through const adapter";
  const auto immutable = loaded;
  EXPECT_NO_THROW(state.save(immutable));
  EXPECT_NO_THROW(state.save(immutable));
  EXPECT_NO_THROW(state.revise_messages(immutable, {0}));
  EXPECT_EQ(immutable.checkpoint_projection, loaded.checkpoint_projection);
}

TEST_F(Pr22, BlobToolCompletionPreservesEachHistoricalToolsNormalizationPolicy) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Db raw(config.state_root / "state.sqlite3");
  auto s = state.create();
  codet::Message old_call, old_result, new_call, new_result;
  old_call.role = new_call.role = "assistant";
  old_call.calls = {{"read", "read_file", Json::object()}};
  old_result.role = new_result.role = "tool";
  old_result.tool_call_id = "read";
  old_result.result_file = "results/owner/read.json";
  old_result.text =
      Json({{"content", Json({{"result_path", "/user/document/value"}}).dump()}}).dump();
  ToolCall call{"exec", "exec_command", Json::object()};
  new_call.calls = {call};
  new_result.tool_call_id = call.id;
  new_result.result_file = "results/owner/exec.json";
  new_result.text =
      Json({{"content", Json({{"result_path", "/legacy/exec/output"}}).dump()}}).dump();
  s.messages = {old_call, old_result, new_call};
  state.save(s);
  raw.exec("UPDATE sessions SET message_format=0,messages='" + Json(s.messages).dump() +
           "' WHERE id='" + s.id + "';DELETE FROM session_messages WHERE session='" + s.id + "'");
  auto loaded = state.load_id(s.id, false);
  state.tool_started(s.id, call);
  EXPECT_NO_THROW(state.complete_tool(loaded, call, new_result));
  EXPECT_EQ(loaded.messages[1].text, old_result.text);
  EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(loaded.messages));
  EXPECT_THAT(loaded.messages.back().text, HasSubstr("results/owner/exec.json"));
}

namespace {
struct Pr23 : Pr21 {
  Session blob(StateStore &state, Db &raw) {
    auto s = state.create();
    s.messages = {codet::Message{"user", "original prefix"},
                  codet::Message{"assistant", "original reply"}};
    state.save(s);
    raw.exec("UPDATE sessions SET message_format=0,messages='" + Json(s.messages).dump(2) +
             "' WHERE id='" + s.id + "';DELETE FROM session_messages WHERE session='" + s.id + "'");
    return state.load_id(s.id, false);
  }
};
struct BeforeBackupFiles : VirtualBackupFiles {
  void atomic_write(const fs::path &p, const std::string &bytes, bool secret) override {
    if (p.filename().string().find("-backup-") != std::string::npos)
      throw Error("failure before backup publication");
    VirtualBackupFiles::atomic_write(p, bytes, secret);
  }
};
} // namespace
TEST_F(Pr23, BlobAppendAndRevisionRejectTruncatedAndUnrequestedChangesBeforeConversion) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Db raw(config.state_root / "state.sqlite3");
  for (const auto *mode :
       {"truncated-append", "edited-prefix", "unrequested-revision", "short-revision"}) {
    SCOPED_TRACE(mode);
    auto s = blob(state, raw);
    const auto stored = raw.value("SELECT messages FROM sessions WHERE id='" + s.id + "'");
    const auto projection = s.checkpoint_projection;
    if (std::string(mode) == "truncated-append") {
      s.messages.resize(1);
      s.messages.push_back(codet::Message{"user", "would overwrite original reply"});
      EXPECT_THROW(state.append_messages(s, 1), Error);
    } else if (std::string(mode) == "edited-prefix") {
      s.messages[0].text = "unrequested prefix edit";
      s.messages.push_back(codet::Message{"user", "new suffix"});
      EXPECT_THROW(state.append_messages(s, 2), Error);
    } else {
      s.messages[0].text = "unrequested prefix edit";
      if (std::string(mode) == "short-revision")
        s.messages.resize(1);
      else
        s.messages[1].text = "requested revision";
      EXPECT_THROW(state.revise_messages(s, {s.messages.size() - 1}), Error);
    }
    EXPECT_EQ(s.checkpoint_projection, projection);
    EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + s.id + "'"), "0");
    EXPECT_EQ(raw.value("SELECT messages FROM sessions WHERE id='" + s.id + "'"), stored);
    EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + s.id + "'"), "0");
    // Retry an explicitly valid write before another load can hide pending state.
    s.messages = Json::parse(stored).get<std::vector<codet::Message>>();
    s.messages.push_back(codet::Message{"user", "valid suffix"});
    EXPECT_NO_THROW(state.append_messages(s, 2));
    EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(s.messages));
  }
}
TEST_F(Pr23, ValidBlobRevisionAndExplicitReplacementHaveDistinctBoundaries) {
  StateStore state(files, config.state_root);
  Db raw(config.state_root / "state.sqlite3");
  auto s = blob(state, raw);
  s.messages[1].text = "requested new reply";
  EXPECT_NO_THROW(state.revise_messages(s, {1}));
  EXPECT_EQ(s.messages[0].text, "original prefix");
  EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(s.messages));
  auto replacement = blob(state, raw);
  replacement.messages = {codet::Message{"system", "intentional new history"}};
  EXPECT_NO_THROW(state.replace_history(replacement));
  EXPECT_EQ(state.load_id(replacement.id, false).messages.size(), 1u);
  EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + replacement.id + "'"),
            "1");
}
TEST_F(Pr23, RepairedLoadCapturesJournalBeforeProjectionAllowsAnExternalCommit) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  ToolCall call{"coherent-call", "exec_command", {{"version", 1}}};
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {call};
  original.messages.push_back(assistant);
  state.append_messages(original, 1);
  state.tool_started(original.id, call);
  bool committed = false;
  files.on_open = [&](const fs::path &) {
    if (committed)
      return;
    committed = true;
    auto next = original.messages[1];
    next.calls[0].arguments["version"] = 2;
    raw.exec("BEGIN IMMEDIATE;UPDATE session_messages SET payload='" + Json(next).dump() +
             "' WHERE session='" + original.id +
             "' AND position=1;UPDATE tools SET args='{\"version\":2}' WHERE session='" +
             original.id + "';COMMIT");
  };
  Session loaded;
  ASSERT_NO_THROW(loaded = state.load_id(original.id));
  files.on_open = {};
  ASSERT_TRUE(committed);
  EXPECT_EQ(loaded.messages[1].calls[0].arguments["version"], 1);
  EXPECT_THAT(loaded.messages.back().text, HasSubstr("uncertain"));
  auto current = state.load_id(original.id);
  EXPECT_EQ(current.messages[1].calls[0].arguments["version"], 2);
  EXPECT_THROW(state.reconcile(loaded), Error); // A genuine transcript edit remains a conflict.
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + original.id + "'"),
            "2");
  EXPECT_EQ(raw.value("SELECT status FROM tools WHERE session='" + original.id + "'"), "running");
}
TEST_F(Pr23, ReconstructedJournalUsesThePreparedCanonicalTranscriptPayload) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto s = state.create();
  tools.use_session(s.id);
  auto result = checkpoint(tools);
  const auto identifier = result.checkpoint_state.at("recovery_record").get<std::string>();
  const auto path = artifact_directory(config) / identifier;
  const auto bytes = files.read(path, 1024 * 1024);
  fs::remove(path);
  result.checkpoint_state["recovery_record"] = path.string();
  result.role = "tool";
  result.tool_call_id = "canonical-call";
  result.message_id.clear();
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{result.tool_call_id, "exec_command", Json::object()}};
  s.messages = {assistant, result};
  state.save(s);
  files.atomic_write(path, bytes, true);
  EXPECT_NO_THROW(state.reconcile(s));
  const auto journal = raw.value("SELECT result FROM tools WHERE session='" + s.id + "'");
  const auto transcript =
      raw.value("SELECT payload FROM session_messages WHERE session='" + s.id + "' AND position=1");
  EXPECT_EQ(journal, transcript);
  EXPECT_EQ(Json::parse(journal)["checkpoint_state"]["recovery_record"], identifier);
  EXPECT_EQ(Json::parse(journal)["message_id"], s.messages[1].message_id);
  EXPECT_NO_THROW(state.reconcile(s));
}
TEST_F(Pr23, ReconciliationAllowsRepeatedUnrelatedCommitsWithoutInvalidation) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto other = state.create();
  auto original = legacy(state, tools, raw);
  codet::Message assistant;
  assistant.role = "assistant";
  assistant.calls = {{unique_id(), "exec_command", Json::object()}};
  original.messages.push_back(assistant);
  state.append_messages(original, 1);
  const auto path = fs::path(*absolute_checkpoint_reference(original.messages[0]));
  const auto bytes = files.read(path, 1024 * 1024);
  fs::remove(path);
  auto loaded = state.load_id(original.id, false);
  files.atomic_write(path, bytes, true);
  size_t commits = 0;
  files.on_open = [&](const fs::path &) {
    ++commits;
    raw.exec("UPDATE sessions SET status='commit-" + unique_id() + "' WHERE id='" + other.id + "'");
  };
  EXPECT_NO_THROW(state.reconcile(loaded));
  files.on_open = {};
  EXPECT_GE(commits, 1u);
  EXPECT_EQ(loaded.messages.size(), 3u);
}
TEST_F(Pr23, BackupDiagnosticsDistinguishRetainedFilesFromUnconfirmedCandidates) {
  for (int stage : {0, 1, 2}) {
    SCOPED_TRACE(stage);
    BeforeBackupFiles before;
    VirtualBackupFiles after;
    after.unreadable_backup = stage == 2;
    auto &injected = stage == 0 ? static_cast<VirtualBackupFiles &>(before) : after;
    StateStore state(injected, config.state_root, artifact_directory(config));
    Tools tools(config, injected, processes, http, clock);
    const auto s = state.create();
    try {
      state.repair(s.id, false, true, false, tools);
      FAIL();
    } catch (const Error &error) {
      const std::string message = error.what();
      if (stage == 1) {
        EXPECT_THAT(message, HasSubstr("; backup: "));
        EXPECT_THAT(message, Not(HasSubstr("publication unconfirmed")));
      } else {
        EXPECT_THAT(message, HasSubstr("backup candidate (publication unconfirmed): "));
        EXPECT_THAT(message, Not(HasSubstr("; backup: ")));
      }
      EXPECT_THAT(message, HasSubstr(stage == 0 ? "failure before backup publication"
                                                : "published virtual backup"));
    }
  }
}

TEST_F(Pr23, RepeatingAnExplicitConstReplacementCanObserveAnAlreadyRemovedCheckpoint) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool blob : {false, true}) {
    auto original = legacy(state, tools, raw);
    if (blob)
      raw.exec("UPDATE sessions SET message_format=0,messages='" + Json(original.messages).dump() +
               "' WHERE id='" + original.id + "';DELETE FROM session_messages WHERE session='" +
               original.id + "'");
    auto loaded = state.load_id(original.id, false);
    auto stale = loaded;
    loaded.messages.clear();
    const Session &immutable = loaded;
    const auto provenance = immutable.checkpoint_projection;
    EXPECT_NO_THROW(state.replace_history(immutable));
    EXPECT_NO_THROW(state.replace_history(immutable));
    EXPECT_EQ(immutable.checkpoint_projection, provenance);
    EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + loaded.id + "'"),
              "0");
    EXPECT_THROW(state.revise_messages(stale, {}), Error);
    stale.messages.push_back(codet::Message{"user", "stale append"});
    EXPECT_THROW(state.append_messages(stale, 1), Error);
  }
}

TEST_F(Pr23, CopiedBlobProjectionRejectsADifferentCommittedCheckpointAfterConversion) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  auto original = legacy(state, tools, raw);
  raw.exec("UPDATE sessions SET message_format=0,messages='" + Json(original.messages).dump() +
           "' WHERE id='" + original.id + "';DELETE FROM session_messages WHERE session='" +
           original.id + "'");
  auto first = state.load_id(original.id, false);
  auto stale = first;
  first.messages[0].text = "committed revision during materialization";
  state.revise_messages(first, {0});
  stale.messages.push_back(codet::Message{"user", "new suffix from stale snapshot"});
  EXPECT_THROW(state.append_messages(stale, 1), Error);
  EXPECT_THAT(row(raw, first), HasSubstr("committed revision during materialization"));
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + first.id + "'"),
            "1");
}
TEST_F(Pr23, ConvertedBlobSnapshotsValidateUntouchedOrdinaryRowsBeforeWriting) {
  StateStore state(files, config.state_root);
  Db raw(config.state_root / "state.sqlite3");
  auto first = blob(state, raw);
  auto stale = first;
  first.messages[0].text = "committed ordinary prefix";
  state.revise_messages(first, {0});
  stale.messages.push_back(codet::Message{"user", "stale suffix"});
  EXPECT_THROW(state.append_messages(stale, 2), Error);
  stale.messages.pop_back();
  stale.messages[1].text = "requested reply revision";
  EXPECT_THROW(state.revise_messages(stale, {1}), Error);
  EXPECT_THAT(row(raw, first), HasSubstr("committed ordinary prefix"));
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + first.id + "'"),
            "2");
  stale.messages[0] = first.messages[0];
  EXPECT_NO_THROW(state.revise_messages(stale, {1}));
  EXPECT_EQ(Json(state.load_id(stale.id, false).messages), Json(stale.messages));

  auto constant = blob(state, raw);
  const Session &view = constant;
  constant.messages.push_back(codet::Message{"user", "first suffix"});
  EXPECT_NO_THROW(state.append_messages(view, 2));
  constant.messages.push_back(codet::Message{"user", "second suffix"});
  EXPECT_NO_THROW(state.append_messages(view, 3));
  EXPECT_EQ(state.load_id(constant.id, false).messages.size(), 4u);
  constant.messages.push_back(codet::Message{"user", "mutable handoff"});
  EXPECT_NO_THROW(state.append_messages(constant, 4));
  EXPECT_EQ(Json(state.load_id(constant.id, false).messages), Json(constant.messages));
}

namespace {
struct Pr24 : Pr23 {
  std::string token(Db &db, const std::string &id) {
    return db.value("SELECT revision FROM session_revisions WHERE session='" + id + "'");
  }
  Session shifted(StateStore &state, Tools &tools, Db &raw, bool blob, bool insert) {
    auto s = state.create();
    tools.use_session(s.id);
    codet::Message assistant;
    assistant.role = "assistant";
    assistant.calls = {{unique_id(), "exec_command", Json::object()}};
    auto cp = checkpoint(tools);
    cp.message_id = unique_id();
    const auto ref = cp.checkpoint_state.at("recovery_record").get<std::string>();
    cp.checkpoint_state["recovery_record"] = (artifact_directory(config) / ref).string();
    s.messages = {assistant, cp};
    if (!insert) {
      codet::Message result;
      result.role = "tool";
      result.tool_call_id = assistant.calls[0].id;
      result.text = R"({"content":"observed result","is_error":false,"metadata":{}})";
      s.messages.push_back(result);
    }
    state.save(s);
    auto stored = s.messages;
    stored[1] = cp;
    if (blob)
      raw.exec("UPDATE sessions SET message_format=0,messages='" + Json(stored).dump() +
               "' WHERE id='" + s.id + "';DELETE FROM session_messages WHERE session='" + s.id +
               "'");
    else
      raw.exec("UPDATE session_messages SET payload='" + Json(cp).dump() + "' WHERE session='" +
               s.id + "' AND position=1");
    return state.load_id(s.id);
  }
  void remove_session_schema(Db &raw) {
    while (raw.value("SELECT count(*) FROM sqlite_master WHERE type='trigger' AND name LIKE "
                     "'session_revision_%'") != "0")
      raw.exec("DROP TRIGGER " + raw.value("SELECT name FROM sqlite_master WHERE type='trigger' "
                                           "AND name LIKE 'session_revision_%' LIMIT 1"));
    raw.exec("DROP TABLE session_revisions;PRAGMA user_version=7");
  }
};
struct ToolReceiptFiles : RepairFiles {
  bool fail_receipt = false;
  void atomic_write(const fs::path &p, const std::string &text, bool secret) override {
    if (fail_receipt && p.filename().string().find("-tool-result-") != std::string::npos)
      throw Error("receipt failure");
    RepairFiles::atomic_write(p, text, secret);
  }
};
} // namespace
TEST_F(Pr24, SessionMarkersCoverEveryPersistedInputAndMovedRows) {
  StateStore state(files, config.state_root);
  Db raw(config.state_root / "state.sqlite3");
  for (const auto *table : {"session_messages", "tools", "session_references", "provider_parts",
                            "provider_revisions"}) {
    SCOPED_TRACE(table);
    auto a = state.create(), b = state.create();
    auto ta = token(raw, a.id), tb = token(raw, b.id);
    const std::string name(table);
    const auto insert =
        name == "session_messages" ? "INSERT INTO session_messages VALUES('" + a.id + "',0,'{}')"
        : name == "tools"
            ? "INSERT INTO tools VALUES('" + a.id + "','call','exec_command','{}',NULL,'running')"
        : name == "session_references" ? "INSERT INTO session_references VALUES('" + a.id +
                                             "','result','results/owner/result.json')"
        : name == "provider_parts" ? "INSERT INTO provider_parts VALUES('" + a.id + "',0,'null')"
                                   : "INSERT INTO provider_revisions VALUES('" + a.id +
                                         "','0123456789abcdef0123456789abcdef')";
    raw.exec(insert);
    EXPECT_NE(token(raw, a.id), ta);
    EXPECT_EQ(token(raw, b.id), tb);
    ta = token(raw, a.id);
    raw.exec("UPDATE " + name + " SET session=session WHERE session='" + a.id + "'");
    EXPECT_NE(token(raw, a.id), ta);
    ta = token(raw, a.id);
    raw.exec("UPDATE " + name + " SET session='" + b.id + "' WHERE session='" + a.id + "'");
    EXPECT_NE(token(raw, a.id), ta);
    EXPECT_NE(token(raw, b.id), tb);
    tb = token(raw, b.id);
    raw.exec("DELETE FROM " + name + " WHERE session='" + b.id + "'");
    EXPECT_NE(token(raw, b.id), tb);
  }
  auto s = state.create();
  const auto original = token(raw, s.id);
  raw.exec("BEGIN;UPDATE sessions SET name='rolled back' WHERE id='" + s.id + "';ROLLBACK");
  EXPECT_EQ(token(raw, s.id), original);
  raw.exec("UPDATE sessions SET name='committed' WHERE id='" + s.id + "'");
  EXPECT_NE(token(raw, s.id), original);
  raw.exec("PRAGMA foreign_keys=ON;DELETE FROM sessions WHERE id='" + s.id + "'");
  EXPECT_EQ(raw.value("SELECT count(*) FROM session_revisions WHERE session='" + s.id + "'"), "0");
}
TEST_F(Pr24, MissingAndMalformedSessionTokensAreRepairableInBothMutationModes) {
  StateStore state(files, config.state_root);
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool missing : {false, true})
    for (bool recovery : {false, true}) {
      auto s = state.create();
      raw.exec(missing
                   ? "DELETE FROM session_revisions WHERE session='" + s.id + "'"
                   : "UPDATE session_revisions SET revision='broken' WHERE session='" + s.id + "'");
      EXPECT_THROW(state.load_id(s.id, false), Error);
      const auto inspection = state.repair(s.id, false, false, false, tools);
      EXPECT_THAT(inspection["issues"].dump(), HasSubstr("session revision token"));
      const auto report = state.repair(s.id, false, !recovery, recovery, tools);
      EXPECT_EQ(inspection["issues"], report["issues"]);
      const auto backup = Json::parse(files.read(report["backup"].get<std::string>(), 1024 * 1024));
      EXPECT_EQ(backup["session_revisions"].size(), missing ? 0u : 1u);
      if (!missing)
        EXPECT_EQ(backup["session_revisions"][0]["revision"], "broken");
      EXPECT_EQ(token(raw, s.id).size(), 32u);
      EXPECT_NO_THROW(s = state.load_id(s.id, false));
      s.messages.push_back(codet::Message{"user", "next write"});
      EXPECT_NO_THROW(state.append_messages(s, 0));
      EXPECT_NO_THROW(state.repair(s.id, false, true, false, tools));
    }
}
TEST_F(Pr24, SessionRevisionInfrastructureRemainsStrictAfterSchemaChanges) {
  StateStore state(files, config.state_root);
  auto s = state.create();
  Db raw(config.state_root / "state.sqlite3");
  raw.exec("DROP TRIGGER session_revision_tools_UPDATE");
  EXPECT_THROW(state.load_id(s.id, false), Error);
  Tools tools(config, files, processes, http, clock);
  EXPECT_THROW(state.repair(s.id, false, true, false, tools), Error);
  EXPECT_THROW(StateStore fresh(config.state_root), Error);
}
TEST_F(Pr24, SchemaSevenUpgradeSeedsMarkersWithoutConvertingOrDecodingBlobs) {
  std::string id;
  {
    StateStore state(files, config.state_root);
    auto s = state.create();
    id = s.id;
    s.messages = {codet::Message{"user", "original"}};
    state.save(s);
  }
  Db raw(config.state_root / "state.sqlite3");
  remove_session_schema(raw);
  raw.exec("UPDATE sessions SET message_format=0,messages='{malformed' WHERE id='" + id +
           "';DELETE FROM session_messages WHERE session='" + id + "'");
  {
    StateStore upgraded(files, config.state_root);
    EXPECT_EQ(raw.value("PRAGMA user_version"), "8");
    EXPECT_EQ(token(raw, id).size(), 32u);
    EXPECT_EQ(raw.value("SELECT messages FROM sessions WHERE id='" + id + "'"), "{malformed");
    EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + id + "'"), "0");
    EXPECT_THROW(upgraded.load_id(id, false), Error);
  }
  size_t backups = 0;
  for (const auto &entry : fs::directory_iterator(config.state_root))
    if (entry.path().filename().string().find(".pre-v8-") != std::string::npos) {
      ++backups;
      struct stat st{};
      ASSERT_EQ(::stat(entry.path().c_str(), &st), 0);
      EXPECT_EQ(st.st_mode & 0777, 0600);
    }
  EXPECT_EQ(backups, 1u);
}
TEST_F(Pr24, SchemaEightUpgradeRollsBackAndRequiresInactiveSessions) {
  std::string id;
  std::shared_ptr<void> lease;
  {
    StateStore state(files, config.state_root);
    auto s = state.create();
    id = s.id;
    lease = state.acquire(id);
  }
  Db raw(config.state_root / "state.sqlite3");
  remove_session_schema(raw);
  EXPECT_THROW(StateStore active(config.state_root), Error);
  EXPECT_EQ(raw.value("PRAGMA user_version"), "7");
  lease.reset();
  raw.exec("CREATE INDEX session_revision_tools_INSERT ON tools(status)");
  EXPECT_THROW(StateStore failed(config.state_root), Error);
  EXPECT_EQ(raw.value("PRAGMA user_version"), "7");
  EXPECT_EQ(raw.value("SELECT count(*) FROM sqlite_master WHERE name='session_revisions'"), "0");
  raw.exec("DROP INDEX session_revision_tools_INSERT");
  EXPECT_NO_THROW(StateStore upgraded(config.state_root));
  EXPECT_EQ(raw.value("PRAGMA user_version"), "8");
}
TEST_F(Pr24, RecoveredPositionsPersistWithAppendAndRevisionInBothStorageFormats) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool blob : {false, true})
    for (bool insert : {false, true})
      for (bool revise : {false, true}) {
        SCOPED_TRACE(std::to_string(blob) + std::to_string(insert) + std::to_string(revise));
        auto s = shifted(state, tools, raw, blob, insert);
        ASSERT_EQ(s.messages.size(), 3u);
        ASSERT_EQ(s.messages[1].role, "tool");
        const auto checkpoint_id = s.messages[2].message_id;
        if (revise) {
          s.messages[2].text = "requested checkpoint revision";
          ASSERT_NO_THROW(state.revise_messages(s, {2}));
        } else {
          s.messages.push_back(codet::Message{"user", "new suffix"});
          ASSERT_NO_THROW(state.append_messages(s, 3));
        }
        const auto loaded = state.load_id(s.id, false);
        EXPECT_EQ(Json(loaded.messages), Json(s.messages));
        EXPECT_EQ(loaded.messages[2].message_id, checkpoint_id);
        EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + s.id + "'"), "1");
        EXPECT_EQ(raw.value("SELECT result FROM tools WHERE session='" + s.id + "'"),
                  raw.value("SELECT payload FROM session_messages WHERE session='" + s.id +
                            "' AND position=1"));
        EXPECT_THAT(
            state.references(s.id),
            Contains(s.messages[2].checkpoint_state.at("recovery_record").get<std::string>()));
        EXPECT_NO_THROW(append(state, s));
        EXPECT_NO_THROW(state.reconcile(s));
        auto child = state.fork(s);
        EXPECT_EQ(Json(child.messages), Json(s.messages));
      }
}
TEST_F(Pr24, RecoveredWritesRejectGenuineSourceAndUnrequestedCallerEdits) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool external : {false, true}) {
    auto s = shifted(state, tools, raw, false, true);
    const auto projection = s.checkpoint_projection;
    if (external) {
      auto changed = Json::parse(raw.value("SELECT payload FROM session_messages WHERE session='" +
                                           s.id + "' AND position=1"));
      changed["text"] = "external edit";
      raw.exec("UPDATE session_messages SET payload='" + changed.dump() + "' WHERE session='" +
               s.id + "' AND position=1");
    } else
      s.messages[2].text = "unrequested edit";
    s.messages.push_back(codet::Message{"user", "new suffix"});
    EXPECT_THROW(state.append_messages(s, 3), Error);
    EXPECT_EQ(s.checkpoint_projection, projection);
    EXPECT_EQ(raw.value("SELECT count(*) FROM session_messages WHERE session='" + s.id + "'"), "2");
    EXPECT_EQ(raw.value("SELECT count(*) FROM tools WHERE session='" + s.id + "'"), "0");
  }
}
TEST_F(Pr24, RecoveryRollbackRetainsProvenanceForRetryBeforeReload) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool blob : {false, true}) {
    auto s = shifted(state, tools, raw, blob, true);
    const auto projection = s.checkpoint_projection;
    s.messages.push_back(codet::Message{"user", "suffix"});
    const auto before = Json(s.messages);
    raw.exec("CREATE TABLE parent(id PRIMARY KEY);CREATE TABLE poison(id REFERENCES parent(id) "
             "DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER deferred AFTER INSERT ON "
             "session_messages BEGIN INSERT INTO poison VALUES(1); END");
    EXPECT_THROW(state.append_messages(s, 3), Error);
    EXPECT_EQ(Json(s.messages), before);
    EXPECT_EQ(s.checkpoint_projection, projection);
    EXPECT_EQ(raw.value("SELECT count(*) FROM tools WHERE session='" + s.id + "'"), "0");
    EXPECT_EQ(raw.value("SELECT message_format FROM sessions WHERE id='" + s.id + "'"),
              blob ? "0" : "1");
    raw.exec("DROP TRIGGER deferred;DROP TABLE poison;DROP TABLE parent");
    EXPECT_NO_THROW(state.append_messages(s, 3));
    EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(s.messages));
  }
}
TEST_F(Pr24, FailedToolCompletionKeepsOriginalErrorAndPrivateReceiptWithoutReplay) {
  for (bool receipt_failure : {false, true}) {
    config.state_root = temp.root / (receipt_failure ? "failed-receipt" : "receipt");
    ToolReceiptFiles injected;
    injected.fail_receipt = receipt_failure;
    StateStore state(injected, config.state_root);
    Db raw(config.state_root / "state.sqlite3");
    auto s = state.create();
    ToolCall call{unique_id(), "exec_command", Json::object()};
    codet::Message assistant;
    assistant.role = "assistant";
    assistant.calls = {call};
    s.messages = {assistant};
    state.save(s);
    state.tool_started(s.id, call);
    raw.exec("CREATE TABLE parent(id PRIMARY KEY);CREATE TABLE poison(id REFERENCES parent(id) "
             "DEFERRABLE INITIALLY DEFERRED);CREATE TRIGGER deferred AFTER INSERT ON "
             "session_messages BEGIN INSERT INTO poison VALUES(1); END");
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = call.id;
    result.text = "completed output";
    std::string failure;
    try {
      state.complete_tool(s, call, result);
      FAIL();
    } catch (const Error &error) {
      failure = error.what();
    }
    EXPECT_THAT(failure, HasSubstr("FOREIGN KEY"));
    EXPECT_THAT(failure, Not(HasSubstr("receipt failure")));
    EXPECT_EQ(s.messages.size(), 1u);
    size_t receipts = 0;
    for (const auto &entry : fs::directory_iterator(config.state_root / "repairs")) {
      const auto receipt = Json::parse(injected.read(entry.path(), 1024 * 1024));
      EXPECT_EQ(receipt["kind"], "uncommitted_tool_result");
      EXPECT_EQ(receipt["message"]["text"], result.text);
      EXPECT_EQ(receipt["dispatch_allowed"], false);
      struct stat status{};
      ASSERT_EQ(::stat(entry.path().c_str(), &status), 0);
      EXPECT_EQ(status.st_mode & 0777, 0600);
      ++receipts;
    }
    EXPECT_EQ(receipts, receipt_failure ? 0u : 1u);
    EXPECT_EQ(raw.value("SELECT status FROM tools WHERE session='" + s.id + "'"), "running");
    raw.exec("DROP TRIGGER deferred");
    EXPECT_NO_THROW(state.complete_tool(s, call, result));
    EXPECT_EQ(state.load_id(s.id, false).messages.back().text, result.text);
  }
}
TEST_F(Pr24, ConstRecoveredSnapshotsCanRepeatAndAppendWithoutReload) {
  StateStore state(files, config.state_root, artifact_directory(config));
  Tools tools(config, files, processes, http, clock);
  Db raw(config.state_root / "state.sqlite3");
  for (bool blob : {false, true}) {
    auto s = shifted(state, tools, raw, blob, true);
    const Session &view = s;
    const auto projection = s.checkpoint_projection;
    s.messages[2].text = "explicit revision";
    EXPECT_NO_THROW(state.revise_messages(view, {2}));
    EXPECT_NO_THROW(state.revise_messages(view, {2}));
    for (int i = 0; i < 3; ++i) {
      const auto first = s.messages.size();
      codet::Message message{"user", "suffix"};

      s.messages.push_back(message);
      EXPECT_NO_THROW(state.append_messages(view, first));
    }
    EXPECT_EQ(s.checkpoint_projection, projection);
    auto canonical = s.messages;
    portable_messages(canonical, s.id);
    ensure_message_ids(canonical);
    EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(canonical));
    auto first = s.messages.size();
    s.messages.push_back(codet::Message{"user", "mutable handoff"});
    EXPECT_NO_THROW(state.append_messages(s, first));
    EXPECT_EQ(Json(state.load_id(s.id, false).messages), Json(s.messages));
  }
}
namespace {
struct VirtualToolReceiptFiles : RepairFiles {
  std::map<fs::path, std::string> receipts;
  bool readable = true;
  void atomic_write(const fs::path &p, const std::string &text, bool secret) override {
    if (p.filename().string().find("-tool-result-") != std::string::npos) {
      receipts[p] = text;
      throw Error("receipt published virtually");
    }
    RepairFiles::atomic_write(p, text, secret);
  }
  std::string read(const fs::path &p, size_t limit) const override {
    const auto found = receipts.find(p);
    if (found != receipts.end()) {
      if (!readable)
        throw Error("receipt verification unavailable");
      return found->second;
    }
    return RepairFiles::read(p, limit);
  }
};
} // namespace
TEST_F(Pr24, ToolReceiptDiagnosticsUseConfiguredPublicationVerification) {
  for (bool readable : {false, true}) {
    config.state_root = temp.root / (readable ? "confirmed" : "unconfirmed");
    VirtualToolReceiptFiles injected;
    injected.readable = readable;
    StateStore state(injected, config.state_root);
    auto s = state.create();
    ToolCall call{unique_id(), "exec_command", Json::object()};
    codet::Message assistant;
    assistant.role = "assistant";
    assistant.calls = {call};
    s.messages = {assistant};
    state.save(s);
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = call.id;
    result.text = "completed tool output";
    std::string failure;
    try {
      state.complete_tool(s, call, result);
      FAIL();
    } catch (const Error &error) {
      failure = error.what();
    }
    EXPECT_THAT(failure, HasSubstr("outcome is no longer pending"));
    EXPECT_THAT(failure, Not(HasSubstr("published virtually")));
    EXPECT_THAT(failure, HasSubstr(readable ? "uncommitted tool result preserved at"
                                            : "receipt candidate (publication unconfirmed)"));
    ASSERT_EQ(injected.receipts.size(), 1u);
    EXPECT_FALSE(fs::exists(injected.receipts.begin()->first));
    EXPECT_EQ(Json::parse(injected.receipts.begin()->second)["message"]["text"], result.text);
    EXPECT_EQ(s.messages.size(), 1u);
  }
}
TEST_F(Pr24, RecoveryMappingsSurviveArchiveLossAndRelocationWithoutAuthorizingReads) {
  std::string id, checkpoint_id, reference;
  {
    StateStore state(files, config.state_root, artifact_directory(config));
    Tools tools(config, files, processes, http, clock);
    Db raw(config.state_root / "state.sqlite3");
    auto s = shifted(state, tools, raw, false, true);
    id = s.id;
    checkpoint_id = s.messages[2].message_id;
    reference = s.messages[2].checkpoint_state.at("recovery_record").get<std::string>();
    fs::remove(artifact_directory(config) / reference);
    EXPECT_NO_THROW(append(state, s));
    auto facts = tools.execution_state(s.messages);
    EXPECT_THAT(facts.dump(), HasSubstr("recovery_unavailable"));
    EXPECT_EQ(s.messages[2].message_id, checkpoint_id);
  }
  const auto old_state = config.state_root, old_artifacts = artifact_directory(config);
  config.state_root = temp.root / "relocated-state";
  config.artifact_root = temp.root / "relocated-artifacts";
  fs::rename(old_state, config.state_root);
  const auto moved_artifacts = within(old_artifacts, old_state)
                                   ? config.state_root / old_artifacts.lexically_relative(old_state)
                                   : old_artifacts;
  fs::rename(moved_artifacts, config.artifact_root);
  StateStore relocated(files, config.state_root, config.artifact_root);
  Tools tools(config, files, processes, http, clock);
  auto loaded = relocated.load_id(id);
  EXPECT_EQ(loaded.messages[2].message_id, checkpoint_id);
  EXPECT_EQ(loaded.messages[2].checkpoint_state["recovery_record"], reference);
  EXPECT_NO_THROW(append(relocated, loaded));
  EXPECT_THAT(tools.execution_state(loaded.messages).dump(), HasSubstr("recovery_unavailable"));
}
