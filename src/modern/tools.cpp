#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/interaction.hpp"
#include "codet/output.hpp"
#include "codet/patch.hpp"
#include "codet/process_control.hpp"
#include "codet/support.hpp"
#include "codet/tool_registry.hpp"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fnmatch.h>
#include <iostream>
#include <limits>
#include <nlohmann/json-schema.hpp>
#include <set>
#include <sstream>
#include <sys/stat.h>

namespace codet {
namespace {
std::string default_shell() {
  const auto shell = env("SHELL");
  return shell.empty() ? "/bin/sh" : shell;
}
void reject_nul_strings(const Json &value) {
  if (value.is_string() && value.get_ref<const std::string &>().find('\0') != std::string::npos)
    throw Error("Computer arguments must not contain NUL");
  if (value.is_structured())
    for (const auto &child : value)
      reject_nul_strings(child);
}
bool literal_shell_command(const std::string &command) {
  if (command.empty())
    return false;
  if (command.find_first_of(";&|`$()<>\\\"'*?[]{}~#") != std::string::npos)
    return false;
  for (unsigned char ch : command)
    if (ch < 32 || ch == 127)
      return false;
  std::istringstream words(command);
  std::string first;
  words >> first;
  static const std::set<std::string> reserved{
      "!",  "if",   "then", "else", "elif", "fi",       "for",  "while",  "until",
      "do", "done", "case", "esac", "in",   "function", "time", "select", "coproc"};
  return !first.empty() && !reserved.count(first) && first.find('=') == std::string::npos;
}
struct ParsedRule {
  std::string tool, pattern;
  bool scoped = false, valid = true;
  explicit ParsedRule(const std::string &text) {
    const auto open = text.find('(');
    scoped = open != std::string::npos;
    tool = text.substr(0, open);
    valid = !scoped || text.back() == ')';
    if (scoped && valid)
      pattern = text.substr(open + 1, text.size() - open - 2);
  }
  bool applies(const std::string &name) const {
    return valid && !fnmatch(tool.c_str(), name.c_str(), 0);
  }
};
std::string literal_glob(const std::string &text) {
  std::string result;
  for (char ch : text) {
    if (ch == '*' || ch == '?' || ch == '[' || ch == ']' || ch == '\\')
      result += '\\';
    result += ch;
  }
  return result;
}
std::string unescape_glob(const std::string &text) {
  std::string result;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size())
      ++i;
    result += text[i];
  }
  return result;
}
size_t first_wildcard(const std::string &text) {
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size())
      ++i;
    else if (text[i] == '*' || text[i] == '?' || text[i] == '[')
      return i;
  }
  return std::string::npos;
}
struct RulePaths {
  const Config &config;
  const FileSystem &files;
  std::map<fs::path, fs::path> canonical_cache;
  fs::path requested, target;
  std::vector<fs::path> literal_roots;
  std::map<std::string, std::vector<std::string>> patterns;
  void reset() {
    resolved = false;
    requested.clear();
    target.clear();
  }
  bool resolved = false;
  RulePaths(const Config &c, const FileSystem &f) : config(c), files(f) {}
  fs::path canonical(const fs::path &p) {
    const auto found = canonical_cache.find(p);
    if (found != canonical_cache.end())
      return found->second;
    return canonical_cache.emplace(p, files.canonical(p)).first->second;
  }
  const fs::path &resolve(const PermissionRequest &r) {
    if (!r.path)
      throw Error("Permission path is required");
    if (!resolved) {
      requested = (fs::absolute(config.cwd) / r.path.value()).lexically_normal();
      target = canonical(requested);
      resolved = true;
    }
    return target;
  }
  bool matches(const ParsedRule &rule, const PermissionRequest &r) {
    if (!rule.applies(r.tool))
      return false;
    if (!rule.scoped)
      return true;
    if (!r.path)
      return !fnmatch(rule.pattern.c_str(), r.target.c_str(), 0);
    resolve(r);
    const auto cached = patterns.find(rule.pattern);
    const auto matches_candidates = [&](const std::vector<std::string> &values) {
      return std::any_of(values.begin(), values.end(), [&](const auto &candidate) {
        return !fnmatch(candidate.c_str(), r.target.c_str(), 0) ||
               !fnmatch(candidate.c_str(), requested.c_str(), 0) ||
               !fnmatch(candidate.c_str(), target.c_str(), 0);
      });
    };
    if (cached != patterns.end()) {
      return matches_candidates(cached->second);
    }
    auto pattern = fs::path(rule.pattern).lexically_normal().string();
    auto base = fs::absolute(config.cwd).lexically_normal();
    // Configured filesystem roots are literal even when their names contain glob syntax.
    if (literal_roots.empty()) {
      literal_roots = std::vector<fs::path>{base, canonical(base)};
      for (const auto &root : config.roots) {
        literal_roots.push_back(fs::absolute(root).lexically_normal());
        literal_roots.push_back(canonical(root));
      }
      std::sort(literal_roots.begin(), literal_roots.end(),
                [](const auto &a, const auto &b) { return a.string().size() > b.string().size(); });
    }
    bool known_prefix = false;
    for (const auto &root : literal_roots) {
      const auto prefix = root.string();
      if (pattern == prefix ||
          (pattern.size() > prefix.size() && pattern.compare(0, prefix.size(), prefix) == 0 &&
           (prefix == "/" || pattern[prefix.size()] == '/'))) {
        known_prefix = true;
        base = root;
        pattern = pattern.size() == prefix.size()
                      ? ""
                      : pattern.substr(prefix.size() + (prefix == "/" ? 0 : 1));
        break;
      }
    }
    if (!known_prefix && fs::path(pattern).is_absolute()) {
      // Recognize filesystem-equivalent root aliases literally, including metacharacters.
      for (auto alias = fs::path(pattern); !alias.empty();) {
        if (std::any_of(literal_roots.begin(), literal_roots.end(),
                        [&](const auto &root) { return canonical(alias) == canonical(root); })) {
          known_prefix = true;
          base = alias;
          const auto prefix_size = alias.string().size();
          pattern = pattern.size() == prefix_size
                        ? ""
                        : pattern.substr(prefix_size + (alias == alias.root_path() ? 0 : 1));
          break;
        }
        auto parent = alias.parent_path();
        if (parent == alias)
          break;
        alias = parent;
      }
    }
    std::vector<std::string> candidates;
    // Unanchored authored globs may match absolute aliases outside cwd. Known filesystem
    // prefixes must never participate in raw glob matching (including allow rules).
    if (!known_prefix)
      candidates.push_back(pattern);
    const auto wildcard = first_wildcard(pattern);
    fs::path prefix;
    std::string suffix;
    if (wildcard == std::string::npos)
      prefix = (base / fs::path(unescape_glob(pattern))).lexically_normal();
    else {
      const auto slash = pattern.rfind('/', wildcard);
      prefix =
          (base /
           fs::path(unescape_glob(slash == std::string::npos ? "" : pattern.substr(0, slash + 1))))
              .lexically_normal();
      suffix = pattern.substr(slash == std::string::npos ? 0 : slash + 1);
    }
    for (const auto &fixed : {prefix, canonical(prefix)}) {
      auto candidate = literal_glob(fixed.string());
      if (!suffix.empty()) {
        if (candidate.empty() || candidate.back() != '/')
          candidate += '/';
        candidate += suffix;
      }
      candidates.push_back(std::move(candidate));
    }
    patterns.emplace(rule.pattern, candidates);
    return matches_candidates(candidates);
  }
};
std::vector<std::string> lines(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream in(s);
  std::string l;
  while (std::getline(in, l))
    out.push_back(l);
  return out;
}
struct Change {
  fs::path path;
  std::optional<std::string> before, after;
  mode_t mode = 0644;
};
std::string feedback_text(const std::string &text, size_t limit) {
  auto clean = safe_text(text);
  if (clean.size() <= limit)
    return clean;
  const std::string marker = "\n[Hook feedback truncated]";
  if (limit < marker.size())
    return bounded_text(marker, limit);
  return bounded_text(clean, limit - marker.size()) + marker;
}
RenderedOutput annotated(const OutputSnapshot &data, size_t limit, const std::string &prefix,
                         const std::string &suffix, const std::string &notice) {
  const auto rendered = render_output(data, limit - prefix.size() - suffix.size() - notice.size());
  return {prefix + rendered.text + suffix + notice, rendered.omitted};
}
} // namespace
struct Permissions::Rules {
  std::shared_ptr<const PermissionRuleSet> source;
  std::vector<ParsedRule> allow, deny;
};
void Permissions::refresh_rules() const {
  if (rules_ && rules_->source == config_.rule_snapshot())
    return;
  auto next = std::make_shared<Rules>();
  next->source = config_.rule_snapshot();
  for (const auto &rule : config_.rules().allow)
    next->allow.emplace_back(rule);
  for (const auto &rule : config_.rules().deny)
    next->deny.emplace_back(rule);
  rules_ = std::move(next);
}
bool Permissions::has_scoped_denials(const std::string &tool) const {
  refresh_rules();
  return std::any_of(rules_->deny.begin(), rules_->deny.end(),
                     [&](const auto &rule) { return rule.scoped && rule.applies(tool); });
}
struct Permissions::Context {
  RulePaths paths;
  std::shared_ptr<const PermissionRuleSet> source;
  Context(const Config &c, const FileSystem &f) : paths(c, f), source(c.rule_snapshot()) {}
};
std::shared_ptr<Permissions::Context> Permissions::context() const {
  return std::make_shared<Context>(config_, files_);
}
Decision Permissions::decide(const PermissionRequest &r) const {
  return decide(r, context());
}
Decision Permissions::decide(const PermissionRequest &r,
                             const std::shared_ptr<Context> &ctx) const {
  refresh_rules();
  auto &paths = ctx->paths;
  if (ctx->source != config_.rule_snapshot()) {
    paths.canonical_cache.clear();
    paths.literal_roots.clear();
    paths.patterns.clear();
    ctx->source = config_.rule_snapshot();
  }
  paths.reset();
  for (const auto &rule : rules_->deny)
    if (paths.matches(rule, r))
      return Decision::deny;
  if (config_.permission_mode == "plan" && r.mutation)
    return Decision::deny;
  if (config_.permission_mode == "bypass")
    return Decision::allow;
  if (r.tool == "exec_command" && r.context.contains("cwd")) {
    const auto cwd = paths.canonical(r.context.at("cwd").get<std::string>());
    if (std::none_of(config_.roots.begin(), config_.roots.end(),
                     [&](const auto &root) { return within(cwd, paths.canonical(root)); }))
      return Decision::deny;
  }
  if (r.path) {
    const auto &p = paths.resolve(r);
    bool found = false;
    for (const auto &root : config_.roots)
      if (within(p, paths.canonical(root)))
        found = true;
    const auto spill = capture_directory(config_);
    if (r.tool == "read_file" && !fs::is_symlink(spill) && within(p, paths.canonical(spill)))
      found = true;
    if (!found &&
        !(r.internal_read && !r.mutation && (r.tool == "read_file" || r.tool == "view_image")))
      return Decision::deny;
  }
  for (const auto &rule : rules_->allow)
    if (paths.matches(rule, r) &&
        (!rule.scoped || r.tool != "exec_command" ||
         (literal_shell_command(r.target) && !r.context.value("custom_shell", false))))
      return Decision::allow;
  if (!r.mutation)
    return Decision::allow;
  if (config_.permission_mode == "accept-edits" && r.tool == "apply_patch")
    return Decision::allow;
  return Decision::ask;
}
fs::path Tools::path(const std::string &value) const {
  if (value.empty())
    throw Error("Path cannot be empty");
  return files_.canonical(config_.cwd / fs::path(value));
}
void Tools::authorize(const PermissionRequest &r) {
  const auto d = permissions_.decide(r);
  if (d == Decision::deny)
    throw Error("Permission denied for " + r.tool + ": " + r.target);
  if (d == Decision::ask && active_cancel_ && config_.hooks.contains("PermissionRequest")) {
    const auto *definition = tool_definition(r.tool);
    const auto hook = run_hooks("PermissionRequest",
                                {{"hook_event_name", "PermissionRequest"},
                                 {"tool_name", definition ? definition->hook_name : r.tool},
                                 {"tool", r.tool},
                                 {"target", r.target},
                                 {"tool_input", r.context},
                                 {"cwd", r.context.value("cwd", config_.cwd.string())}},
                                *active_cancel_);
    if (hook.blocked || hook.stopped || hook.permission == Decision::deny)
      throw Error("Permission denied by hook: " + hook.reason);
    if (hook.permission == Decision::allow)
      return;
  }
  if (d == Decision::ask && (config_.approval == "never" || !approve_ || !approve_(r)))
    throw Error("Permission denied: " + r.tool +
                " requires approval. Do not retry without a policy change.");
}
Tools::Tools(const Config &c, FileSystem &f, Processes &p, HttpTransport &h, Clock &clock,
             Approver a, Questioner q, DiagnosticSink diagnostics,
             std::shared_ptr<Interaction> interaction, std::shared_ptr<ComputerBackend> computer,
             std::shared_ptr<ImageBackend> images)
    : config_(c), files_(f), processes_(p), clock_(clock), permissions_(c, f),
      approve_(std::move(a)), question_(std::move(q)), diagnostics_(std::move(diagnostics)),
      capture_store_(std::make_shared<CaptureStore>(capture_directory(c), size_t{8} * 1024 * 1024,
                                                    size_t{64} * 1024 * 1024, 64, diagnostics_)),
      interaction_(std::move(interaction)),
      computer_(computer ? std::move(computer) : native_computer()),
      images_(images ? std::move(images) : native_images()) {
  (void)h;
}
PermissionRequest Tools::execution_request(const ToolCall &call) const {
  const auto &a = call.arguments;
  const auto *definition = tool_definition(call.name);
  PermissionRequest request{call.name, "", std::nullopt, definition && definition->mutation};
  request.context = a;
  if (call.name == "read_file" || call.name == "view_image") {
    const auto value = a.at("path").get<std::string>();
    request.target = value;
    request.path = (config_.cwd / fs::path(value)).lexically_normal();
    const auto p = files_.canonical(*request.path);
    const auto root = artifact_directory(config_);
    if (call.name == "read_file" && !session_id_.empty()) {
      const auto index = artifact_index_path(root, session_id_);
      request.internal_read = !fs::is_symlink(root) && !fs::is_symlink(index.parent_path()) &&
                              !fs::is_symlink(*request.path) && p == files_.canonical(index);
    } else if (call.name == "view_image")
      request.internal_read = generated_image(config_, files_, p, false);
  } else if (call.name == "exec_command") {
    const auto configured_shell = default_shell();
    const auto shell = a.value("shell", configured_shell);
    const auto command = a.at("cmd").get<std::string>();
    if (command.empty() || command.size() > size_t{1024} * 1024 ||
        command.find('\0') != std::string::npos)
      throw Error("Invalid command length or embedded NUL");
    if (shell.empty() || shell.front() != '/' || shell.find('\0') != std::string::npos)
      throw Error("shell must be an absolute executable path without NUL");
    const auto authored_cwd = a.value("workdir", config_.cwd.string());
    if (authored_cwd.find('\0') != std::string::npos)
      throw Error("Working directory contains NUL");
    const auto cwd = files_.canonical(config_.cwd / fs::path(authored_cwd));
    if (!fs::is_directory(cwd))
      throw Error("Working directory does not exist");
    request.target = command;
    request.context = {{"command", command},
                       {"cwd", cwd.string()},
                       {"shell", shell},
                       {"login", a.value("login", true)},
                       {"tty", a.value("tty", false)},
                       {"custom_shell", shell != configured_shell}};
  } else if (call.name == "write_stdin") {
    const auto id = a.at("session_id").get<int>();
    const auto it = exec_sessions_.find(id);
    if (it == exec_sessions_.end() ||
        (it->second.completed && (!it->second.exit_code || !a.value("close_stdin", false) ||
                                  !a.value("chars", std::string{}).empty())))
      throw Error("Unknown or completed exec session");
    const auto &job = it->second;
    const auto chars = a.value("chars", std::string{});
    if (chars.find('\0') != std::string::npos)
      throw Error("stdin input must not contain NUL");
    const bool close = a.value("close_stdin", false);
    if (close && job.pty)
      throw Error("close_stdin is only supported for pipe sessions; use terminal input for PTYs");
    request.target = chars;
    request.mutation = !chars.empty() || close;
    request.context = {{"command", job.command}, {"cwd", job.cwd.string()}, {"shell", job.shell},
                       {"session_id", id},       {"chars", chars},          {"close_stdin", close}};
  } else if (call.name.rfind("computer.", 0) == 0) {
    reject_nul_strings(a);
    for (const auto *key : {"script", "text", "path", "url"})
      if (a.contains(key)) {
        request.target = a.at(key).get<std::string>();
        if (request.target.find('\0') != std::string::npos)
          throw Error("Computer permission targets must not contain NUL");
        return request;
      }
    request.target = a.dump();
  }
  return request;
}
void Tools::reference_results(const std::vector<Message> &messages) {
  const auto accept = [&](const std::string &value) {
    const fs::path p(value);
    if (!capture_name(p))
      return;
    const auto expected =
        fs::absolute(capture_directory(config_) / p.filename()).lexically_normal();
    if (p.is_absolute() || p == p.filename())
      referenced_results_.insert(expected);
  };
  for (const auto &message : messages) {
    if (message.role != "tool")
      continue;
    if (!message.result_file.empty())
      accept(message.result_file);
    const auto envelope = Json::parse(message.text, nullptr, false);
    if (!envelope.is_object())
      continue;
    const auto metadata = envelope.value("metadata", Json::object());
    if (metadata.is_object() && metadata.value("spill_path", Json()).is_string())
      accept(metadata.at("spill_path"));
  }
}
std::string Tools::result_reference(const Message &message) const {
  return (capture_directory(config_) / (message.result_file.empty()
                                            ? "capture-00000000-0000-0000-0000-000000000000.txt"
                                            : fs::path(message.result_file).filename().string()))
      .string();
}
std::string Tools::retain_result(Message &message) {
  if (message.result_file.empty()) {
    OutputCapture capture(capture_store_, 1);
    const auto payload = safe_json(Json(message)).dump();
    capture.append(payload);
    capture.retain();
    capture.finish();
    ProcessResult stored;
    capture.metadata(stored, payload.size());
    if (stored.spill_path.empty() || stored.spill_truncated || !stored.capture_error.empty())
      throw Error("Cannot retain full tool result for paging: " + stored.capture_error);
    message.result_file = fs::path(stored.spill_path).filename().string();
  }
  const auto path = result_reference(message);
  referenced_results_.insert(fs::absolute(path).lexically_normal());
  return path;
}
Json Tools::authored_hook_input(const ToolCall &call) const {
  if (call.name == "exec_command" && call.arguments.is_object()) {
    auto input = call.arguments;
    input["command"] = input.value("cmd", Json());
    input.erase("cmd");
    input["cwd"] = input.value("workdir", Json(config_.cwd.string()));
    input.erase("workdir");
    input["shell"] = input.value("shell", Json(default_shell()));
    return input;
  }
  return call.arguments;
}
Json Tools::capabilities() const {
  return {{"desktop", computer_->capabilities()}, {"image_input", image_input_enabled(config_)},
          {"image_codec", images_->available()},  {"interactive", bool(question_ || interaction_)},
          {"approvals", bool(approve_)},          {"questions", bool(question_)},
          {"async_input", bool(interaction_)},    {"os_sandbox", false}};
}
std::vector<std::string> Tools::take_input() {
  return interaction_ ? interaction_->take_input() : std::vector<std::string>{};
}
void Tools::begin_input_turn(const std::string &session) {
  if (interaction_)
    interaction_->begin_turn(session);
}
void Tools::end_input_turn() {
  if (interaction_)
    interaction_->end_turn();
}
void Tools::cancel_exec() noexcept {
  for (auto &entry : exec_sessions_)
    if (!entry.second.completed) {
      try {
        processes_.stop(entry.second.process);
      } catch (...) {
        std::fputs("Exec cleanup failed while stopping owned process\n", stderr);
      }
      entry.second.completed = true;
    }
}
Json Tools::schemas() const {
  return schemas(capabilities());
}
Json Tools::schemas(const Json &facts) const {
  Json out = Json::array();
  for (const auto &d : tool_registry())
    if (tool_available(d, config_, facts))
      out.push_back({{"name", d.name}, {"description", d.description}, {"input_schema", d.schema}});
  return out;
}
ToolResult Tools::execute(const ToolCall &call, const Cancellation &cancel) {
  struct Active {
    const Cancellation *&slot;
    const Cancellation *previous;
    ~Active() {
      slot = previous;
    }
  } active{active_cancel_, active_cancel_};
  active_cancel_ = &cancel;
  ToolCall effective = call;
  Json invocation = call.arguments;
  std::string invocation_cwd = config_.cwd.string();
  ToolResult completed;
  bool action_completed = false;
  size_t response_limit = config_.tool_output_limit;
  const bool exec_output = call.name == "exec_command" || call.name == "write_stdin";
  const auto preserve_completed = [&](const std::exception_ptr &failure,
                                      const std::string &message) {
    completed.error = true;
    completed.metadata["action_completed"] = true;
    completed.metadata["post_action_error"] = safe_text(message);
    const auto notice =
        feedback_text("Tool completed; post-action failure: " + message + "\n", response_limit / 3);
    if (completed.preview) {
      const auto rendered = annotated(*completed.preview, response_limit, notice, "", "");
      completed.content = rendered.text;
      completed.metadata["total_output_bytes"] = completed.preview->total;
      completed.metadata["omitted_output_bytes"] = rendered.omitted;
      completed.metadata["truncated"] = rendered.omitted != 0;
      if (exec_output && rendered.omitted)
        completed.metadata["original_token_count"] = (completed.preview->total + 3) / 4;
    } else
      completed.content = bounded_text(notice + completed.content, response_limit);
    completed.metadata = safe_json(completed.metadata);
    if (exec_output) {
      completed.metadata["output"] = completed.content;
      completed.content = completed.metadata.dump();
    }
    if (fatal(failure))
      completed.failure_after_completion = failure;
    return completed;
  };
  try {
    cancel.check();
    if (call.name == "Grep")
      throw Error(
          "Grep is retired; use exec_command to run system grep, subject to shell permissions");
    if (!call.arguments.is_object())
      throw Error("Tool arguments must be an object");
    const auto *definition = tool_definition(call.name);
    if (!definition || !tool_available(*definition, config_, capabilities()))
      throw Error("Unknown, removed, or unavailable tool: " + call.name);
    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(definition->schema);
    invocation = authored_hook_input(call);
    validator.validate(call.arguments);
    auto execution = execution_request(call);
    const auto preflight = [&](const ToolCall &candidate, const PermissionRequest &request) {
      const auto *d = tool_definition(candidate.name);
      auto check = [&](const std::string &value) {
        const auto requested = (config_.cwd / fs::path(value)).lexically_normal();
        if (permissions_.decide({candidate.name, value, requested, d->mutation}) == Decision::deny)
          throw Error("Permission denied for " + candidate.name + ": " + value);
      };
      if (candidate.name == "read_file" || candidate.name == "view_image") {
        if (permissions_.decide(request) == Decision::deny)
          throw Error("Permission denied for " + candidate.name + ": " + request.target);
      } else if (candidate.name == "apply_patch") {
        for (const auto &line : patch_lines(candidate.arguments.at("patch")))
          for (const auto &prefix :
               {"*** Update File: ", "*** Add File: ", "*** Delete File: ", "*** Move to: "})
            if (line.rfind(prefix, 0) == 0)
              check(line.substr(std::string(prefix).size()));
      } else if (permissions_.decide(request) == Decision::deny)
        throw Error("Permission denied for " + candidate.name);
    };
    preflight(call, execution);
    invocation = execution.context;
    invocation_cwd = invocation.value("cwd", config_.cwd.string());
    HookOutcome hook;
    if (call.name != "write_stdin")
      hook = run_hooks("PreToolUse",
                       {{"hook_event_name", "PreToolUse"},
                        {"tool_name", definition->hook_name},
                        {"tool", call.name},
                        {"tool_input", invocation},
                        {"cwd", invocation_cwd}},
                       cancel);
    if (hook.blocked || hook.stopped)
      throw Error("Hook blocked PreToolUse: " + hook.reason);
    if (hook.updated_input) {
      const auto updated = hook.updated_input.value();
      if (call.name == "exec_command") {
        effective.arguments["cmd"] = updated.at("command");
        for (const auto *key : {"shell", "login", "tty"})
          if (updated.contains(key))
            effective.arguments[key] = updated.at(key);
        if (updated.contains("cwd"))
          effective.arguments["workdir"] = updated.at("cwd");
      } else
        effective.arguments = updated;
    }
    if (hook.updated_input) {
      invocation = authored_hook_input(effective);
      if (invocation.is_object() && invocation.value("cwd", Json()).is_string())
        invocation_cwd = invocation.at("cwd").get<std::string>();
      validator.validate(effective.arguments);
      execution = execution_request(effective);
      invocation = execution.context;
      invocation_cwd = invocation.value("cwd", config_.cwd.string());
      preflight(effective, execution);
    }
    if (exec_output) {
      const auto requested = effective.arguments.value("max_output_tokens", 10000);
      if (requested < 1 || requested > 1000000)
        throw Error("Invalid max_output_tokens");
      response_limit = std::min(config_.tool_output_limit, static_cast<size_t>(requested) * 4);
    }
    completed = execute_inner(effective, execution, cancel);
    action_completed = true;
    auto &result = completed;
    const bool process_output = result.metadata.contains("total_output_bytes");
    if (!result.capture) {
      result.capture = std::make_shared<OutputCapture>(capture_store_, response_limit);
      result.capture->append(result.content);
      result.capture->finish();
    }
    if (!result.preview)
      result.preview = result.capture->snapshot();
    std::string prefix, suffix;
    if (!hook.additional_context.empty())
      suffix = "\nHook context: " + hook.additional_context;
    result.content = annotated(*result.preview, response_limit, "",
                               feedback_text(suffix, response_limit / 3), "")
                         .text;
    try {
      HookOutcome post;
      if (!result.metadata.value("already_completed", false) &&
          !((call.name == "exec_command" || call.name == "write_stdin") &&
            result.metadata.contains("session_id")))
        post = run_hooks("PostToolUse",
                         {{"hook_event_name", "PostToolUse"},
                          {"tool_name", definition->hook_name},
                          {"tool", call.name},
                          {"tool_input", invocation},
                          {"tool_response", result.content},
                          {"cwd", invocation_cwd}},
                         cancel);
      if (!post.additional_context.empty())
        suffix += "\nHook context: " + post.additional_context;
      if (post.stopped) {
        result.error = true;
        result.metadata["action_completed"] = true;
        result.metadata["hook_stop"] = post.reason;
        prefix = "Tool completed; hook requested stop: " + post.reason + "\n";
      } else if (post.blocked)
        throw Error(post.reason);
    } catch (const Error &e) {
      if (e.code == 130)
        throw;
      prefix = "Tool completed; PostToolUse feedback: " + std::string(e.what()) + "\n";
      result.error = true;
      result.metadata["action_completed"] = true;
    }
    const auto annotation_budget = response_limit / 3;
    const auto clean_prefix = safe_text(prefix), clean_suffix = safe_text(suffix);
    prefix = feedback_text(clean_prefix, annotation_budget);
    suffix = feedback_text(clean_suffix, annotation_budget - prefix.size());
    if (prefix.size() != clean_prefix.size() || suffix.size() != clean_suffix.size())
      result.metadata["hook_feedback_truncated"] = true;
    auto rendered = annotated(*result.preview, response_limit, prefix, suffix, "");
    if (rendered.omitted)
      result.capture->retain();
    result.capture->flush();
    ProcessResult retention;
    result.capture->metadata(retention, result.preview->total);
    if (!retention.spill_path.empty())
      result.metadata["spill_path"] = retention.spill_path;
    if (!retention.spill_path.empty() || retention.spill_truncated ||
        !retention.capture_error.empty()) {
      result.metadata["spill_bytes"] = retention.spill_bytes;
      result.metadata["spill_truncated"] = retention.spill_truncated;
    }
    if (!retention.capture_error.empty())
      result.metadata["capture_error"] = retention.capture_error;
    if (result.metadata.contains("capture_error"))
      capture_store_->warn(result.metadata["capture_error"].get<std::string>());
    std::string notice;
    if (rendered.omitted && response_limit >= 128) {
      if (result.metadata.contains("spill_path")) {
        notice =
            "\n[Output saved to " + safe_text(result.metadata["spill_path"].get<std::string>());
        if (result.metadata.value("spill_truncated", false))
          notice += "; capture incomplete";
        notice += "]";
        if (notice.size() > response_limit / 3)
          notice = result.metadata.value("spill_truncated", false)
                       ? "\n[Capture incomplete; see metadata.spill_path]"
                       : "\n[Output saved; see metadata.spill_path]";
      } else
        notice = "\n[Output omitted; disk capture unavailable]";
      rendered = annotated(*result.preview, response_limit, prefix, suffix, notice);
    }
    if (process_output || rendered.omitted || result.metadata.contains("spill_path")) {
      result.metadata["total_output_bytes"] = result.preview->total;
      result.metadata["omitted_output_bytes"] = rendered.omitted;
      result.metadata["truncated"] = rendered.omitted != 0;
      if (exec_output && rendered.omitted)
        result.metadata["original_token_count"] = (result.preview->total + 3) / 4;
    }
    result.content = std::move(rendered.text);
    result.metadata = safe_json(result.metadata);
    if (exec_output) {
      result.metadata["output"] = result.content;
      result.content = result.metadata.dump();
    }
    Message reference;
    reference.role = "tool";
    reference.text = Json({{"metadata", result.metadata}}).dump();
    reference_results({reference});
    return result;
  } catch (const Error &e) {
    if (action_completed)
      return preserve_completed(std::current_exception(), e.what());
    if (e.code == 130)
      throw;
    try {
      run_hooks("PostToolUseFailure",
                {{"hook_event_name", "PostToolUseFailure"},
                 {"tool_name",
                  tool_definition(call.name) ? tool_definition(call.name)->hook_name : call.name},
                 {"tool_input", invocation},
                 {"cwd", invocation_cwd},
                 {"error", e.what()}},
                cancel);
    } catch (const Error &hook_error) {
      if (hook_error.code == 130)
        throw;
    }
    Json metadata = Json::object();
    if (dynamic_cast<const ProcessTimeout *>(&e))
      metadata["exit_code"] = 124;
    return {bounded_text(e.what(), config_.tool_output_limit), true, metadata};

  } catch (const std::exception &e) {
    if (action_completed)
      return preserve_completed(std::current_exception(), e.what());
    if (fatal(std::current_exception()))
      throw;
    return {bounded_text(std::string("Tool failed: ") + e.what(), config_.tool_output_limit), true,
            Json::object()};
  } catch (...) {
    if (action_completed)
      return preserve_completed(std::current_exception(), "Unknown post-action failure");
    throw;
  }
}
ToolResult Tools::execute_inner(const ToolCall &call, const PermissionRequest &request,
                                const Cancellation &cancel) {
  const auto &a = call.arguments;
  const auto &name = call.name;
  const auto *definition = tool_definition(name);
  const auto structured = [](Json data) { return ToolResult{data.dump(), false, std::move(data)}; };
  if (definition->handler == ToolHandler::read) {
    const auto p = path(a.at("path"));
    authorize(request);
    if (fs::is_directory(p)) {
      Json entries = Json::array();
      size_t visited = 0;
      const auto context = permissions_.context();
      for (const auto &entry : fs::directory_iterator(p)) {
        cancel.check();
        if (++visited > 10000)
          break;
        if (permissions_.decide({name, entry.path().string(), entry.path(), false}, context) ==
            Decision::deny)
          continue;
        entries.push_back({{"name", safe_text(entry.path().filename().string())},
                           {"type", entry.is_symlink()     ? "symlink"
                                    : entry.is_directory() ? "directory"
                                                           : "file"}});
        if (entries.dump().size() > config_.tool_output_limit / 2)
          break;
      }
      return structured({{"entries", entries}, {"truncated", visited > entries.size()}});
    }
    const auto content = files_.read(p);
    if (content.find('\0') != std::string::npos)
      throw Error("Binary file cannot be read as text");
    const auto ls = lines(content);
    const auto first = a.value("offset", 1), count = a.value("limit", 2000);
    if (first < 1 || count < 1 || count > 100000)
      throw Error("Invalid line range");
    std::string text;
    for (size_t i = static_cast<size_t>(first - 1);
         i < ls.size() && i - static_cast<size_t>(first - 1) < static_cast<size_t>(count); ++i) {
      text += std::to_string(i + 1) + "\t" + ls[i] + "\n";
      if (text.size() > config_.tool_output_limit)
        break;
    }
    return {text, false, {{"total_lines", ls.size()}}};
  }
  if (definition->handler == ToolHandler::patch) {
    // Snapshot only within this call. A prior model-facing read is not required.
    std::map<fs::path, std::string> snapshots;
    auto guard = [&](const fs::path &p) {
      const auto current = fs::exists(p) ? digest(files_.read(p)) : "<missing>";
      const auto it = snapshots.find(p);
      if (it != snapshots.end() && it->second != current)
        throw Error("File changed during patch: " + p.string());
      snapshots[p] = current;
    };
    std::vector<Change> changes;
    auto add = [&](const fs::path &p, std::optional<std::string> after, bool approved = false) {
      if (!approved)
        authorize({name, p.string(), p, true});
      guard(p);
      std::optional<std::string> before;
      if (fs::exists(p))
        before = files_.read(p);
      if (after && after->size() > size_t{16} * size_t{1024} * 1024)
        throw Error("Write exceeds 16 MiB limit");
      if (std::any_of(changes.begin(), changes.end(), [&](const Change &x) { return x.path == p; }))
        throw Error("Duplicate file in batch/patch");
      if (!before && !after)
        throw Error("Cannot delete a missing file");
      struct stat st{};
      const auto mode = stat(p.c_str(), &st) == 0 ? st.st_mode & 0777 : 0644;
      changes.push_back({p, before, std::move(after), static_cast<mode_t>(mode)});
    };
    const auto patch = a.at("patch").get<std::string>();
    auto ls = patch_lines(patch);
    const auto trim = [](const std::string &line) {
      const auto first = line.find_first_not_of(" \t\r");
      return first == std::string::npos
                 ? std::string{}
                 : line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
    };
    while (!ls.empty() && trim(ls.front()).empty())
      ls.erase(ls.begin());
    while (!ls.empty() && trim(ls.back()).empty())
      ls.pop_back();
    if (!ls.empty()) {
      ls.front() = trim(ls.front());
      ls.back() = trim(ls.back());
    }
    if (ls.empty() || ls.front() != "*** Begin Patch" || ls.back() != "*** End Patch")
      throw Error("Expected complete *** Begin Patch envelope");
    for (size_t i = 1; i + 1 < ls.size();) {
      const auto &header = ls[i++];
      const auto colon = header.find(": ");
      if (colon == std::string::npos)
        throw Error("Malformed patch file header");
      const auto p = path(header.substr(colon + 2));
      const auto kind = header.substr(0, colon);
      if (kind == "*** Delete File") {
        add(p, std::nullopt);
        continue;
      }
      if (kind == "*** Add File") {
        if (fs::exists(p))
          throw Error("Add File already exists");
        std::string text;
        while (i + 1 < ls.size() && ls[i].rfind("*** ", 0) != 0) {
          if (ls[i].empty() || ls[i][0] != '+')
            throw Error("Invalid add line");
          text += ls[i++].substr(1) + "\n";
        }
        add(p, text);
        continue;
      }
      if (kind != "*** Update File")
        throw Error("Unknown patch directive");
      authorize({name, p.string(), p, true});
      guard(p);
      auto text = files_.read(p);
      fs::path destination = p;
      if (i < ls.size() && ls[i].rfind("*** Move to: ", 0) == 0) {
        destination = path(ls[i++].substr(13));
        if (fs::exists(destination))
          throw Error("Move destination exists");
      }
      text = patch_update(text, ls, i, cancel);
      if (destination != p) {
        add(p, std::nullopt, true);
        const auto original_mode = changes.back().mode;
        add(destination, text);
        changes.back().mode = original_mode;
      } else
        add(p, text, true);
    }
    if (changes.empty())
      throw Error("Empty patch");
    for (const auto &change : changes)
      guard(change.path);
    size_t applied = 0;
    try {
      for (auto &change : changes) {
        cancel.check();
        if (change.after) {
          fs::create_directories(change.path.parent_path());
          files_.atomic_write(change.path, *change.after);
          ++applied;
          if (!change.before && chmod(change.path.c_str(), change.mode) != 0)
            throw Error("Cannot set file mode");
        } else {
          files_.remove(change.path);
          ++applied;
        }
      }
    } catch (...) {
      const auto original = std::current_exception();
      std::string failures;
      while (applied > 0) {
        auto &change = changes[--applied];
        try {
          if (change.before) {
            files_.atomic_write(change.path, *change.before);
            if (chmod(change.path.c_str(), change.mode) != 0)
              throw Error("Cannot restore file mode");
          } else
            files_.remove(change.path);
        } catch (const std::exception &e) {
          failures += change.path.string() + ": " + e.what() + "; ";
        }
      }
      if (!failures.empty())
        throw Error("Batch failed and rollback needs attention: " + failures);
      std::rethrow_exception(original);
    }
    Json paths = Json::array();
    for (const auto &change : changes) {
      paths.push_back(change.path.string());
    }
    return {
        "Applied " + std::to_string(changes.size()) + " file change(s)", false, {{"paths", paths}}};
  }

  if (definition->handler == ToolHandler::exec || definition->handler == ToolHandler::stdin_write) {
    const auto started = std::chrono::steady_clock::now();
    int id = 0;
    int wait = 10000;
    if (definition->handler == ToolHandler::exec) {
      const auto command = request.target;
      if (a.value("sandbox_permissions", "use_default") != "use_default" ||
          a.contains("justification") || a.contains("prefix_rule"))
        throw Error("Sandbox escalation and permission profiles are unavailable on Tiger; use "
                    "application permission configuration");
      authorize(request);
      ProcessSpec spec;
      const auto shell = request.context.at("shell").get<std::string>();
      spec.argv = {shell};
      if (request.context.at("login").get<bool>())
        spec.argv.push_back("-l");
      spec.argv.push_back("-c");
      spec.argv.push_back(command);
      spec.cwd = request.context.at("cwd").get<std::string>();
      spec.pty = request.context.at("tty").get<bool>();
      spec.interactive_stdin = true;
      spec.timeout_ms = std::numeric_limits<int>::max();
      spec.output_limit = config_.tool_output_limit;
      spec.spool_directory = capture_directory(config_);
      spec.capture_store = capture_store_;
      wait = std::clamp(a.value("yield_time_ms", 10000), 250, 30000);
      if (exec_sessions_.size() >= 128) {
        for (auto it = exec_sessions_.begin(); it != exec_sessions_.end();) {
          if (it->second.completed)
            it = exec_sessions_.erase(it);
          else
            ++it;
        }
        if (exec_sessions_.size() >= 128)
          throw Error("Exec session limit reached");
      }
      do {
        id = static_cast<int>(std::hash<std::string>{}(unique_id()) & 0x7fffffffU);
      } while (id == 0 || exec_sessions_.count(id));
      exec_sessions_[id] = {
          processes_.start(spec, cancel), command, spec.cwd, shell, spec.pty, false, std::nullopt};
    } else {
      id = a.at("session_id").get<int>();
      auto &session = exec_sessions_.at(id);
      const auto input = a.value("chars", std::string{});
      authorize(request);
      if (session.completed) {
        const auto exit_status = session.exit_code;
        if (!exit_status)
          throw Error("Exec session ended without a collected exit status");
        const auto exit_code = exit_status.value();
        return {"",
                false,
                {{"exit_code", exit_code},
                 {"already_completed", true},
                 {"total_output_bytes", 0},
                 {"stdin_closed", true}}};
      }
      if (!input.empty())
        processes_.write(session.process, input);
      if (a.value("close_stdin", false))
        processes_.close_stdin(session.process);
      wait = std::clamp(a.value("yield_time_ms", input.empty() ? 5000 : 250),
                        input.empty() ? 5000 : 1, input.empty() ? 300000 : 30000);
    }
    auto &job = exec_sessions_.at(id);
    ProcessResult result;
    try {
      do {
        cancel.check();
        const auto status = processes_.status(job.process);
        if (!status.running)
          break;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        if (elapsed >= wait)
          break;
        clock_.sleep_ms(static_cast<int>(std::min<int64_t>(10, wait - elapsed)), cancel);
      } while (true);
      result = processes_.take_output(job.process);
    } catch (...) {
      try {
        processes_.stop(job.process);
      } catch (...) {
        std::fputs("Exec cleanup failed; preserving original error\n", stderr);
      }
      job.completed = true;
      throw;
    }
    Json data = {
        {"chunk_id", unique_id()},
        {"wall_time_seconds",
         std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()},
        {"total_output_bytes", result.total_output_bytes}};
    if (result.running)
      data["session_id"] = id;
    else {
      data["exit_code"] = result.exit_code;
      job.completed = true;
      job.exit_code = result.exit_code;
    }
    if (!result.spill_path.empty())
      data["spill_path"] = result.spill_path;
    if (!result.spill_path.empty() || result.spill_truncated || !result.capture_error.empty()) {
      data["spill_bytes"] = result.spill_bytes;
      data["spill_truncated"] = result.spill_truncated;
    }
    if (!result.capture_error.empty())
      data["capture_error"] = result.capture_error;
    return {result.output, false, data, result.preview, result.capture};
  }
  if (definition->handler == ToolHandler::image) {
    const auto p = path(a.at("path"));
    authorize(request);
    if (request.internal_read && !generated_image(config_, files_, p))
      throw Error("Generated image artifact checksum mismatch");
    const auto detail = a.value("detail", std::string("high"));
    auto artifact = persist_image(config_, files_, images_->load(p, detail), detail);
    auto out = structured({{"detail", detail}, {"artifact", artifact}});
    out.images.push_back(artifact);
    return out;
  }
  if (definition->handler == ToolHandler::plan) {
    authorize({name, "task plan", std::nullopt, false});
    const auto &plan = a.at("plan");
    if (plan.size() > 100)
      throw Error("Plan exceeds 100 steps");
    size_t active = 0;
    for (const auto &step : plan)
      if (step.at("status") == "in_progress")
        ++active;
    if (active > 1)
      throw Error("At most one step may be in_progress");
    const auto serialized = plan.dump();
    if (serialized.size() > 1048576)
      throw Error("Plan exceeds 1 MiB; saved plan unchanged");
    if (!session_id_.empty()) {
      fs::create_directories(config_.home / "plans");
      files_.atomic_write(config_.home / "plans" / (session_id_ + ".json"), serialized, true);
    }
    todos_ = plan;
    large_plan_ = false;
    return {"Plan updated"};
  }
  if (definition->handler == ToolHandler::question) {
    authorize({name, "user questions", std::nullopt, false});
    const auto &questions = a.at("questions");
    if (questions.empty() || questions.size() > 3)
      throw Error("Provide one to three questions");
    Json answers = Json::object();
    for (const auto &q : questions) {
      const auto id = q.at("id").get<std::string>();
      if (id.empty() || answers.contains(id) || q.at("header").get<std::string>().size() > 12)
        throw Error("Invalid question ID/header");
      const auto &choices = q.at("options");
      if (choices.size() < 2 || choices.size() > 3)
        throw Error("Provide two or three options");
      std::vector<std::string> options;
      std::string prompt = q.at("question");
      if (prompt.empty() || prompt.size() > 8192)
        throw Error("Invalid question length");
      for (const auto &option : choices) {
        const auto label = option.at("label").get<std::string>();
        if (label.empty() || label.size() > 1024)
          throw Error("Invalid option label");
        options.push_back(label);
        prompt += "\n" + label + ": " + option.at("description").get<std::string>();
      }
      auto answer = question_(prompt, options);
      for (size_t i = 0; i < options.size(); ++i)
        if (answer == std::to_string(i + 1)) {
          answer = options[i];
          break;
        }
      answers[id] = {{"answers", Json::array({answer})}};
    }
    return structured({{"answers", answers}});
  }
  if (definition->handler == ToolHandler::async_question ||
      definition->handler == ToolHandler::message) {
    authorize({name, "user interaction", std::nullopt, false});
    Json event = {{"type", definition->handler == ToolHandler::message
                               ? "codet.user_message"
                               : "codet.user_input.requested"},
                  {"request_id", call.id}};
    if (definition->handler == ToolHandler::message) {
      const auto text = a.at("message").get<std::string>();
      if (text.empty() || text.size() > 8192)
        throw Error("Invalid message length");
      event["message"] = text;
    } else {
      const auto &qs = a.at("questions");
      if (qs.empty() || qs.size() > 32)
        throw Error("Invalid question count");
      for (const auto &q : qs) {
        const auto title = q.at("title").get<std::string>();
        if (title.empty() || title.size() > 8192)
          throw Error("Invalid question title");
        if (q.contains("options")) {
          if (q["options"].empty() || q["options"].size() > 32)
            throw Error("Invalid options");
          for (const auto &o : q["options"])
            if (o.get<std::string>().empty() || o.get<std::string>().size() > 1024)
              throw Error("Invalid option");
        }
      }
      event["questions"] = qs;
    }
    interaction_->post(event);
    return structured({{"accepted", true}});
  }
  if (definition->handler == ToolHandler::time) {
    authorize({name, "current time", std::nullopt, false});
    const auto stamp = static_cast<time_t>(clock_.seconds());
    std::tm time{};
    if (!gmtime_r(&stamp, &time))
      throw Error("Current time unavailable");
    char text[64];
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S UTC", &time);
    return structured({{"current_time", text}});
  }
  if (definition->handler == ToolHandler::sleep) {
    authorize({name, "wait", std::nullopt, false});
    const auto duration = a.at("duration_ms").get<int64_t>();
    if (duration < 1 || duration > 43200000)
      throw Error("duration_ms must be between 1 and 43200000");
    const auto start = std::chrono::steady_clock::now();
    bool interrupted = false;
    if (interaction_)
      interrupted = interaction_->sleep(duration, cancel);
    else
      clock_.sleep_ms(static_cast<int>(duration), cancel);
    return structured(
        {{"wall_time_seconds",
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()},
         {"interrupted", interrupted}});
  }
  if (definition->handler == ToolHandler::computer) {
    authorize(request);
    auto result = computer_->invoke(name.substr(9), a, cancel);
    auto out = structured(result.data);
    if (result.image) {
      auto artifact =
          persist_image(config_, files_, *result.image, a.value("detail", std::string("high")));
      out.images.push_back(artifact);
      out.metadata["artifact"] = artifact;
      out.content = out.metadata.dump();
    }
    return out;
  }
  throw Error("Unavailable tool handler");
}
std::string Tools::instructions() const {
  const std::string inspection = config_.permission_mode == "plan" ? "read_file" : "exec_command";
  const auto cwd = files_.canonical(config_.cwd), home = files_.canonical(config_.home);
  fs::path root = cwd;
  for (auto p = cwd; !p.empty(); p = p.parent_path()) {
    if (fs::exists(p / ".git")) {
      root = p;
      break;
    }
    if (p == p.root_path())
      break;
  }
  std::vector<fs::path> directories;
  for (auto p = cwd;; p = p.parent_path()) {
    directories.push_back(p);
    if (p == root)
      break;
  }
  directories.push_back(home);
  std::string out;
  const auto warn = [&](const std::string &message) {
    if (warnings_.insert(message).second)
      report_diagnostic(diagnostics_, message);
  };
  std::set<fs::path> seen;
  const auto append = [&](const std::string &text, size_t cap) {
    if (out.size() >= cap)
      return false;
    const auto clean = safe_text(text);
    const auto portion = bounded_text(clean, cap - out.size());
    out += portion;
    return portion.size() == clean.size();
  };
  std::function<void(const fs::path &, int)> include = [&](const fs::path &input, int depth) {
    const auto p = files_.canonical(input);
    if (seen.count(p) || !fs::exists(p) || (!within(p, root) && !within(p, home)))
      return;
    if (depth > 16) {
      warn("Instruction include depth exceeded: " + p.string());
      return;
    }
    seen.insert(p);
    if (!config_.project_doc_max_bytes)
      return;
    if (out.size() >= config_.project_doc_max_bytes) {
      warn("Instruction budget exhausted; omitted: " + p.string());
      return;
    }
    bool truncated = false;
    std::string text;
    try {
      text = files_.read_prefix(p, config_.project_doc_max_bytes - out.size(), truncated);
    } catch (const std::exception &e) {
      warn("Instruction read failed: " + std::string(e.what()));
      return;
    }
    append("\n# Instructions: " + p.string() + "\n", config_.project_doc_max_bytes);
    const auto parts = lines(text);
    for (size_t i = 0; i < parts.size(); ++i) {
      const auto &line = parts[i];
      const bool complete_line =
          !truncated || i + 1 < parts.size() || (!text.empty() && text.back() == '\n');
      if (complete_line && line.rfind("@include ", 0) == 0)
        include(p.parent_path() / line.substr(9), depth + 1);
      else if (complete_line && !line.empty() && line[0] == '@' &&
               line.find(' ') == std::string::npos)
        include(p.parent_path() / line.substr(1), depth + 1);
      else if (!append(line + "\n", config_.project_doc_max_bytes)) {
        truncated = true;
        break;
      }
    }
    if (truncated)
      warn("Instruction content truncated to project_doc_max_bytes: " + p.string());
  };
  for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
    include(*it / "AGENTS.md", 0);
    include(*it / "CLAUDE.md", 0);
  }
  const auto memory = config_.home / "memory";
  size_t count = 0;
  if (config_.memory_context_max_bytes && fs::exists(memory)) {
    std::set<fs::path> paths;
    bool omitted = false;
    for (const auto &entry : fs::directory_iterator(memory))
      if (!entry.is_symlink() && entry.is_regular_file() && entry.path().extension() == ".md") {
        paths.insert(entry.path());
        if (paths.size() > 200) {
          paths.erase(std::prev(paths.end()));
          omitted = true;
        }
      }
    if (omitted)
      warn("Memory file count exceeds 200; omitted later paths");
    const auto cap = out.size() + config_.memory_context_max_bytes;
    for (const auto &p : paths) {
      if (++count > 200 || out.size() >= cap) {
        warn("Memory context budget exhausted; use " + inspection + " to read additional files");
        break;
      }
      if (!append("\n# Memory: " + p.string() + " (read through " + inspection + ")\n", cap))
        break;
      bool truncated = false;
      try {
        const auto text = files_.read_prefix(p, cap - out.size(), truncated);
        append(text, cap);
        if (truncated)
          warn("Memory context truncated: " + p.string());
      } catch (const std::exception &e) {
        warn("Memory read failed: " + std::string(e.what()));
      }
    }
  }
  out += "\n# Available skills (read SKILL.md through " +
         std::string(config_.permission_mode == "plan" ? "read_file" : "exec_command") + ")\n";
  count = 0;
  for (const auto &dir :
       std::vector<fs::path>{config_.cwd / ".codet/skills", config_.cwd / ".agents/skills",
                             config_.cwd / ".claude/skills", config_.home / "skills"})
    if (fs::exists(dir))
      for (const auto &entry : fs::directory_iterator(dir))
        if (++count <= 100 && !entry.is_symlink() && fs::exists(entry.path() / "SKILL.md"))
          out += entry.path().string() + "\n";
  return out;
}
std::string Tools::dynamic_context() const {
  std::string out;
  if (!todos_.empty() || large_plan_) {
    out = "\n# Current task plan (bounded view; original step numbers)\n";
    out += "Full saved plan: " + (config_.home / "plans" / (session_id_ + ".json")).string() +
           "; inspect with read_file.\n";
    if (large_plan_)
      out += "Saved plan is too large to preview; inspect it in pages before changing it.\n";
    for (const auto *status : {"in_progress", "pending", "completed"})
      for (size_t i = 0; i < todos_.size(); ++i) {
        const auto &step = todos_[i];
        if (step.at("status") != status)
          continue;
        const auto &label = step.at("step").get_ref<const std::string &>();
        const auto line = std::to_string(i + 1) + ". [" + status + "] " +
                          bounded_text(label.substr(0, 132), 128) +
                          (label.size() > 128 ? " [see full plan]" : "") + "\n";
        if (out.size() + line.size() + 100 > 8192) {
          out += "Additional steps omitted; read the full saved plan.\n";
          break;
        }
        out += line;
      }
  }
  if (!session_id_.empty()) {
    const auto index = artifact_index_path(artifact_directory(config_), session_id_);
    if (fs::exists(index))
      out += "\n# Historical image index\n" + index.string() + "\nPosition: " +
             image_history_position(artifact_directory(config_), files_, session_id_).dump() +
             "\nArtifact root: " + artifact_directory(config_).string() +
             "\nPage this JSONL observation index with read_file in any mode. Resolve relative "
             "image paths under the artifact root and reload selected images with view_image. "
             "Compaction removes every attachment.\n";
  }
  return out;
}
fs::path Tools::archive_images(const std::vector<Message> &messages) {
  return archive_image_history(artifact_directory(config_), files_, session_id_, messages);
}
void Tools::use_session(const std::string &id) {
  if (id == session_id_)
    return;
  session_id_ = id;
  referenced_results_.clear();
  large_plan_ = false;
  todos_ = Json::array();
  const auto p = config_.home / "plans" / (id + ".json");
  if (fs::exists(p)) {
    if (fs::file_size(p) > 1048576) {
      large_plan_ = true;
      return;
    }
    todos_ = Json::parse(files_.read(p, 1048576), nullptr, false);
    if (!todos_.is_array())
      throw Error("Invalid saved task plan");
  }
}
std::string Tools::expand_command(const std::string &input) const {
  if (input.empty() || input[0] != '/')
    return input;
  const auto space = input.find(' ');
  const auto name = input.substr(1, space == std::string::npos ? std::string::npos : space - 1);
  if (name.empty() ||
      name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
          std::string::npos)
    throw Error("Invalid custom command name");
  for (const auto &dir : std::vector<fs::path>{
           config_.trust_project ? config_.cwd / ".codet/commands" : config_.home / "commands",
           config_.home / "commands"}) {
    const auto p = dir / (name + ".md");
    if (fs::exists(p) && within(files_.canonical(p), files_.canonical(dir))) {
      auto text = files_.read(p, 65536);
      const auto args = space == std::string::npos ? "" : input.substr(space + 1);
      size_t at = 0;
      while ((at = text.find("$ARGUMENTS", at)) != std::string::npos) {
        text.replace(at, 10, args);
        at += args.size();
      }
      return text;
    }
  }
  throw Error("Unknown command: /" + name);
}
} // namespace codet
