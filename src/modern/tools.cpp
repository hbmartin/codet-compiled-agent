#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/interaction.hpp"
#include "codet/output.hpp"
#include "codet/patch.hpp"
#include "codet/process_control.hpp"
#include "codet/support.hpp"
#include "codet/tool_registry.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <fnmatch.h>
#include <iostream>
#include <limits>
#include <nlohmann/json-schema.hpp>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

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
  for (char ch : command)
    if (static_cast<unsigned char>(ch) < 32 || ch == 127)
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
  if (r.tool == "read_file" && config_.permission_mode != "plan" && !r.internal_read)
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
      throw HookFailure("Permission denied by hook: " + hook.reason);
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
  for (const auto &root : {config_.home, invocation_directory(c), config_.cwd,
                           artifact_directory(c), capture_directory(c)})
    private_anchors_[fs::absolute(root).lexically_normal()] = files_.canonical(root);
}
PermissionRequest Tools::execution_request(const ToolCall &call) const {
  const auto &a = call.arguments;
  const auto *definition = tool_definition(call.name);
  PermissionRequest request{call.name, "", std::nullopt, definition && definition->mutation};
  request.context = a;
  if (call.name == "read_file" || call.name == "view_image") {
    const auto value = a.at("path").get<std::string>();
    if (value.find('\0') != std::string::npos)
      throw Error("Path must not contain NUL");
    request.target = value;
    request.path = (config_.cwd / fs::path(value)).lexically_normal();
    if (call.name == "read_file" && !fs::exists(*request.path)) {
      for (const auto &candidate : referenced_results_)
        if (value == candidate.string() ||
            fs::path(value) == candidate.lexically_relative(
                                   fs::absolute(artifact_directory(config_)).lexically_normal()) ||
            (capture_name(value) && fs::path(value).filename() == candidate.filename() &&
             (fs::path(value).is_absolute() || fs::path(value) == fs::path(value).filename()))) {
          request.path = candidate;
          break;
        }
    }
    if (call.name == "view_image") {
      const auto filename = fs::path(value).filename();
      ImageArtifact identity;
      identity.path = value;
      identity.sha256 = filename.stem().string();
      identity.mime = filename.extension() == ".png" ? "image/png" : "image/jpeg";
      if (!fs::exists(*request.path) &&
          (fs::path(value).is_absolute() || fs::path(value) == filename)) {
        try {
          request.path = resolve_image(artifact_directory(config_), identity);
        } catch (const Error &) {
          request.path = (config_.cwd / fs::path(value)).lexically_normal();
        }
      }
    }
    request.internal_read =
        call.name == "read_file"
            ? private_read(*request.path)
            : private_anchor(artifact_directory(config_)) &&
                  generated_image(config_, files_, files_.canonical(*request.path), false);
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
namespace {
bool private_regular(const FileSystem &files, const fs::path &root, const fs::path &p,
                     bool directory = false) {
  try {
    (void)checked_artifact_identity(
        files, root, p, directory ? ArtifactPolicy::file_or_directory : ArtifactPolicy::regular);
    return true;
  } catch (const Error &) {
    return false;
  }
}
} // namespace
bool Tools::private_anchor(const fs::path &root) const {
  const auto key = fs::absolute(root).lexically_normal();
  const auto found = private_anchors_.find(key);
  return found != private_anchors_.end() && files_.canonical(root) == found->second;
}
bool Tools::private_read(const fs::path &p) const {
  auto value = fs::absolute(p).lexically_normal();
  // Recognize stable OS aliases (for example Tiger's /var -> /private/var),
  // preserving every component beneath the trusted anchor for symlink checks.
  for (const auto &anchor : private_anchors_)
    if (within(value, anchor.second) && !within(value, anchor.first)) {
      value = anchor.first / value.lexically_relative(anchor.second);
      break;
    }
  const auto artifacts = artifact_directory(config_);
  if (!session_id_.empty() &&
      value == fs::absolute(artifact_index_path(artifacts, session_id_)).lexically_normal())
    return private_anchor(artifacts) && private_regular(files_, artifacts, value);
  if (!session_id_.empty() &&
      value ==
          fs::absolute(artifacts / "indexes" / (session_id_ + ".results.jsonl")).lexically_normal())
    return private_anchor(artifacts) && private_regular(files_, artifacts, value);
  const auto runtime =
      invocation_directory(config_) / "runtime" / (session_id_ + ".execution.json");
  if (!session_id_.empty() && value == fs::absolute(runtime).lexically_normal())
    return private_anchor(invocation_directory(config_)) &&
           private_regular(files_, invocation_directory(config_), value);
  const auto plans = invocation_directory(config_) / "plans";
  if (!session_id_.empty() &&
      value == fs::absolute(plans / (session_id_ + ".json")).lexically_normal())
    return private_anchor(invocation_directory(config_)) &&
           private_regular(files_, invocation_directory(config_), value);
  const auto memory = fs::absolute(config_.home / "memory").lexically_normal();
  if ((value == memory || (value.parent_path() == memory && value.extension() == ".md")) &&
      private_anchor(config_.home) && private_regular(files_, config_.home, value, value == memory))
    return true;
  for (const auto &skill : skill_directories_)
    if (within(value, fs::absolute(skill).lexically_normal())) {
      const auto found = skill_anchors_.find(skill);
      if (found != skill_anchors_.end() && private_anchor(found->second) &&
          private_regular(files_, found->second, value, true))
        return true;
    }
  const auto captures = fs::absolute(capture_directory(config_)).lexically_normal();
  if (!referenced_results_.count(value))
    return false;
  if (value.parent_path() == captures && capture_name(value))
    return private_anchor(captures) && private_regular(files_, captures, value);
  return within(value, fs::absolute(artifacts / "results").lexically_normal()) &&
         private_anchor(artifacts) && private_regular(files_, artifacts, value);
}
void Tools::restore_references(const std::vector<std::string> &references) {
  bool changed = false;
  for (const auto &ref : references) {
    fs::path target;
    if (result_name(ref))
      target = artifact_directory(config_) / ref;
    else if (capture_name(ref) && fs::path(ref) == fs::path(ref).filename())
      target = capture_directory(config_) / ref;
    else
      throw Error("Invalid persisted result reference");
    changed = referenced_results_.insert(fs::absolute(target).lexically_normal()).second || changed;
  }
  const auto root = artifact_directory(config_);
  const auto index = root / "indexes" / (session_id_ + ".results.jsonl");
  if (references.empty() || (!changed && fs::exists(index)))
    return;
  if (!private_anchor(root) || fs::is_symlink(root) || fs::is_symlink(index.parent_path()) ||
      fs::is_symlink(index))
    throw Error("Invalid result index storage");
  fs::create_directories(index.parent_path());
  if (::chmod(root.c_str(), 0700) || ::chmod(index.parent_path().c_str(), 0700))
    throw Error("Cannot protect result index directory");
  files_.atomic_stream(
      index,
      [&](const FileSystem::Writer &write) {
        write(Json({{"version", 1}, {"session_id", session_id_}}).dump() + '\n');
        for (const auto &ref : references)
          write(Json({{"kind", result_name(ref) ? "result" : "capture"}, {"path", ref}}).dump() +
                '\n');
      },
      true);
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
    if (!message.result_file.empty()) {
      const fs::path ref(message.result_file);
      if (result_name(ref))
        referenced_results_.insert(
            fs::absolute(artifact_directory(config_) / ref).lexically_normal());
      else
        accept(message.result_file);
    }
    const auto envelope = Json::parse(message.text, nullptr, false);
    if (!envelope.is_object())
      continue;
    const auto metadata = envelope.value("metadata", Json::object());
    if (metadata.is_object() && metadata.value("spill_path", Json()).is_string())
      accept(metadata.at("spill_path"));
  }
}
std::string Tools::result_identifier(const Message &message) const {
  if (!message.result_file.empty()) {
    const fs::path ref(message.result_file);
    if (capture_name(ref) && ref == ref.filename())
      return ref.string();
    if (!result_name(ref))
      throw Error("Invalid private result reference");
    return ref.string();
  }
  const auto ref =
      fs::path("results") / (session_id_.empty() ? result_owner_ : session_id_) /
      ((message.message_id.empty() ? "00000000-0000-0000-0000-000000000000" : message.message_id) +
       ".json");
  if (!result_name(ref))
    throw Error("Invalid private result identity");
  return ref.string();
}
std::string Tools::result_reference(const Message &message) const {
  const auto ref = result_identifier(message);
  return ((capture_name(ref) ? capture_directory(config_) : artifact_directory(config_)) / ref)
      .string();
}
ReferenceObservation Tools::observe_result_reference(const std::string &original,
                                                     bool checkpoint) const {
  ReferenceObservation observation;
  observation.policy =
      checkpoint ? ArtifactPolicy::private_owned_regular : ArtifactPolicy::owned_regular;
  try {
    fs::path ref(original);
    const auto artifacts = fs::absolute(artifact_directory(config_)).lexically_normal();
    const auto captures = fs::absolute(capture_directory(config_)).lexically_normal();
    observation.root = artifacts;
    if (ref.is_absolute()) {
      // Remap only a trusted anchor alias. Keep components beneath it for symlink checks.
      for (const auto &root : {artifacts, captures}) {
        const auto anchor = private_anchors_.find(root);
        if (anchor != private_anchors_.end()) {
          const auto remapped = remap_artifact_alias(ref, root, anchor->second);
          if (remapped != ref) {
            ref = remapped;
            break;
          }
        }
      }
      if (ref.parent_path() == captures && capture_name(ref)) {
        observation.root = captures;
        ref = ref.filename();
      } else
        ref = ref.lexically_relative(artifacts);
    }
    if (!checkpoint && capture_name(ref) && ref == ref.filename())
      observation.root = captures;
    else if (!result_name(ref))
      throw Error("Untrusted result reference");
    observation.identifier = ref.string();
    observation.path = observation.root / ref;
    if (!private_anchor(observation.root))
      throw Error("Artifact anchor changed");
    observation.identity =
        checked_artifact_identity(files_, observation.root, observation.path, observation.policy);
    if (!checkpoint) {
      auto input = files_.open_read(observation.path);
      if (input->path() != files_.canonical(observation.path) || !reference_current(observation))
        throw Error("Result identity changed while opening");
    }
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &error) {
    observation.error = bounded_text(error.what(), 512);
    if (observation.error.empty())
      observation.error = "Result reference unavailable";
  }
  return observation;
}
bool Tools::reference_current(const ReferenceObservation &observation) const {
  std::optional<std::string> current;
  try {
    if (!observation.path.empty() && private_anchor(observation.root))
      current =
          checked_artifact_identity(files_, observation.root, observation.path, observation.policy);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    // A missing or unsafe reference remains unavailable; availability changes invalidate it.
    current = std::nullopt;
  }
  return current == observation.identity;
}
void Tools::retain_recovery_result(Message &message, std::vector<RecoveryCandidate> &candidates) {
  if (message.message_id.empty())
    message.message_id = unique_id();
  const fs::path path = result_reference(message);
  const bool created = message.result_file.empty() && !fs::exists(path);
  if (created) {
    RecoveryCandidate candidate;
    candidate.file.identifier = result_identifier(message);
    candidate.file.root = fs::absolute(artifact_directory(config_)).lexically_normal();
    candidate.file.path = candidate.file.root / candidate.file.identifier;
    candidate.file.policy = ArtifactPolicy::private_owned_regular;
    candidates.push_back(std::move(candidate));
  }
  retain_result_impl(message, created ? &candidates.back() : nullptr);
  if (created) {
    auto &candidate = candidates.back();
    candidate.file.identity = checked_artifact_identity(files_, candidate.file.root,
                                                        candidate.file.path, candidate.file.policy);
  }
}
void Tools::discard_recovery_candidate(const RecoveryCandidate &candidate) {
  const auto &file = candidate.file;
  if (!result_name(file.identifier) || file.path != file.root / file.identifier ||
      !private_anchor(file.root) || fs::is_symlink(file.root))
    throw Error("Recovery candidate storage changed");
  // Check each component without following links, even when the leaf is absent.
  auto component = file.root;
  for (const auto &part : file.path.lexically_relative(file.root)) {
    component /= part;
    struct stat status {};
    if (::lstat(component.c_str(), &status)) {
      if (errno == ENOENT)
        return;
      throw Error("Cannot inspect recovery candidate");
    }
    if (S_ISLNK(status.st_mode))
      throw Error("Unsafe recovery candidate symlink");
  }
  const auto identity = checked_artifact_identity(files_, file.root, file.path, file.policy);
  if (file.identity && file.identity != identity)
    throw Error("Recovery candidate identity changed");
  // Also handles a writer that throws after publishing the newly created archive.
  if (digest(files_.read(file.path, candidate.bytes + 1)) != candidate.content_digest ||
      checked_artifact_identity(files_, file.root, file.path, file.policy) != identity)
    throw Error("Recovery candidate content changed");
  files_.remove(file.path);
}
Json Tools::execution_fact(int id, const ExecSession &session) const {
  Json fact = {{"session_id", id}};
  try {
    const auto status = processes_.status(session.process);
    if (!status.error.empty()) {
      fact["state"] = "unknown";
      fact["error"] = bounded_text(status.error, 512);
    } else {
      fact["state"] = status.running ? "running" : "awaiting_output";
      if (!status.running)
        fact["exit_code"] = status.exit_code;
    }
  } catch (const std::exception &error) {
    if (fatal(std::current_exception()))
      throw;
    fact["state"] = "unknown";
    fact["error"] = bounded_text(error.what(), 512);
  }
  return fact;
}
Json Tools::execution_state(const std::vector<Message> &messages,
                            const std::function<Json()> &recover) const {
  return observe_execution(messages, recover).facts;
}
ExecutionObservation Tools::observe_execution(const std::vector<Message> &messages,
                                              const std::function<Json()> &recover) const {
  ExecutionObservation observation;
  std::map<std::string, Json> recovery, unavailable;
  Json journal;
  std::map<fs::path, std::pair<std::string, std::shared_ptr<const Json>>> observed;
  const auto collect = [&](const Json &facts) {
    if (!facts.is_array())
      throw Error("Invalid checkpoint recovery state");
    for (const auto &fact : facts)
      if (fact.is_object() && fact.value("kind", Json()) == "recovery_unavailable")
        unavailable[fact.dump()] = fact;
      else if (fact.is_object() && fact.value("call_id", Json()).is_string() &&
               fact.contains("recovery"))
        recovery[fact.at("call_id").get<std::string>()] = fact;
  };
  for (const auto &message : messages) {
    if (!message.checkpoint_state.is_null()) {
      const auto &saved = message.checkpoint_state;
      try {
        if (saved.is_array())
          collect(saved);
        else {
          const auto ref = saved.at("recovery_record").get<std::string>();
          auto entry = observation.archives.find(ref);
          if (entry == observation.archives.end())
            entry = observation.archives.emplace(ref, observe_result_reference(ref, true)).first;
          auto &reference = entry->second;
          if (!reference.error.empty() || !reference.identity)
            throw Error(reference.error.empty() ? "Recovery identity unavailable"
                                                : reference.error);
          const auto &path = reference.path;
          const auto &identity = reference.identity.value();
          auto cached = recovery_archives_.find(path);
          if (cached != recovery_archives_.end() && cached->second.identity != identity) {
            recovery_archive_bytes_ -= cached->second.bytes;
            recovery_archives_.erase(cached);
            cached = recovery_archives_.end();
          }
          if (cached != recovery_archives_.end()) {
            cached->second.used = ++recovery_archive_clock_;
            observed[path] = {identity, cached->second.facts};
            collect(*cached->second.facts);
          } else if (observed.count(path) && observed.at(path).first == identity) {
            collect(*observed.at(path).second);
          } else {
            const auto bytes = files_.read(path, size_t{8} * 1024 * 1024);
            if (bytes.find('\0') != std::string::npos)
              throw Error("Invalid checkpoint recovery JSON");
            const auto archive = Json::parse(bytes).get<Message>();
            if (archive.role != "system" || archive.origin != "recovery" ||
                archive.message_id != path.stem().string() ||
                digest(archive.text) != archive.message_id || !reference_current(reference))
              throw Error("Invalid checkpoint recovery archive identity");
            auto facts = std::make_shared<const Json>(Json::parse(archive.text));
            collect(*facts);
            observed[path] = {identity, facts};
            constexpr size_t limit = size_t{8} * 1024 * 1024;
            const auto size = archive.text.size();
            if (size <= limit) {
              while (size > limit - recovery_archive_bytes_ && !recovery_archives_.empty()) {
                const auto oldest = std::min_element(
                    recovery_archives_.begin(), recovery_archives_.end(),
                    [](const auto &a, const auto &b) { return a.second.used < b.second.used; });
                recovery_archive_bytes_ -= oldest->second.bytes;
                recovery_archives_.erase(oldest);
              }
              recovery_archives_.emplace(path, RecoveryArchive{identity, std::move(facts), size,
                                                               ++recovery_archive_clock_});
              recovery_archive_bytes_ += size;
            }
          }
        }
      } catch (const std::bad_alloc &) {
        throw;
      } catch (const std::exception &error) {
        if (saved.is_object() && saved.value("recovery_record", Json()).is_string()) {
          const auto entry =
              observation.archives.find(saved.at("recovery_record").get<std::string>());
          if (entry != observation.archives.end())
            entry->second.error = bounded_text(error.what(), 512);
        }
        if (journal.is_null())
          journal = recover ? recover() : Json::array();
        collect(journal);
        // The archive's membership cannot be proven from a fork's surviving
        // journal. Keep the original reference even when some facts are rebuilt.
        Json missing = {
            {"kind", "recovery_unavailable"},
            {"recovery", "unknown"},
            {"checkpoint_state", saved},
            {"reason", bounded_text(error.what(), 512)},
            {"instruction",
             "Historical outcomes are incomplete; inspect effects before repeating actions."}};
        const auto identity = missing.dump();
        unavailable[identity] = std::move(missing);
      }
    }
    if (message.role != "tool")
      continue;
    const auto envelope = Json::parse(message.text, nullptr, false);
    if (!envelope.is_object())
      continue;
    const auto meta = envelope.value("metadata", Json::object());
    if (meta.is_object() && meta.contains("recovery"))
      recovery[message.tool_call_id] = {
          {"call_id", message.tool_call_id},
          {"recovery", meta.at("recovery")},
          {"action_completed", meta.value("action_completed", false)}};
    else
      recovery.erase(message.tool_call_id);
  }
  Json facts = Json::array();
  for (const auto &entry : recovery)
    facts.push_back(entry.second);
  for (const auto &entry : unavailable)
    facts.push_back(entry.second);
  // Only this Tools instance owns execution handles. Historical IDs are never live after resume.
  for (const auto &entry : exec_sessions_)
    if (!entry.second.completed)
      facts.push_back(execution_fact(entry.first, entry.second));
  observation.facts = std::move(facts);
  return observation;
}
std::string Tools::execution_context(const Json &facts) {
  auto text = facts.dump();
  if (text.size() > 2048) {
    const auto root = invocation_directory(config_);
    const auto dir = root / "runtime";
    const auto path = dir / (session_id_ + ".execution.json");
    if (session_id_.empty() || !private_anchor(root) || fs::is_symlink(dir) || fs::is_symlink(path))
      throw Error("Invalid live execution storage");
    fs::create_directories(dir);
    if (::chmod(dir.c_str(), 0700))
      throw Error("Cannot protect live execution directory");
    if (!private_regular(files_, root, path) || fs::file_size(path) != text.size() ||
        files_.read(path, text.size() + 1) != text)
      files_.atomic_write(path, text, true);
    text =
        Json(
            {{"records", facts.size()}, {"sha256", digest(text)}, {"live_snapshot", path.string()}})
            .dump();
  }
  return "Current execution and recovery facts (fresh runtime observation). "
         "Saved checkpoints are historical; historical handles are never live.\n" +
         text;
}
std::string Tools::retain_result(Message &message) {
  return retain_result_impl(message, nullptr);
}
std::string Tools::retain_result_impl(Message &message, RecoveryCandidate *candidate) {
  if (message.result_file.empty()) {
    if (message.message_id.empty())
      message.message_id = unique_id();
    const auto root = artifact_directory(config_);
    const fs::path path = result_reference(message);
    const auto ref = path.lexically_relative(root);
    if (!private_anchor(root) || fs::is_symlink(root) || fs::is_symlink(root / "results") ||
        fs::is_symlink(path.parent_path()) || fs::is_symlink(path))
      throw Error("Invalid private result storage");
    auto identity = safe_json(Json(message));
    identity.erase("previewed");
    const auto content = identity.dump();
    const auto hash = digest(content);
    if (candidate) {
      candidate->content_digest = hash;
      candidate->bytes = content.size();
    }
    const auto cached = archived_results_.find(message.message_id);
    bool published = cached != archived_results_.end() && cached->second.first == hash &&
                     cached->second.second == path.string() && private_regular(files_, root, path);
    if (!published && private_regular(files_, root, path)) {
      // Earlier archives included the optional presentation flag (at most 18 bytes).
      auto archived = Json::parse(files_.read(path, content.size() + 32), nullptr, false);
      if (!archived.is_object() ||
          (archived.contains("previewed") && !archived.at("previewed").is_boolean()))
        throw Error("Invalid archived message");
      archived.erase("previewed");
      if (archived.dump() != content)
        throw Error("Archived message identity changed");
      published = true;
    }
    if (!published) {
      fs::create_directories(path.parent_path());
      for (const auto &dir : {root, root / "results", path.parent_path()})
        if (::chmod(dir.c_str(), 0700))
          throw Error("Cannot protect private result directory");
      files_.atomic_write(path, content, true);
    }
    // Keep retry reuse bounded independently of session length.
    if (archived_results_.size() >= 256)
      archived_results_.clear();
    archived_results_[message.message_id] = {hash, path.string()};
    message.result_file = ref.string();
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
      throw HookFailure("Hook blocked PreToolUse: " + hook.reason);
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
      if (dynamic_cast<const HookFailure *>(&e)) {
        result.metadata["hook_stop"] = e.what();
        result.metadata["hook_failure"] = true;
      }
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
                 {"tool", call.name},
                 {"tool_input", invocation},
                 {"cwd", invocation_cwd},
                 {"error", e.what()}},
                cancel);
    } catch (const Error &hook_error) {
      if (hook_error.code == 130)
        throw;
    }
    Json metadata = Json::object();
    if (dynamic_cast<const HookFailure *>(&e)) {
      metadata["hook_stop"] = e.what();
      metadata["hook_failure"] = true;
      metadata["action_completed"] = false;
    }
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
    auto fresh = execution_request(call);
    authorize(fresh);
    fresh = execution_request(call);
    if (permissions_.decide(fresh) == Decision::deny)
      throw Error("Permission denied for read_file: " + fresh.target);
    const auto p = files_.canonical(*fresh.path);
    const auto number = [&](const char *key, uint64_t fallback, uint64_t maximum) {
      if (!a.contains(key))
        return fallback;
      if (!a.at(key).is_number_unsigned() && (!a.at(key).is_number_integer() || a.at(key) < 0))
        throw Error("Invalid inspection range");
      const auto value = a.at(key).get<uint64_t>();
      if (value > maximum)
        throw Error("Inspection range exceeds supported limit");
      return value;
    };
    const auto context_budget = inspection_budget_.value_or(std::numeric_limits<size_t>::max());
    const auto encoded = [&](const std::string &text, const Json &metadata) {
      Message page;
      page.role = "tool";
      page.message_id = "00000000-0000-0000-0000-000000000000";
      page.tool_call_id = call.id;
      page.previewed = true;
      page.text = Json({{"content", text}, {"is_error", false}, {"metadata", metadata}}).dump();
      return result_bytes(page, call.name);
    };
    if (fs::is_directory(p)) {
      if (a.contains("byte_offset") || a.contains("byte_limit"))
        throw Error("Directory paging uses offset and limit");
      const auto offset = number("offset", 1, uint64_t{1} << 62);
      const auto count = number("limit", 2000, 100000);
      if (!offset || !count)
        throw Error("Invalid inspection range");
      std::vector<fs::directory_entry> candidates;
      bool bounded = false;
      const auto context = permissions_.context();
      size_t visited = 0;
      for (const auto &entry : fs::directory_iterator(p)) {
        cancel.check();
        if (++visited > 10000) {
          bounded = true;
          break;
        }
        PermissionRequest candidate{"read_file", entry.path().string(), entry.path(), false};
        candidate.internal_read = private_read(entry.path());
        if (permissions_.decide(candidate, context) != Decision::deny)
          candidates.push_back(entry);
      }
      std::sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
        return left.path().filename().native() < right.path().filename().native();
      });
      // False is the longer JSON spelling, reserving room for either flag value.
      const Json page_meta = {{"next_entry_offset", UINT64_MAX},
                              {"eof", false},
                              {"truncated", false},
                              {"scan_limited", false}};
      const auto budget =
          std::min(context_budget, config_.tool_output_limit + encoded("", page_meta));
      Json entries = Json::array();
      const auto start = std::min<uint64_t>(offset - 1, candidates.size());
      size_t at = static_cast<size_t>(start);
      const auto empty_cost = encoded("", page_meta);
      size_t page_cost =
          encoded(Json({{"entries", entries}, {"truncated", false}}).dump(), page_meta);
      for (; at < candidates.size() && at - start < count; ++at) {
        const auto &entry = candidates[at];
        Json row = {{"name", safe_text(entry.path().filename().string())},
                    {"type", entry.is_symlink()     ? "symlink"
                             : entry.is_directory() ? "directory"
                                                    : "file"}};
        const auto addition =
            encoded(row.dump(), page_meta) - empty_cost + (entries.empty() ? 0 : 1);
        if (addition > budget || page_cost > budget - addition)
          break;
        entries.push_back(std::move(row));
        page_cost += addition;
      }
      if (at == start && at < candidates.size())
        throw Error("Inspection budget cannot fit one directory entry; increase context_tokens");
      const bool more = at < candidates.size();
      return {Json({{"entries", entries}, {"truncated", more || bounded}}).dump(),
              false,
              {{"next_entry_offset", at + 1},
               {"eof", !more},
               {"truncated", more || bounded},
               {"scan_limited", bounded}}};
    }
    auto opened = files_.open_read(p);
    fresh = execution_request(call);
    if (opened->path() != files_.canonical(*fresh.path) ||
        permissions_.decide(fresh) == Decision::deny)
      throw Error("Inspection target changed or permission denied while opening");
    auto actual = fresh;
    actual.path = opened->path();
    if (permissions_.decide(actual) == Decision::deny)
      throw Error("Permission denied for opened inspection target");
    const bool bytes = a.contains("byte_offset") || a.contains("byte_limit");
    if (bytes && (a.contains("offset") || a.contains("limit")))
      throw Error("Choose line paging or byte paging");
    const auto offset = bytes ? number("byte_offset", 0, uint64_t{1} << 62)
                              : number("offset", 1, uint64_t{1} << 62);
    const auto count = bytes ? number("byte_limit", 65536, 1048576) : number("limit", 2000, 100000);
    if (!count || (!bytes && !offset))
      throw Error("Invalid inspection range");
    Json maximum = {{"total_bytes", opened->size()},
                    {"next_byte_offset", UINT64_MAX},
                    {"bytes_read", UINT64_MAX},
                    {"eof", false}};
    if (!bytes) {
      maximum["next_line"] = UINT64_MAX;
      maximum["total_lines"] = UINT64_MAX;
    }
    const auto fixed = encoded("", maximum);
    const auto budget = std::min(context_budget, fixed + config_.tool_output_limit);
    // Hooks may add bounded feedback after the read. Reserve it only when configured.
    const auto annotations = config_.hooks.empty() ? 0 : config_.tool_output_limit / 3;
    const auto cap = budget > fixed + annotations ? budget - fixed - annotations : 0;
    if (cap < 4)
      throw Error("Inspection budget cannot fit a text page; increase context_tokens");
    if (bytes)
      opened->seek(offset);
    std::string text;
    uint64_t consumed = 0, line = 1, next_line = offset;
    bool eof = false, full = false, any = false;
    char last = 0;
    size_t cost = 0;
    static const auto weights = [] {
      std::array<size_t, 256> out{};
      for (size_t i = 0; i < out.size(); ++i) {
        std::string value(1, static_cast<char>(i));
        out[i] = i >= 128 ? 3 : Json(value).dump().size() - 2;
      }
      return out;
    }();
    char buffer[4096];
    while (!full && (bytes ? consumed < count : line < offset + count)) {
      cancel.check();
      const auto want =
          bytes ? static_cast<size_t>(std::min<uint64_t>(sizeof buffer, count - consumed))
                : sizeof buffer;
      const auto n = opened->read(buffer, want);
      if (!n) {
        eof = true;
        break;
      }
      size_t valid_until = 0;
      for (size_t i = 0; i < n; ++i) {
        if (!bytes && line >= offset + count)
          break;
        const char ch = buffer[i];
        if (ch == '\0')
          throw Error("Binary file cannot be read as text");
        const bool selected = bytes || line >= offset;
        const auto prefix = !bytes && selected && (text.empty() || text.back() == '\n')
                                ? std::to_string(line) + "\t"
                                : "";
        const auto lead = static_cast<unsigned char>(ch);
        if (lead >= 0xc2 && i >= valid_until) {
          const size_t width = lead <= 0xdf ? 2 : lead <= 0xef ? 3 : lead <= 0xf4 ? 4 : 0;
          bool valid = width && width <= n - i;
          for (size_t j = 1; valid && j < width; ++j) {
            const auto next = static_cast<unsigned char>(buffer[i + j]);
            const auto low = j == 1 && lead == 0xe0 ? 0xa0 : j == 1 && lead == 0xf0 ? 0x90 : 0x80;
            const auto high = j == 1 && lead == 0xed ? 0x9f : j == 1 && lead == 0xf4 ? 0x8f : 0xbf;
            valid = next >= low && next <= high;
          }
          if (valid)
            valid_until = i + width;
        }
        const auto addition = (lead >= 128 && i < valid_until ? 1 : weights[lead]) + prefix.size() +
                              (prefix.empty() ? 0 : 1);
        if (selected && cost + addition > cap) {
          full = true;
          break;
        }
        ++consumed;
        any = true;
        last = ch;
        if (selected) {
          text += prefix;
          text += ch;
          cost += addition;
          next_line = line;
        }
        if (!bytes && ch == '\n') {
          ++line;
          next_line = line;
        }
      }
    }
    auto position = (bytes ? offset : 0) + consumed;
    eof = eof || position >= opened->size();
    if (!eof && !text.empty()) {
      size_t lead = text.size() - 1;
      while (lead && (static_cast<unsigned char>(text[lead]) & 0xc0) == 0x80)
        --lead;
      const auto ch = static_cast<unsigned char>(text[lead]);
      const size_t width = ch >= 0xc2 && ch <= 0xdf   ? 2
                           : ch >= 0xe0 && ch <= 0xef ? 3
                           : ch >= 0xf0 && ch <= 0xf4 ? 4
                                                      : 0;
      if (width && text.size() - lead < width) {
        const auto pending = text.size() - lead;
        text.resize(lead);
        consumed -= pending;
        position -= pending;
        if (!consumed)
          throw Error("byte_limit is too small for a Unicode character; use at least 4");
      }
    }
    ToolResult out{safe_text(text),
                   false,
                   {{"total_bytes", opened->size()},
                    {"next_byte_offset", position},
                    {"bytes_read", consumed},
                    {"eof", eof}}};
    if (!bytes) {
      out.metadata["next_line"] = next_line;
      if (eof)
        out.metadata["total_lines"] = any ? line - (last == '\n' ? 1 : 0) : 0;
    }
    if (encoded(out.content, out.metadata) > budget)
      throw Error("Encoded inspection page exceeded its budget");
    return out;
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
      struct stat st {};
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
    auto fresh = execution_request(call);
    authorize(fresh);
    auto opened = files_.open_read(files_.canonical(*fresh.path));
    fresh = execution_request(call);
    if (opened->path() != files_.canonical(*fresh.path) ||
        permissions_.decide(fresh) == Decision::deny)
      throw Error("Image target changed or permission denied while opening");
    auto actual = fresh;
    actual.path = opened->path();
    if (permissions_.decide(actual) == Decision::deny)
      throw Error("Permission denied for opened image");
    if (opened->size() > image_file_limit)
      throw Error("Image file exceeds 8 MiB");
    std::string bytes;
    char buffer[8192];
    for (;;) {
      cancel.check();
      const auto n = opened->read(buffer, sizeof buffer);
      if (!n)
        break;
      if (bytes.size() + n > image_file_limit)
        throw Error("Image grew beyond 8 MiB");
      bytes.append(buffer, n);
    }
    if (fresh.internal_read && digest(bytes) != opened->path().stem().string())
      throw Error("Generated image artifact checksum mismatch");
    const auto detail = a.value("detail", std::string("high"));
    auto artifact = persist_image(config_, files_, images_->load_bytes(bytes, detail), detail);
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
      const auto plans = invocation_directory(config_) / "plans";
      fs::create_directories(plans);
      const auto target = plans / (session_id_ + ".json");
      if (damaged_plan_ && fs::exists(target)) {
        if (fs::is_symlink(target) || !fs::is_regular_file(target))
          throw Error("Saved plan must be a regular file");
        if (::chmod(target.c_str(), 0600))
          throw Error("Cannot protect damaged plan");
        fs::rename(target, plans / (session_id_ + ".corrupt-" + unique_id() + ".json"));
      }
      files_.atomic_write(target, serialized, true);
      damaged_plan_ = false;
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
std::string Tools::dynamic_context(bool anticipate_indexes) const {
  std::string out;
  if (!todos_.empty() || large_plan_) {
    out = "\n# Current task plan (bounded view; original step numbers)\n";
    out += "Full saved plan: " +
           (invocation_directory(config_) / "plans" / (session_id_ + ".json")).string() +
           "; inspect with read_file.\n";
    if (large_plan_)
      out += "Saved plan is too large to preview; inspect it in pages before changing it.\n";
    bool omitted = false;
    for (const auto *status : {"in_progress", "pending", "completed"}) {
      if (omitted)
        break;
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
          omitted = true;
          break;
        }
        out += line;
      }
    }
  }
  if (!session_id_.empty()) {
    const auto results = artifact_directory(config_) / "indexes" / (session_id_ + ".results.jsonl");
    if (anticipate_indexes || fs::exists(results))
      out += "\n# Retained tool results and captures\n" + results.string() +
             "\nPage this reference index with read_file in any mode. Paths are relative to "
             "the artifact root (results) or capture root (text captures). Pass listed paths "
             "directly to read_file; registered capture filenames resolve automatically. Captures "
             "may expire.\n";
    const auto index = artifact_index_path(artifact_directory(config_), session_id_);
    if (anticipate_indexes || fs::exists(index))
      out += "\n# Historical image index\n" + index.string() + "\nPosition: " +
             (anticipate_indexes
                  ? Json({{"version", 2},
                          {"kind", "position"},
                          {"observations", UINT64_MAX},
                          {"legacy_unknown", UINT64_MAX},
                          {"last_sequence", UINT64_MAX},
                          {"latest_line", UINT64_MAX}})
                  : image_history_position(artifact_directory(config_), files_, session_id_))
                 .dump() +
             "\nArtifact root: " + artifact_directory(config_).string() +
             "\nPage this JSONL observation index with read_file in any mode. Resolve relative "
             "image paths under the artifact root and reload selected images with view_image. "
             "Compaction archives presented attachments; images awaiting a model response remain "
             "attached.\n";
  }
  return out;
}
fs::path Tools::archive_images(const std::vector<Message> &messages) {
  return archive_image_history(artifact_directory(config_), files_, session_id_, messages);
}
void Tools::use_session(const std::string &id) {
  if (id == session_id_)
    return;
  const auto root = artifact_directory(config_);
  if (fs::exists(artifact_index_path(root, id))) {
    if (!private_anchor(root))
      throw Error("Artifact storage changed before session inspection");
    archive_image_history(root, files_, id, {});
  }
  session_id_ = id;
  referenced_results_.clear();
  large_plan_ = false;
  todos_ = Json::array();
  damaged_plan_ = false;
  const auto p = invocation_directory(config_) / "plans" / (id + ".json");
  if (fs::exists(p) || fs::is_symlink(p)) {
    if (fs::is_symlink(p) || !fs::is_regular_file(p))
      throw Error("Saved plan must be a regular file");
    if (fs::file_size(p) > 1048576) {
      large_plan_ = true;
      damaged_plan_ = true;
      report_diagnostic(diagnostics_, "Saved task plan exceeds 1 MiB; preserved at " + p.string() +
                                          "; resuming without an active plan");
      return;
    }
    const auto bytes = files_.read(p, 1048576);
    todos_ = bytes.find('\0') == std::string::npos ? Json::parse(bytes, nullptr, false) : Json();
    bool valid = todos_.is_array() && todos_.size() <= 100;
    size_t active = 0;
    if (valid)
      for (const auto &step : todos_) {
        const auto status = step.is_object() ? step.value("status", Json()) : Json();
        valid = valid && step.is_object() && step.contains("step") && step["step"].is_string() &&
                (status == "pending" || status == "in_progress" || status == "completed");
        active += status == "in_progress";
      }
    if (!valid || active > 1) {
      todos_ = Json::array();
      damaged_plan_ = true;
      report_diagnostic(diagnostics_, "Invalid saved task plan preserved at " + p.string() +
                                          "; resuming without an active plan");
    }
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
