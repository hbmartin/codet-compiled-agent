#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
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
  mutable size_t reads = 0;
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
    return native.canonical(p);
  }
  std::unique_ptr<ReadHandle> open_read(const fs::path &p) const override {
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
TEST_F(Pr18, ArchiveReadsAndBackupsAllowConcurrentCommitsAndRetryOnce) {
  for (const bool during_backup : {false, true}) {
    SCOPED_TRACE(during_backup);
    files.reads = 0;
    config.state_root = temp.root / (during_backup ? "backup" : "read");
    StateStore state(config.state_root);
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
    EXPECT_EQ(backups, 2u);
    EXPECT_EQ(report["backups"].size(), 2u);
    EXPECT_EQ(writer.value("SELECT status FROM sessions WHERE id='" + other.id + "'"),
              "concurrent");
    EXPECT_EQ(files.reads, 1u); // One decoded archive per Tools instance, including its retry.
    files.on_backup = {};
    files.on_read = {};
  }
}
TEST_F(Pr18, SecondInvalidationStopsWithoutRepairWritesAndListsBothBackups) {
  StateStore state(config.state_root);
  auto s = state.create(), other = state.create();
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
    writer.exec("UPDATE sessions SET updated=updated+1 WHERE id='" + other.id + "'");
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
    StateStore state(config.state_root); // Deliberately differs from Tools' artifact root.
    auto s = state.create(), other = state.create();
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
        raw.exec("UPDATE sessions SET updated=updated+1 WHERE id='" + other.id + "'");
      };
    Tools tools(config, files, processes, http, clock);
    EXPECT_THROW(state.repair(s.id, false, false, true, tools), Error);
    files.on_backup = {};
    EXPECT_EQ(archives(), 0u);
    EXPECT_EQ(state.load_id(s.id, false).messages[0].result_file, m.result_file);
  }
}
TEST_F(Pr18, CandidateCleanupProtectsExistingReferencedAndReplacedArchives) {
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
    StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root, artifact_directory(config));
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
  raw.exec("UPDATE session_messages SET payload='" + Json(s.messages[0]).dump() +
           "' WHERE session='" + s.id +
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
  StateStore state(config.state_root, artifact_directory(config));
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
  StateStore state(config.state_root);
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
