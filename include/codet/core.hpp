#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace codet {
using Json = nlohmann::json;
namespace fs = std::filesystem;
struct Error : std::runtime_error {
  int code;
  explicit Error(const std::string &message, int c = 1) : std::runtime_error(message), code(c) {}
};
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
  int timeout_seconds = 120;
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
  virtual std::string read(const fs::path &, size_t limit = size_t{16} * size_t{1024} * 1024) const = 0;
  virtual void atomic_write(const fs::path &, const std::string &, bool private_file = false) = 0;
  virtual void remove(const fs::path &) = 0;
  virtual fs::path canonical(const fs::path &) const = 0;
};
struct NativeFileSystem final : FileSystem {
  std::string read(const fs::path &, size_t = size_t{16} * size_t{1024} * 1024) const override;
  void atomic_write(const fs::path &, const std::string &, bool = false) override;
  void remove(const fs::path &) override;
  fs::path canonical(const fs::path &) const override;
};
struct ProcessSpec {
  std::vector<std::string> argv;
  fs::path cwd;
  std::map<std::string, std::string> env;
  int timeout_ms = 120000;
  size_t output_limit = 65536;
  bool pty = false;
};
struct ProcessResult {
  int exit_code = 0;
  std::string output;
  bool truncated = false, running = false;
};
struct Processes {
  virtual ~Processes() = default;
  virtual ProcessResult run(const ProcessSpec &, const Cancellation &) = 0;
  virtual std::string start(const ProcessSpec &) = 0;
  virtual ProcessResult poll(const std::string &) = 0;
  virtual void stop(const std::string &) = 0;
  virtual void write(const std::string &, const std::string &) {
    throw Error("Process input is unavailable");
  }
};
class ProcessControl;
class NativeProcesses final : public Processes {
  struct Job;
  std::map<std::string, std::unique_ptr<Job>> jobs_;
  std::shared_ptr<ProcessControl> control_;
  unsigned next_ = 0;
  void collect(Job &);
  void finish(Job &);

public:
  NativeProcesses();
  explicit NativeProcesses(std::shared_ptr<ProcessControl>);
  ~NativeProcesses() override;
  ProcessResult run(const ProcessSpec &, const Cancellation &) override;
  std::string start(const ProcessSpec &) override;
  ProcessResult poll(const std::string &) override;
  void stop(const std::string &) override;
  void write(const std::string &, const std::string &) override;
};
struct ToolCall {
  std::string id, name;
  Json arguments = Json::object();
};
struct Message {
  std::string role, text, tool_call_id;
  std::vector<ToolCall> calls;
  Json replay = Json::array();
  std::string replay_provider = "";
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
};
struct Config {
  fs::path home, cwd;
  std::string provider = "codex", model, permission_mode = "default", approval = "on-request";
  std::string ui = "auto", theme = "default", effort;
  size_t max_turns = 100, max_output_tokens = 8192, context_tokens = 32768;
  size_t tool_output_limit = 65536;
  int request_timeout = 120;
  std::map<std::string, ProviderConfig> providers;
  std::vector<std::string> allow, deny;
  std::vector<fs::path> roots;
  Json hooks = Json::object(), keybindings = Json::object();
  bool trust_project = false;
  // Invocation selection only: explicit profile/CLI fields, independent of base defaults.
  std::optional<std::string> selected_provider, selected_model;
};
Config load_config(const fs::path &home, const fs::path &cwd, const std::string &profile,
                   const std::vector<std::string> &overrides, FileSystem &);
std::string default_model(const Config &, const std::string &provider);
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
  Session load(const std::string &id_or_name);
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
};
// NOLINTNEXTLINE(performance-enum-size): Preserve the public permission decision's existing int ABI.
enum class Decision : int { allow, ask, deny };
class Permissions {
  const Config &config_;
  const FileSystem &files_;

public:
  Permissions(const Config &c, const FileSystem &f) : config_(c), files_(f) {}
  Decision decide(const PermissionRequest &) const;
};
using Approver = std::function<bool(const PermissionRequest &)>;
using Questioner =
    std::function<std::string(const std::string &, const std::vector<std::string> &)>;
struct ToolResult {
  std::string content;
  bool error = false;
  Json metadata = Json::object();
};
class Tools {
  const Config &config_;
  FileSystem &files_;
  Processes &processes_;
  HttpTransport &http_;
  Clock &clock_;
  Permissions permissions_;
  Approver approve_;
  Questioner question_;
  std::map<fs::path, std::string> read_versions_;
  Json todos_ = Json::array();
  bool plan_ = false;
  std::string session_id_;
  std::vector<std::string> task_ids_;
  const Cancellation *active_cancel_ = nullptr;
  fs::path path(const std::string &) const;
  void authorize(const PermissionRequest &);
  ToolResult execute_inner(const ToolCall &, const Cancellation &);

public:
  Tools(const Config &c, FileSystem &f, Processes &p, HttpTransport &h, Clock &clock,
        Approver a = {}, Questioner q = {})
      : config_(c), files_(f), processes_(p), http_(h), clock_(clock), permissions_(c, f),
        approve_(std::move(a)), question_(std::move(q)) {}
  Json schemas() const;
  void use_session(const std::string &id);
  ToolResult execute(const ToolCall &, const Cancellation &);
  std::string instructions() const;
  std::string expand_command(const std::string &) const;
  Json run_hooks(const std::string &, const Json &, const Cancellation &, int budget_ms = 0);
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

public:
  Agent(const Config &c, Provider &p, Tools &t, StateStore &s, EventSink e)
      : config_(c), provider_(p), tools_(t), state_(s), events_(std::move(e)) {}
  RunResult run(Session &, const std::string &, const Cancellation &, const Json &schema = Json());
};
int cli_main(const std::vector<std::string> &args, Cancellation &);
} // namespace codet
