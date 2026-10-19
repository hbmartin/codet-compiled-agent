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
struct RepairFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t reads = 0;
  mutable std::function<void()> on_read;
  std::function<void()> on_backup;
  std::string read(const fs::path &p, size_t n) const override {
    if (p.string().find("/results/") != std::string::npos) {
      ++reads;
      if (on_read)
        on_read();
    }
    return native.read(p, n);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    native.atomic_write(p, s, secret);
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
  EXPECT_EQ(tools.checked_result_reference(path.string()).first, capture);
  EXPECT_EQ(tools.checked_result_reference(capture).first, capture);
  fs::remove(path);
  fs::create_symlink(config.cwd, path);
  EXPECT_THROW(tools.checked_result_reference(capture), Error);
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
  EXPECT_EQ(tools.checked_result_reference(path.string()).first, ref.string());
  EXPECT_NO_THROW(checked_artifact_identity(files, artifact_directory(config), path,
                                            ArtifactPolicy::owned_regular));
  EXPECT_THROW(checked_artifact_identity(files, artifact_directory(config), path,
                                         ArtifactPolicy::private_owned_regular),
               Error);
  ASSERT_EQ(::chmod(path.c_str(), 0), 0);
  if (::geteuid() != 0)
    EXPECT_THROW(tools.checked_result_reference(ref.string()), Error);
  ASSERT_EQ(::chmod(path.c_str(), 0600), 0);
  if (::geteuid() == 0) {
    ASSERT_EQ(::chown(path.c_str(), 12345, 12345), 0);
    EXPECT_THROW(tools.checked_result_reference(ref.string()), Error);
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
    EXPECT_EQ(files.reads, during_backup ? 2u : 1u); // One decoded archive per Tools instance.
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
