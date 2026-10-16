#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/patch.hpp"
#include <algorithm>
#include <fnmatch.h>
#include <iostream>
#include <openssl/sha.h>
#include <set>
#include <sstream>
#include <sys/stat.h>

namespace codet {
namespace {
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
    auto pattern = rule.pattern;
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
           pattern[prefix.size()] == '/')) {
        known_prefix = true;
        base = root;
        pattern = pattern.size() == prefix.size() ? "" : pattern.substr(prefix.size() + 1);
        break;
      }
    }
    // Unanchored authored globs may match absolute aliases outside cwd. Known filesystem
    // prefixes must never participate in raw glob matching (including allow rules).
    if (!known_prefix && (!fnmatch(pattern.c_str(), r.target.c_str(), 0) ||
                          !fnmatch(pattern.c_str(), requested.c_str(), 0) ||
                          !fnmatch(pattern.c_str(), target.c_str(), 0)))
      return true;
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
      if (!fnmatch(candidate.c_str(), requested.c_str(), 0) ||
          !fnmatch(candidate.c_str(), target.c_str(), 0))
        return true;
    }
    return false;
  }
};
std::string digest(const std::string &value) {
  unsigned char sum[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), sum);
  std::string out;
  const char *hex = "0123456789abcdef";
  for (unsigned char c : sum) {
    out += hex[c >> 4];
    out += hex[c & 15];
  }
  return out;
}
std::vector<std::string> lines(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream in(s);
  std::string l;
  while (std::getline(in, l))
    out.push_back(l);
  return out;
}
std::string html_text(const std::string &s) {
  std::string out;
  bool tag = false;
  std::string name, skip;
  for (char c : s) {
    if (c == '<') {
      tag = true;
      name.clear();
      continue;
    }
    if (tag) {
      if (c == '>') {
        tag = false;
        std::string n = name;
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char x) { return static_cast<char>(std::tolower(x)); });
        if (n.rfind("script", 0) == 0)
          skip = "script";
        if (n.rfind("style", 0) == 0)
          skip = "style";
        if (n == "/" + skip)
          skip.clear();
        if (skip.empty())
          out += '\n';
      } else
        name += c;
      continue;
    }
    if (skip.empty())
      out += c;
  }
  for (const auto &e : std::vector<std::pair<std::string, std::string>>{{"&amp;", "&"},
                                                                        {"&lt;", "<"},
                                                                        {"&gt;", ">"},
                                                                        {"&quot;", "\""},
                                                                        {"&#39;", "'"},
                                                                        {"&nbsp;", " "}}) {
    size_t pos = 0;
    while ((pos = out.find(e.first, pos)) != std::string::npos) {
      out.replace(pos, e.first.size(), e.second);
      pos += e.second.size();
    }
  }
  return out;
}
Json property(const char *type) {
  return {{"type", type}};
}
struct Change {
  fs::path path;
  std::optional<std::string> before, after;
  mode_t mode = 0644;
};
ToolResult process_result(const ProcessResult &r, bool background) {
  Json metadata = {{"exit_code", r.exit_code},
                   {"truncated", r.truncated},
                   {"total_output_bytes", r.total_output_bytes}};
  if (background)
    metadata["running"] = r.running;
  if (!r.spill_path.empty())
    metadata["spill_path"] = r.spill_path;
  if (!r.spill_path.empty() || r.spill_truncated || !r.capture_error.empty()) {
    metadata["spill_bytes"] = r.spill_bytes;
    metadata["spill_truncated"] = r.spill_truncated;
  }
  if (!r.capture_error.empty())
    metadata["capture_error"] = r.capture_error;
  return {r.output, r.exit_code != 0 && (!background || !r.running), metadata, r.preview,
          r.capture};
}
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
  std::vector<std::string> allow_source, deny_source;
  std::vector<ParsedRule> allow, deny;
};
void Permissions::refresh_rules() const {
  if (rules_ && rules_->allow_source == config_.allow && rules_->deny_source == config_.deny)
    return;
  auto next = std::make_shared<Rules>();
  next->allow_source = config_.allow;
  next->deny_source = config_.deny;
  for (const auto &rule : config_.allow)
    next->allow.emplace_back(rule);
  for (const auto &rule : config_.deny)
    next->deny.emplace_back(rule);
  rules_ = std::move(next);
}
bool Permissions::has_scoped_denials(const std::string &tool) const {
  refresh_rules();
  return std::any_of(rules_->deny.begin(), rules_->deny.end(),
                     [&](const auto &rule) { return rule.scoped && rule.applies(tool); });
}
Decision Permissions::decide(const PermissionRequest &r) const {
  refresh_rules();
  RulePaths paths(config_, files_);
  for (const auto &rule : rules_->deny)
    if (paths.matches(rule, r))
      return Decision::deny;
  if (config_.permission_mode == "plan" && r.mutation)
    return Decision::deny;
  if (config_.permission_mode == "bypass")
    return Decision::allow;
  if (r.path) {
    const auto &p = paths.resolve(r);
    bool found = false;
    for (const auto &root : config_.roots)
      if (within(p, paths.canonical(root)))
        found = true;
    const auto spill = capture_directory(config_);
    if (r.tool == "Read" && !fs::is_symlink(spill) && within(p, paths.canonical(spill)))
      found = true;
    if (!found)
      return Decision::deny;
  }
  for (const auto &rule : rules_->allow)
    if (paths.matches(rule, r) && (!rule.scoped || (r.tool != "Bash" && r.tool != "TaskCreate") ||
                                   literal_shell_command(r.target)))
      return Decision::allow;
  if (!r.mutation)
    return Decision::allow;
  if (config_.permission_mode == "accept-edits" &&
      (r.tool == "Write" || r.tool == "Edit" || r.tool == "BatchEdit" || r.tool == "ApplyPatch" ||
       r.tool == "Memory"))
    return Decision::allow;
  return Decision::ask;
}
fs::path Tools::path(const std::string &value) const {
  if (value.empty())
    throw Error("Path cannot be empty");
  return files_.canonical(config_.cwd / fs::path(value));
}
void Tools::authorize(const PermissionRequest &r) {
  if (plan_ && r.mutation)
    throw Error("Permission denied: plan mode is read-only");
  const auto d = permissions_.decide(r);
  if (d == Decision::deny)
    throw Error("Permission denied for " + r.tool + ": " + r.target);
  if (d == Decision::ask && active_cancel_ && config_.hooks.contains("PermissionRequest")) {
    const auto hook = run_hooks("PermissionRequest",
                                {{"hook_event_name", "PermissionRequest"},
                                 {"tool_name", r.tool},
                                 {"target", r.target},
                                 {"cwd", config_.cwd.string()}},
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
             Approver a, Questioner q, DiagnosticSink diagnostics)
    : config_(c), files_(f), processes_(p), http_(h), clock_(clock), permissions_(c, f),
      approve_(std::move(a)), question_(std::move(q)), diagnostics_(std::move(diagnostics)),
      capture_store_(std::make_shared<CaptureStore>(capture_directory(c), size_t{8} * 1024 * 1024,
                                                    size_t{64} * 1024 * 1024, 64, diagnostics_)) {}
Json Tools::schemas() const {
  Json out = Json::array();
  auto add = [&](const char *name, const char *desc, Json props, Json required) {
    out.push_back(
        {{"name", name},
         {"description", desc},
         {"input_schema", {{"type", "object"}, {"properties", props}, {"required", required}}}});
  };
  add("Read", "Read a text file before editing. Paths are workspace-relative or absolute.",
      {{"file_path", property("string")},
       {"offset", property("integer")},
       {"limit", property("integer")}},
      Json::array({"file_path"}));
  add("Write", "Atomically write a file. Existing files must have been read and unchanged.",
      {{"file_path", property("string")}, {"content", property("string")}},
      Json::array({"file_path", "content"}));
  add("Edit", "Replace an exact unique string; read the file first.",
      {{"file_path", property("string")},
       {"old_string", property("string")},
       {"new_string", property("string")},
       {"replace_all", property("boolean")}},
      Json::array({"file_path", "old_string", "new_string"}));
  add("BatchEdit",
      "Validate then apply edits across files, restoring previous content if a write fails.",
      {{"edits",
        {{"type", "array"},
         {"items",
          {{"type", "object"},
           {"properties",
            {{"file_path", property("string")},
             {"old_string", property("string")},
             {"new_string", property("string")}}},
           {"required", Json::array({"file_path", "old_string", "new_string"})}}}}}},
      Json::array({"edits"}));
  add("ApplyPatch",
      "Apply a Codex *** Begin Patch envelope. Supports Add/Delete/Update/Move; read existing "
      "files first.",
      {{"patch", property("string")}}, Json::array({"patch"}));
  add("Bash",
      "Execute a shell command under application permission controls; this is not an OS sandbox.",
      {{"command", property("string")},
       {"timeout_ms", property("integer")},
       {"pty", property("boolean")},
       {"run_in_background", property("boolean")}},
      Json::array({"command"}));
  add("Glob",
      "Find workspace files with a glob pattern; skips generated and version-control directories.",
      {{"pattern", property("string")}}, Json::array({"pattern"}));
  add("WebFetch", "Fetch an HTTP(S) URL and return bounded readable text.",
      {{"url", property("string")}}, Json::array({"url"}));
  add("WebSearch",
      "Search DuckDuckGo or configured SEARXNG_URL, BRAVE_SEARCH_API_KEY, SERPLY_API_KEY.",
      {{"query", property("string")}}, Json::array({"query"}));
  add("TaskCreate", "Start a background shell job for this process lifetime.",
      {{"command", property("string")},
       {"timeout_ms", property("integer")},
       {"pty", property("boolean")}},
      Json::array({"command"}));
  add("TaskInput", "Send input to a running PTY shell job.",
      {{"task_id", property("string")}, {"text", property("string")}},
      Json::array({"task_id", "text"}));
  add("TaskOutput", "Read a background job result.", {{"task_id", property("string")}},
      Json::array({"task_id"}));
  add("TaskList", "List shell jobs in this Codet process.", Json::object(), Json::array());
  add("TaskStop", "Stop a background job and its process group.", {{"task_id", property("string")}},
      Json::array({"task_id"}));
  add("TodoWrite", "Replace the current task plan.",
      {{"todos",
        {{"type", "array"},
         {"items",
          {{"type", "object"},
           {"properties", {{"content", property("string")}, {"status", property("string")}}},
           {"required", Json::array({"content", "status"})}}}}}},
      Json::array({"todos"}));
  add("AskUserQuestion",
      "Ask for a text answer, optionally suggesting choices. Noninteractive runs cannot answer.",
      {{"question", property("string")},
       {"options", {{"type", "array"}, {"items", property("string")}, {"maxItems", 12}}}},
      Json::array({"question"}));
  add("EnterPlanMode", "Temporarily restrict tools to reads.", Json::object(), Json::array());
  add("ExitPlanMode", "Request permission to leave read-only plan mode.", Json::object(),
      Json::array());
  add("Memory", "List/read/write durable Markdown memory. Names are simple .md filenames.",
      {{"action", property("string")},
       {"name", property("string")},
       {"content", property("string")}},
      Json::array({"action"}));
  add("Skill", "Load an installed skill's SKILL.md by name.", {{"name", property("string")}},
      Json::array({"name"}));
  add("Sleep", "Wait up to 60 seconds, cancellably.", {{"seconds", property("integer")}},
      Json::array({"seconds"}));
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
  try {
    cancel.check();
    if (call.name == "Grep")
      throw Error("Grep is retired; use Bash to run system grep, subject to shell permissions");
    if (!call.arguments.is_object())
      throw Error("Tool arguments must be an object");
    bool known = false;
    for (const auto &s : schemas())
      if (s["name"] == call.name)
        known = true;
    if (!known)
      throw Error("Unknown tool: " + call.name);
    const auto preflight = [&](const ToolCall &candidate) {
      const bool mutation = candidate.name == "Write" || candidate.name == "Edit" ||
                            candidate.name == "BatchEdit" || candidate.name == "ApplyPatch";
      const auto check = [&](const std::string &value) {
        const auto requested = (config_.cwd / fs::path(value)).lexically_normal();
        if (permissions_.decide({candidate.name, value, requested, mutation}) == Decision::deny)
          throw Error("Permission denied for " + candidate.name + ": " + value);
      };
      if (candidate.arguments.contains("file_path"))
        check(candidate.arguments.at("file_path"));
      else if (candidate.name == "Glob")
        check(candidate.arguments.value("path", "."));
      else if (candidate.name == "BatchEdit" && candidate.arguments.contains("edits")) {
        for (const auto &edit : candidate.arguments.at("edits"))
          check(edit.at("file_path"));
      } else if (candidate.name == "ApplyPatch" && candidate.arguments.contains("patch")) {
        for (const auto &line : lines(candidate.arguments.at("patch")))
          for (const auto &prefix :
               {"*** Update File: ", "*** Add File: ", "*** Delete File: ", "*** Move to: "})
            if (line.rfind(prefix, 0) == 0)
              check(line.substr(std::string(prefix).size()));
      } else {
        const auto target =
            candidate.arguments.value("command", candidate.arguments.value("name", ""));
        if (permissions_.decide({candidate.name, target, std::nullopt, false}) == Decision::deny)
          throw Error("Permission denied for " + candidate.name);
      }
    };
    preflight(call);
    ToolCall effective = call;
    const auto hook = run_hooks("PreToolUse",
                                {{"hook_event_name", "PreToolUse"},
                                 {"tool_name", call.name},
                                 {"tool_input", call.arguments},
                                 {"cwd", config_.cwd.string()}},
                                cancel);
    if (hook.blocked || hook.stopped)
      throw Error("Hook blocked PreToolUse: " + hook.reason);
    if (hook.updated_input)
      effective.arguments = *hook.updated_input;
    preflight(effective);
    auto result = execute_inner(effective, cancel);
    const bool process_output = result.metadata.contains("total_output_bytes");
    if (!result.capture) {
      result.capture = std::make_shared<OutputCapture>(capture_store_, config_.tool_output_limit);
      result.capture->append(result.content);
      result.capture->finish();
    }
    if (!result.preview)
      result.preview = result.capture->snapshot();
    std::string prefix, suffix;
    if (!hook.additional_context.empty())
      suffix = "\nHook context: " + hook.additional_context;
    result.content = annotated(*result.preview, config_.tool_output_limit, "",
                               feedback_text(suffix, config_.tool_output_limit / 3), "")
                         .text;
    try {
      const auto post = run_hooks("PostToolUse",
                                  {{"hook_event_name", "PostToolUse"},
                                   {"tool_name", call.name},
                                   {"tool_input", effective.arguments},
                                   {"tool_response", result.content},
                                   {"cwd", config_.cwd.string()}},
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
    const auto annotation_budget = config_.tool_output_limit / 3;
    const auto clean_prefix = safe_text(prefix), clean_suffix = safe_text(suffix);
    prefix = feedback_text(clean_prefix, annotation_budget);
    suffix = feedback_text(clean_suffix, annotation_budget - prefix.size());
    if (prefix.size() != clean_prefix.size() || suffix.size() != clean_suffix.size())
      result.metadata["hook_feedback_truncated"] = true;
    auto rendered = annotated(*result.preview, config_.tool_output_limit, prefix, suffix, "");
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
    if (rendered.omitted) {
      if (result.metadata.contains("spill_path")) {
        notice =
            "\n[Output saved to " + safe_text(result.metadata["spill_path"].get<std::string>());
        if (result.metadata.value("spill_truncated", false))
          notice += "; capture incomplete";
        notice += "]";
        if (notice.size() > config_.tool_output_limit / 3)
          notice = result.metadata.value("spill_truncated", false)
                       ? "\n[Capture incomplete; see metadata.spill_path]"
                       : "\n[Output saved; see metadata.spill_path]";
      } else
        notice = "\n[Output omitted; disk capture unavailable]";
      rendered = annotated(*result.preview, config_.tool_output_limit, prefix, suffix, notice);
    }
    if (process_output || rendered.omitted || result.metadata.contains("spill_path")) {
      result.metadata["total_output_bytes"] = result.preview->total;
      result.metadata["omitted_output_bytes"] = rendered.omitted;
      result.metadata["truncated"] = rendered.omitted != 0;
    }
    result.content = std::move(rendered.text);
    result.metadata = safe_json(result.metadata);
    return result;
  } catch (const Error &e) {
    if (e.code == 130)
      throw;
    try {
      run_hooks("PostToolUseFailure",
                {{"hook_event_name", "PostToolUseFailure"},
                 {"tool_name", call.name},
                 {"tool_input", call.arguments},
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
    return {bounded_text(std::string("Tool failed: ") + e.what(), config_.tool_output_limit), true,
            Json::object()};
  }
}
ToolResult Tools::execute_inner(const ToolCall &call, const Cancellation &cancel) {
  const auto &a = call.arguments;
  const auto &name = call.name;
  auto getpath = [&] { return path(a.at("file_path").get<std::string>()); };
  auto guard = [&](const fs::path &p) {
    if (fs::exists(p)) {
      const auto it = read_versions_.find(p);
      if (it == read_versions_.end())
        throw Error("Read the file before changing it: " + p.string());
      if (it->second != digest(files_.read(p)))
        throw Error("File changed since Read; read it again: " + p.string());
    }
  };
  if (name == "Read") {
    const auto p = getpath();
    authorize({name, p.string(), p, false});
    const auto content = files_.read(p);
    if (content.find('\0') != std::string::npos)
      throw Error("Binary file cannot be read as text");
    read_versions_[p] = digest(content);
    const auto ls = lines(content);
    const size_t first = a.value("offset", size_t{1}), limit = a.value("limit", size_t{2000});
    if (first == 0 || limit > 100000)
      throw Error("Invalid line range");
    std::string out;
    for (size_t i = first - 1; i < ls.size() && i - first + 1 < limit; ++i) {
      out += std::to_string(i + 1) + "\t" + ls[i] + "\n";
      if (out.size() > config_.tool_output_limit * 2)
        break;
    }
    return {out, false, {{"total_lines", ls.size()}, {"version", digest(content)}}};
  }
  if (name == "Write" || name == "Edit" || name == "BatchEdit" || name == "ApplyPatch") {
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
    auto edit = [&](const Json &e) {
      const auto p = path(e.at("file_path").get<std::string>());
      auto staged = std::find_if(changes.begin(), changes.end(),
                                 [&](const Change &x) { return x.path == p; });
      if (staged == changes.end()) {
        authorize({name, p.string(), p, true});
        guard(p);
      }
      auto text = staged == changes.end() ? files_.read(p) : staged->after.value();
      const auto old = e.at("old_string").get<std::string>(),
                 replacement = e.at("new_string").get<std::string>();
      if (old.empty())
        throw Error("old_string cannot be empty");
      size_t at = text.find(old);
      if (at == std::string::npos)
        throw Error("old_string was not found");
      const bool all = e.value("replace_all", false);
      if (!all && text.find(old, at + old.size()) != std::string::npos)
        throw Error("Ambiguous edit; provide unique context or replace_all");
      do {
        text.replace(at, old.size(), replacement);
        at = text.find(old, at + replacement.size());
      } while (all && at != std::string::npos);
      if (staged == changes.end())
        add(p, text, true);
      else
        staged->after = text;
    };
    if (name == "Write")
      add(getpath(), a.at("content").get<std::string>());
    else if (name == "Edit")
      edit(a);
    else if (name == "BatchEdit") {
      if (!a.at("edits").is_array() || a["edits"].empty() || a["edits"].size() > 100)
        throw Error("Batch requires 1..100 edits");
      for (const auto &e : a["edits"])
        edit(e);
    } else {
      const auto patch = a.at("patch").get<std::string>();
      auto ls = lines(patch);
      for (auto &line : ls)
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
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
    }
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
      read_versions_[change.path] = change.after ? digest(*change.after) : "<missing>";
      paths.push_back(change.path.string());
    }
    return {
        "Applied " + std::to_string(changes.size()) + " file change(s)", false, {{"paths", paths}}};
  }
  if (name == "Bash" || name == "TaskCreate") {
    const auto command = a.at("command").get<std::string>();
    authorize({name, command, std::nullopt, true});
    ProcessSpec spec;
    spec.argv = {"/bin/sh", "-c", command};
    spec.cwd = config_.cwd;
    spec.timeout_ms = a.value("timeout_ms", 120000);
    spec.pty = a.value("pty", false);
    spec.output_limit = config_.tool_output_limit;
    spec.spool_directory = capture_directory(config_);
    spec.capture_store = capture_store_;
    if (spec.timeout_ms < 1 || spec.timeout_ms > 3600000)
      throw Error("Timeout must be 1..3600000 ms");
    if (name == "TaskCreate" || a.value("run_in_background", false)) {
      const auto id = processes_.start(spec, cancel);
      return {id, false, {{"lifetime", "current codet process"}}};
    }
    const auto r = processes_.run(spec, cancel);
    return process_result(r, false);
  }
  if (name == "TaskInput") {
    const auto id = a.at("task_id").get<std::string>();
    authorize({name, id, std::nullopt, true});
    processes_.write(id, a.at("text").get<std::string>());
    return {"Input sent", false, Json::object()};
  }
  if (name == "TaskOutput" || name == "TaskStop") {
    const auto id = a.at("task_id").get<std::string>();
    authorize({name, id, std::nullopt, name == "TaskStop"});
    if (name == "TaskStop")
      processes_.stop(id);
    const auto r = processes_.poll(id);
    return process_result(r, true);
  }
  if (name == "TaskList") {
    Json jobs = Json::array();
    for (const auto &job : processes_.list()) {
      Json item = {{"id", job.id}, {"running", job.running}, {"exit_code", job.exit_code}};
      if (!job.error.empty())
        item["error"] = job.error;
      jobs.push_back(std::move(item));
    }
    return {jobs.dump(), false, Json::object()};
  }
  if (name == "Glob") {
    const auto start = a.contains("path") ? path(a["path"].get<std::string>()) : config_.cwd;
    authorize({name, start.string(), start, false});
    const auto pattern = a.at("pattern").get<std::string>();
    if (pattern.size() > 4096)
      throw Error("Pattern too large");
    const bool scoped_denials = permissions_.has_scoped_denials(name);
    const auto denied = [&](const fs::path &p) {
      return scoped_denials && permissions_.decide({name, p.string(), p, false}) == Decision::deny;
    };
    std::vector<fs::path> candidates;
    if (fs::is_regular_file(start))
      candidates.push_back(start);
    else {
      size_t visited = 0;
      for (fs::recursive_directory_iterator
               it(start, fs::directory_options::skip_permission_denied),
           end;
           it != end && visited++ < 10000; ++it) {
        cancel.check();
        const auto base = it->path().filename().string();
        if (it->is_directory() && denied(it->path())) {
          it.disable_recursion_pending();
          continue;
        }
        if (it->is_directory() &&
            (base == ".git" || base == ".deps" || base == "build" || base == "node_modules")) {
          it.disable_recursion_pending();
          continue;
        }
        if (it->is_symlink()) {
          if (it->is_directory())
            it.disable_recursion_pending();
          continue;
        }
        if (it->is_regular_file())
          candidates.push_back(it->path());
      }
    }
    std::sort(candidates.begin(), candidates.end());
    std::string out;
    for (const auto &p : candidates) {
      cancel.check();
      if (permissions_.decide({name, p.string(), p, false}) == Decision::deny)
        continue;
      const auto relative = fs::relative(p, config_.cwd).string();
      if (fnmatch(pattern.c_str(), relative.c_str(), 0) == 0 ||
          (pattern.rfind("**/", 0) == 0 &&
           fnmatch(pattern.substr(3).c_str(), relative.c_str(), 0) == 0))
        out += relative + "\n";
      if (out.size() > config_.tool_output_limit * 2)
        break;
    }
    return {out, false, Json::object()};
  }
  if (name == "WebFetch" || name == "WebSearch") {
    HttpRequest r;
    r.method = "GET";
    r.timeout_seconds = 20;
    r.max_bytes = size_t{2} * size_t{1024} * 1024;
    std::string backend = "html";
    if (name == "WebFetch")
      r.url = a.at("url");
    else {
      const auto query = url_encode(a.at("query").get<std::string>());
      if (!env("SEARXNG_URL").empty()) {
        r.url = env("SEARXNG_URL") + "/search?q=" + query + "&format=json";
        backend = "searxng";
      } else if (!env("BRAVE_SEARCH_API_KEY").empty()) {
        r.url = "https://api.search.brave.com/res/v1/web/search?q=" + query + "&count=5";
        r.headers["X-Subscription-Token"] = env("BRAVE_SEARCH_API_KEY");
        backend = "brave";
      } else if (!env("SERPLY_API_KEY").empty()) {
        r.url = "https://api.serply.io/v1/search/?q=" + query + "&num=5";
        r.headers["X-Api-Key"] = env("SERPLY_API_KEY");
        backend = "serply";
      } else {
        r.url = "https://api.duckduckgo.com/?q=" + query + "&format=json&no_html=1&skip_disambig=1";
        backend = "ddg";
      }
    }
    authorize({name, r.url, std::nullopt, false});
    const auto response = http_.perform(r, cancel);
    if (response.status < 200 || response.status >= 300)
      throw Error("Web request failed (HTTP " + std::to_string(response.status) + ")");
    if (backend == "html") {
      const auto first = response.body.find_first_not_of(" \t\r\n");
      return {first != std::string::npos && response.body[first] == '<' ? html_text(response.body)
                                                                        : response.body,
              false,
              {{"url", r.url}}};
    }
    const auto data = Json::parse(response.body);
    if (backend == "ddg") {
      std::string out;
      if (!data.value("Abstract", "").empty())
        out = data.value("AbstractSource", "") + "\n" + data.value("AbstractURL", "") + "\n" +
              data.value("Abstract", "") + "\n\n";
      size_t count = 0;
      std::function<void(const Json &)> topics = [&](const Json &list) {
        for (const auto &item : list) {
          if (count >= 5)
            break;
          if (item.contains("Topics"))
            topics(item["Topics"]);
          else if (!item.value("Text", "").empty()) {
            ++count;
            out += item.value("Text", "") + "\n" + item.value("FirstURL", "") + "\n\n";
          }
        }
      };
      topics(data.value("RelatedTopics", Json::array()));
      return {out.empty() ? "No DuckDuckGo instant answers. Configure SearXNG, Brave, or Serply "
                            "for general web search."
                          : out,
              false, Json::object()};
    }
    const auto results = backend == "brave"
                             ? data.value("web", Json::object()).value("results", Json::array())
                             : data.value("results", Json::array());
    std::string out;
    size_t count = 0;
    for (const auto &item : results) {
      if (++count > 5)
        break;
      out += item.value("title", "") + "\n" + item.value("url", item.value("link", "")) + "\n" +
             item.value("content", item.value("description", item.value("snippet", ""))) + "\n\n";
    }
    return {out, false, Json::object()};
  }
  if (name == "TodoWrite") {
    authorize({name, "task plan", std::nullopt, false});
    todos_ = a.at("todos");
    if (!todos_.is_array() || todos_.size() > 100)
      throw Error("todos must be an array of at most 100 items");
    if (!session_id_.empty()) {
      fs::create_directories(config_.home / "plans");
      files_.atomic_write(config_.home / "plans" / (session_id_ + ".json"), todos_.dump(), true);
    }
    return {todos_.dump(), false, Json::object()};
  }
  if (name == "EnterPlanMode") {
    plan_ = true;
    return {"Plan mode enabled", false, Json::object()};
  }
  if (name == "ExitPlanMode") {
    if (config_.approval == "never" || !approve_ ||
        !approve_({name, "Leave read-only plan mode", std::nullopt, true}))
      throw Error("Leaving plan mode requires explicit approval");
    plan_ = false;
    return {"Plan mode disabled", false, Json::object()};
  }
  if (name == "AskUserQuestion") {
    if (!question_)
      throw Error("Cannot ask the user in noninteractive mode");
    const auto question = a.at("question").get<std::string>();
    const auto options = a.value("options", std::vector<std::string>{});
    if (question.empty() || question.size() > 8192 || options.size() > 12)
      throw Error("Question exceeds supported limits");
    for (const auto &option : options)
      if (option.size() > 1024)
        throw Error("Question option exceeds supported limits");
    return {question_(question, options), false, Json::object()};
  }
  if (name == "Sleep") {
    const auto seconds = a.at("seconds").get<int>();
    if (seconds < 0 || seconds > 60)
      throw Error("Sleep must be 0..60 seconds");
    clock_.sleep_ms(seconds * 1000, cancel);
    return {"Wait complete", false, Json::object()};
  }
  if (name == "Memory") {
    const auto dir = config_.home / "memory";
    const auto action = a.at("action").get<std::string>();
    if (action == "list") {
      std::string out;
      if (fs::exists(dir))
        for (const auto &entry : fs::directory_iterator(dir))
          if (entry.path().extension() == ".md" && !entry.is_symlink())
            out += entry.path().filename().string() + "\n";
      return {out, false, Json::object()};
    }
    const auto file = a.at("name").get<std::string>();
    if (file.empty() || fs::path(file).filename() != file || file == ".." ||
        fs::path(file).extension() != ".md")
      throw Error("Memory name must be a simple .md filename");
    if (action == "read")
      return {files_.read(dir / file, 65536), false, Json::object()};
    if (action != "write")
      throw Error("Memory action must be list/read/write");
    authorize({name, file, std::nullopt, true});
    const auto content = a.at("content").get<std::string>();
    if (content.size() > 65536)
      throw Error("Memory file too large");
    fs::create_directories(dir);
    files_.atomic_write(dir / file, content, true);
    return {"Memory saved", false, Json::object()};
  }
  if (name == "Skill") {
    const auto skill = a.at("name").get<std::string>();
    if (skill.empty() || fs::path(skill).filename() != skill || skill == "..")
      throw Error("Invalid skill name");
    for (const auto &dir :
         std::vector<fs::path>{config_.cwd / ".codet/skills", config_.cwd / ".agents/skills",
                               config_.cwd / ".claude/skills", config_.home / "skills"}) {
      const auto p = dir / skill / "SKILL.md";
      if (fs::exists(p) && within(files_.canonical(p), files_.canonical(dir)))
        return {files_.read(p, 65536), false, Json::object()};
    }
    throw Error("Skill not found");
  }
  throw Error("Unknown tool");
}
std::string Tools::instructions() const {
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
        warn("Memory context budget exhausted; use Memory to read additional files");
        break;
      }
      if (!append("\n# Memory: " + p.string() + " (use Memory to read more)\n", cap))
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
  out += "\n# Available skills (load with Skill)\n";
  count = 0;
  for (const auto &dir :
       std::vector<fs::path>{config_.cwd / ".codet/skills", config_.cwd / ".agents/skills",
                             config_.cwd / ".claude/skills", config_.home / "skills"})
    if (fs::exists(dir))
      for (const auto &entry : fs::directory_iterator(dir))
        if (++count <= 100 && !entry.is_symlink() && fs::exists(entry.path() / "SKILL.md"))
          out += entry.path().filename().string() + "\n";
  if (!todos_.empty())
    out += "\n# Current task plan\n" + todos_.dump();
  return out;
}
void Tools::use_session(const std::string &id) {
  if (id == session_id_)
    return;
  session_id_ = id;
  todos_ = Json::array();
  plan_ = false;
  read_versions_.clear();
  const auto p = config_.home / "plans" / (id + ".json");
  if (fs::exists(p)) {
    todos_ = Json::parse(files_.read(p, 65536), nullptr, false);
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
