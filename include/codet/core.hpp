#pragma once
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace codet {
using Json = nlohmann::json;
namespace fs = std::filesystem;
std::string safe_text(const std::string &);
struct Error : std::runtime_error {
  int code;
  explicit Error(const std::string &message, int c = 1)
      : std::runtime_error(safe_text(message)), code(c) {}
};
struct ProcessTimeout : Error {
  ProcessTimeout() : Error("Command timed out during startup") {}
};
Json safe_json(const Json &);
using DiagnosticSink = std::function<void(const std::string &)>;
void report_diagnostic(const DiagnosticSink &, const std::string &);
std::string bounded_text(const std::string &, size_t);
struct Cancellation {
  std::atomic<bool> requested{false};
  void check() const {
    if (requested.load())
      throw Error("Interrupted", 130);
  }
};
struct Clock {
  virtual ~Clock() = default;
  virtual int64_t seconds() const = 0;
  virtual void sleep_ms(int ms, const Cancellation &) = 0;
};
struct SystemClock final : Clock {
  int64_t seconds() const override;
  void sleep_ms(int, const Cancellation &) override;
};
struct HttpRequest {
  std::string method = "POST", url, body;
  std::map<std::string, std::string> headers;
  int timeout_seconds = 120, connect_timeout_seconds = 20, idle_timeout_seconds = 0;
  size_t max_bytes = size_t{16} * size_t{1024} * 1024;
};
struct HttpResponse {
  int status = 0;
  std::string body;
};
using ChunkSink = std::function<void(const std::string &)>;
struct HttpTransport {
  virtual ~HttpTransport() = default;
  virtual HttpResponse perform(const HttpRequest &, const Cancellation &,
                               const ChunkSink & = {}) = 0;
};
struct CurlTransport final : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink & = {}) override;
};
struct FileSystem {
  virtual ~FileSystem() = default;
  virtual std::string read(const fs::path &,
                           size_t limit = size_t{16} * size_t{1024} * 1024) const = 0;
  virtual std::string read_prefix(const fs::path &p, size_t limit, bool &truncated) const {
    auto text = read(p, limit);
    truncated = false;
    return text;
  }
  virtual void atomic_write(const fs::path &, const std::string &, bool private_file = false) = 0;
  virtual void remove(const fs::path &) = 0;
  virtual fs::path canonical(const fs::path &) const = 0;
};
struct NativeFileSystem final : FileSystem {
  std::string read(const fs::path &, size_t = size_t{16} * size_t{1024} * 1024) const override;
  std::string read_prefix(const fs::path &, size_t, bool &) const override;
  void atomic_write(const fs::path &, const std::string &, bool = false) override;
  void remove(const fs::path &) override;
  fs::path canonical(const fs::path &) const override;
};
class CaptureStore;
class OutputCapture;
struct OutputSnapshot;
struct ProcessSpec {
  std::vector<std::string> argv;
  fs::path cwd;
  std::map<std::string, std::string> env;
  int timeout_ms = 120000;
  size_t output_limit = 65536;
  bool pty = false, separate_stderr = false, interactive_stdin = false;
  fs::path spool_directory;
  std::shared_ptr<CaptureStore> capture_store;
  size_t spool_limit = size_t{8} * 1024 * 1024, spool_quota = size_t{64} * 1024 * 1024;
  size_t spool_records = 64;
};
struct ProcessResult {
  int exit_code = 0;
  std::string output;
  bool truncated = false, running = false;
  std::string stderr_output;
  std::string spill_path, capture_error;
  uint64_t total_output_bytes = 0;
  size_t spill_bytes = 0;
  bool spill_truncated = false;
  bool stdout_truncated = false, stderr_truncated = false;
  std::shared_ptr<const OutputSnapshot> preview;
  std::shared_ptr<OutputCapture> capture;
  ProcessResult() = default;
  ProcessResult(int code, std::string text, bool cut = false, bool active = false,
                std::string errors = {})
      : exit_code(code), output(std::move(text)), truncated(cut), running(active),
        stderr_output(std::move(errors)) {}
};
struct ProcessStatus {
  std::string id;
  bool running = false;
  int exit_code = 0;
  std::string error = "";
};
struct Processes {
  virtual ~Processes() = default;
  virtual ProcessResult run(const ProcessSpec &, const Cancellation &) = 0;
  virtual std::string start(const ProcessSpec &) = 0;
  virtual std::string start(const ProcessSpec &s, const Cancellation &c) {
    c.check();
    return start(s);
  }
  virtual std::vector<ProcessStatus> list() {
    return {};
  }
  virtual ProcessResult poll(const std::string &) = 0;
  virtual ProcessStatus status(const std::string &id) {
    const auto result = poll(id);
    return {id, result.running, result.exit_code, ""};
  }
  virtual ProcessResult read_output(const std::string &id) {
    return poll(id);
  }
  virtual ProcessResult take_output(const std::string &id) {
    return read_output(id);
  }
  virtual void stop(const std::string &) = 0;
  virtual void write(const std::string &, const std::string &) {
    throw Error("Process input is unavailable");
  }
  virtual void close_stdin(const std::string &) {
    throw Error("Closing stdin is unavailable");
  }
};
class ProcessControl;
class NativeProcesses final : public Processes {
  struct Job;
  std::map<std::string, std::unique_ptr<Job>> jobs_;
  std::shared_ptr<ProcessControl> control_;
  unsigned next_ = 0;
  uint64_t completed_ = 0;
  DiagnosticSink diagnostics_;
  void collect(Job &);
  void drain(Job &);
  void stop_owned(Job &);
  bool observe_cleanup(Job &, std::exception_ptr &);
  void clear_error(Job &);
  void record_error(const std::string &, Job &, const std::exception_ptr &);
  [[noreturn]] void throw_failure(const Job &) const;
  Job &observe(const std::string &);
  ProcessResult publish(Job &, bool render = true);
  ProcessResult consume(const std::string &, bool render);
  static size_t job_bytes(const Job &);
  void advance_cleanup(Job &);
  void finish(Job &, bool ownership_lost = false);
  void terminate(Job &, int status);
  void refresh();
  void prune();

public:
  NativeProcesses();
  explicit NativeProcesses(std::shared_ptr<ProcessControl>, DiagnosticSink = {});
  void set_diagnostics(DiagnosticSink sink) {
    diagnostics_ = std::move(sink);
  }
  ~NativeProcesses() override;
  void shutdown();
  size_t retained_output_bytes() const;
  ProcessResult run(const ProcessSpec &, const Cancellation &) override;
  std::string start(const ProcessSpec &) override;
  std::string start(const ProcessSpec &, const Cancellation &) override;
  std::vector<ProcessStatus> list() override;
  ProcessResult poll(const std::string &) override;
  ProcessStatus status(const std::string &) override;
  ProcessResult read_output(const std::string &) override;
  ProcessResult take_output(const std::string &) override;
  void stop(const std::string &) override;
  void write(const std::string &, const std::string &) override;
  void close_stdin(const std::string &) override;
};
struct ToolCall {
  std::string id, name;
  Json arguments = Json::object();
};
struct ImageArtifact {
  std::string path, mime = "image/png", sha256, detail = "high";
  size_t width = 0, height = 0;
};
void to_json(Json &, const ImageArtifact &);
void from_json(const Json &, ImageArtifact &);
struct Message {
  std::string role, text, tool_call_id;
  std::vector<ToolCall> calls;
  Json replay = Json::array();
  std::string replay_provider = "";
  std::vector<ImageArtifact> images;
};
void to_json(Json &, const ToolCall &);
void from_json(const Json &, ToolCall &);
void to_json(Json &, const Message &);
void from_json(const Json &, Message &);
struct Usage {
  int64_t input = 0, output = 0, cached = 0, reasoning = 0;
};
struct ProviderResult {
  Message message;
  Usage usage;
  std::string stop_reason;
};
struct ProviderConfig {
  std::string api_key, base_url, model;
  std::optional<bool> image_input;
};
struct Selection {
  std::string provider, model;
};
struct PermissionRuleSet {
  std::vector<std::string> allow, deny;
};
struct Config {
  fs::path home, cwd, capture_root, artifact_root;
  std::string provider = "codex", model, permission_mode = "default", approval = "on-request";
  std::string ui = "auto", theme = "default", effort;
  size_t max_turns = 100, max_output_tokens = 8192, context_tokens = 32768;
  size_t tool_output_limit = 65536;
  int request_timeout = 0, connect_timeout = 20, stream_idle_timeout = 120;
  size_t project_doc_max_bytes = 32768, memory_context_max_bytes = 8900;
  std::map<std::string, ProviderConfig> providers;

private:
  std::shared_ptr<const PermissionRuleSet> rules_ = std::make_shared<const PermissionRuleSet>();

public:
  const PermissionRuleSet &rules() const {
    return *rules_;
  }
  const std::shared_ptr<const PermissionRuleSet> &rule_snapshot() const {
    return rules_;
  }
  void set_rules(std::vector<std::string> allow, std::vector<std::string> deny) {
    rules_ = std::make_shared<const PermissionRuleSet>(
        PermissionRuleSet{std::move(allow), std::move(deny)});
  }
  void set_allow(std::vector<std::string> values) {
    rules_ = std::make_shared<const PermissionRuleSet>(
        PermissionRuleSet{std::move(values), rules_->deny});
  }
  void set_deny(std::vector<std::string> values) {
    rules_ = std::make_shared<const PermissionRuleSet>(
        PermissionRuleSet{rules_->allow, std::move(values)});
  }
  void add_allowed(std::string value) {
    auto values = rules_->allow;
    values.push_back(std::move(value));
    set_allow(std::move(values));
  }
  void add_denied(std::string value) {
    auto values = rules_->deny;
    values.push_back(std::move(value));
    set_deny(std::move(values));
  }
  std::vector<fs::path> roots;
  Json hooks = Json::object(), keybindings = Json::object();
  bool trust_project = false;
  // Invocation selection only: explicit profile/CLI fields, independent of base defaults.
  std::optional<std::string> selected_provider, selected_model;
  Selection configured{"codex", ""};
};
Config load_config(const fs::path &home, const fs::path &cwd, const std::string &profile,
                   const std::vector<std::string> &overrides, FileSystem &);
std::string default_model(const Config &, const std::string &provider);
Selection resolve_selection(const Config &, const std::optional<Selection> &saved = std::nullopt);
bool image_input_enabled(const Config &);
std::string env(const char *);
std::string unique_id();
bool within(const fs::path &, const fs::path &);
std::string url_encode(const std::string &);
class CredentialStore {
  fs::path home_;
  FileSystem &files_;
  Clock &clock_;

public:
  CredentialStore(fs::path home, FileSystem &files, Clock &clock)
      : home_(std::move(home)), files_(files), clock_(clock) {}
  void import(const fs::path &);
  void save_key(const std::string &provider, const std::string &key);
  Json load() const;
  std::string key(const std::string &) const;
  Json codex(HttpTransport &, const Cancellation &);
};
struct Provider {
  virtual ~Provider() = default;
  virtual ProviderResult complete(const Config &, const std::string &system,
                                  const std::vector<Message> &, const Json &tools,
                                  const Cancellation &, const ChunkSink &) = 0;
};
class SseDecoder {
  std::string pending_, event_, data_;
  std::function<void(const std::string &, const std::string &)> sink_;
  void line(std::string);

public:
  explicit SseDecoder(std::function<void(const std::string &, const std::string &)> sink)
      : sink_(std::move(sink)) {}
  void feed(const std::string &);
  void finish();
};
class ApiProvider final : public Provider {
  HttpTransport &http_;
  CredentialStore &credentials_;
  Clock &clock_;

public:
  ApiProvider(HttpTransport &h, CredentialStore &c, Clock &clock)
      : http_(h), credentials_(c), clock_(clock) {}
  HttpRequest request(const Config &, const std::string &, const std::vector<Message> &,
                      const Json &, const Json &auth = Json::object()) const;
  ProviderResult complete(const Config &, const std::string &, const std::vector<Message> &,
                          const Json &, const Cancellation &, const ChunkSink &) override;
};
struct Session {
  std::string id, name;
  std::vector<Message> messages;
  std::string provider = "", model = "";
};
class StateStore {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  explicit StateStore(const fs::path &);
  ~StateStore();
  Session create(const std::string &name = "");
  // Default loads are repaired snapshots, never persistent recovery writes.
  Session load(const std::string &id_or_name, bool repair = true);
  Session load_id(const std::string &id, bool repair = true);
  std::shared_ptr<void> acquire(const std::string &id);
  void reconcile(Session &, bool persist = true);
  Session last();
  Session fork(const Session &);
  void save(const Session &);
  Json list();
  void begin_turn(const std::string &);
  void finish_turn(const std::string &, const std::string &);
  void tool_started(const std::string &, const ToolCall &);
  void tool_finished(const std::string &, const ToolCall &, const Json &);
};
struct PermissionRequest {
  std::string tool, target;
  std::optional<fs::path> path;
  bool mutation = false;
  Json context = Json::object();
};
enum class Decision : uint8_t { allow, ask, deny };
class Permissions {
  struct Rules;
  mutable std::shared_ptr<Rules> rules_;
  void refresh_rules() const;
  const Config &config_;
  const FileSystem &files_;

public:
  Permissions(const Config &c, const FileSystem &f) : config_(c), files_(f) {}
  struct Context;
  std::shared_ptr<Context> context() const;
  Decision decide(const PermissionRequest &) const;
  Decision decide(const PermissionRequest &, const std::shared_ptr<Context> &) const;
  bool has_scoped_denials(const std::string &tool) const;
};
using Approver = std::function<bool(const PermissionRequest &)>;
using Questioner =
    std::function<std::string(const std::string &, const std::vector<std::string> &)>;
struct ToolResult {
  std::string content;
  bool error = false;
  Json metadata = Json::object();
  std::vector<ImageArtifact> images;
  ToolResult() = default;
  ToolResult(std::string text, bool failed = false, Json data = Json::object(),
             std::vector<ImageArtifact> attachments = {})
      : content(std::move(text)), error(failed), metadata(std::move(data)),
        images(std::move(attachments)) {}
  ToolResult(std::string text, bool failed, Json data, std::shared_ptr<const OutputSnapshot> output,
             std::shared_ptr<OutputCapture> store)
      : content(std::move(text)), error(failed), metadata(std::move(data)),
        preview(std::move(output)), capture(std::move(store)) {}
  std::shared_ptr<const OutputSnapshot> preview = {};
  std::shared_ptr<OutputCapture> capture = {};
  // Transient: the agent journals the completed result before propagating this failure.
  std::exception_ptr failure_after_completion;
};
class Interaction;
class ComputerBackend;
class ImageBackend;
struct HookOutcome {
  bool blocked = false, stopped = false;
  std::string reason, additional_context;
  std::optional<Json> updated_input;
  std::optional<Decision> permission;
  std::vector<std::string> diagnostics;
};
class DiagnosticBuffer;
class Tools {
  const Config &config_;
  FileSystem &files_;
  Processes &processes_;
  Clock &clock_;
  Permissions permissions_;
  Approver approve_;
  Questioner question_;
  DiagnosticSink diagnostics_;
  std::shared_ptr<CaptureStore> capture_store_;
  Json todos_ = Json::array();
  std::string session_id_;
  mutable std::set<std::string> warnings_;
  const Cancellation *active_cancel_ = nullptr;
  std::shared_ptr<Interaction> interaction_;
  std::shared_ptr<ComputerBackend> computer_;
  std::shared_ptr<ImageBackend> images_;
  struct ExecSession {
    std::string process, command;
    fs::path cwd;
    std::string shell;
    bool pty = false;
    bool completed = false;
    std::optional<int> exit_code;
  };
  std::map<int, ExecSession> exec_sessions_;
  fs::path path(const std::string &) const;
  void authorize(const PermissionRequest &);
  PermissionRequest execution_request(const ToolCall &) const;
  Json authored_hook_input(const ToolCall &) const;
  ToolResult execute_inner(const ToolCall &, const PermissionRequest &, const Cancellation &);

public:
  Tools(const Config &c, FileSystem &f, Processes &p, HttpTransport &h, Clock &clock,
        Approver a = {}, Questioner q = {}, DiagnosticSink diagnostics = {},
        std::shared_ptr<Interaction> interaction = {},
        std::shared_ptr<ComputerBackend> computer = {}, std::shared_ptr<ImageBackend> images = {});
  Json schemas() const;
  Json schemas(const Json &capabilities) const;
  void use_session(const std::string &id);
  ToolResult execute(const ToolCall &, const Cancellation &);
  std::string instructions() const;
  std::string expand_command(const std::string &) const;
  std::vector<std::string> take_input();
  void begin_input_turn(const std::string &);
  void end_input_turn();
  void cancel_exec() noexcept;
  Json capabilities() const;
  HookOutcome run_hooks(const std::string &, const Json &, const Cancellation &, int budget_ms = 0,
                        DiagnosticBuffer *diagnostics = nullptr);
};
using EventSink = std::function<void(const Json &)>;
struct RunResult {
  int exit_code = 0;
  std::string text;
  Usage usage;
  Json structured;
};
class Agent {
  const Config &config_;
  Provider &provider_;
  Tools &tools_;
  StateStore &state_;
  EventSink events_;
  DiagnosticSink diagnostics_;

public:
  Agent(const Config &c, Provider &p, Tools &t, StateStore &s, EventSink e,
        DiagnosticSink diagnostics = {})
      : config_(c), provider_(p), tools_(t), state_(s), events_(std::move(e)),
        diagnostics_(std::move(diagnostics)) {}
  RunResult run(Session &, const std::string &, const Cancellation &, const Json &schema = Json());
};
int cli_main(const std::vector<std::string> &args, Cancellation &);
} // namespace codet
