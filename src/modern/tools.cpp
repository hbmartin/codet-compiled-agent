#include "codet/core.hpp"
#include <algorithm>
#include <fnmatch.h>
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
bool rule_matches(const std::string &rule, const PermissionRequest &r) {
  const auto pos = rule.find('(');
  if (pos == std::string::npos)
    return fnmatch(rule.c_str(), r.tool.c_str(), 0) == 0;
  if (rule.empty() || rule.back() != ')')
    return false;
  return fnmatch(rule.substr(0, pos).c_str(), r.tool.c_str(), 0) == 0 &&
         fnmatch(rule.substr(pos + 1, rule.size() - pos - 2).c_str(), r.target.c_str(), 0) == 0;
}
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
std::string join_lines(const std::vector<std::string> &ls) {
  std::string out;
  for (const auto &l : ls) {
    out += l;
    out += '\n';
  }
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
} // namespace
Decision Permissions::decide(const PermissionRequest &r) const {
  for (const auto &rule : config_.deny)
    if (rule_matches(rule, r))
      return Decision::deny;
  if (config_.permission_mode == "plan" && r.mutation)
    return Decision::deny;
  if (config_.permission_mode == "bypass")
    return Decision::allow;
  if (r.path) {
    const auto p = files_.canonical(*r.path);
    bool found = false;
    for (const auto &root : config_.roots)
      if (within(p, files_.canonical(root)))
        found = true;
    const auto spill = config_.home / "tool-output";
    if (r.tool == "Read" && !fs::is_symlink(spill) && within(p, files_.canonical(spill)))
      found = true;
    if (!found)
      return Decision::deny;
  }
  for (const auto &rule : config_.allow)
    if (rule_matches(rule, r) &&
        (rule.find('(') == std::string::npos || (r.tool != "Bash" && r.tool != "TaskCreate") ||
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
    if (hook.value("permissionDecision", "") == "allow")
      return;
  }
  if (d == Decision::ask && (config_.approval == "never" || !approve_ || !approve_(r)))
    throw Error("Permission denied: " + r.tool +
                " requires approval. Do not retry without a policy change.");
}
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
Json Tools::run_hooks(const std::string &event, const Json &input, const Cancellation &cancel,
                      int budget_ms) {
  const auto began = std::chrono::steady_clock::now();
  Json combined = Json::object();
  if (!config_.hooks.contains(event))
    return combined;
  const auto &hooks = config_.hooks[event];
  if (!hooks.is_array())
    throw Error("Hook event must be an array");
  for (const auto &h : hooks) {
    const auto matcher = h.value("matcher", "*");
    if (fnmatch(matcher.c_str(), input.value("tool_name", "").c_str(), 0) != 0)
      continue;
    const auto command = h.at("command").get<std::string>();
    fs::create_directories(config_.home / "hook-input");
    const auto file = config_.home / "hook-input" / (unique_id() + ".json");
    files_.atomic_write(file, input.dump() + "\n", true);
    ProcessResult result;
    try {
      ProcessSpec spec;
      spec.cwd = config_.cwd;
      spec.argv = {"/bin/sh",    "-c",    "exec /bin/sh -c \"$1\" < \"$2\"",
                   "codet-hook", command, file.string()};
      spec.timeout_ms = h.value("timeout_ms", 10000);
      if (spec.timeout_ms < 1 || spec.timeout_ms > 120000)
        throw Error("Hook timeout must be 1..120000 ms");
      if (budget_ms > 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - began)
                                 .count();
        if (elapsed >= budget_ms)
          throw Error("Hook cleanup budget exhausted");
        spec.timeout_ms = std::min(spec.timeout_ms, static_cast<int>(budget_ms - elapsed));
      }
      result = processes_.run(spec, cancel);
      files_.remove(file);
    } catch (...) {
      try {
        files_.remove(file);
      } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original hook failure if cleanup fails.
      }
      throw;
    }
    if (result.exit_code == 2)
      throw Error("Hook blocked " + event + ": " + result.output);
    if (result.exit_code != 0)
      throw Error("Hook failed for " + event + " (exit " + std::to_string(result.exit_code) + ")");
    if (!result.output.empty()) {
      const auto output = Json::parse(result.output, nullptr, false);
      if (!output.is_object())
        throw Error("Hook output must be a JSON object");
      if (!output.value("continue", true) || output.value("decision", "") == "block")
        throw Error("Hook blocked " + event);
      if (output.contains("hookSpecificOutput")) {
        const auto &specific = output["hookSpecificOutput"];
        if (specific.value("permissionDecision", "") == "deny")
          throw Error("Hook denied tool execution");
        if (specific.contains("permissionDecision"))
          combined["permissionDecision"] = specific["permissionDecision"];
        if (specific.contains("updatedInput"))
          combined["updatedInput"] = specific["updatedInput"];
        if (specific.contains("additionalContext"))
          combined["additionalContext"] = specific["additionalContext"];
      }
    }
  }
  return combined;
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
    const auto target = call.arguments.value(
        "command", call.arguments.value("file_path", call.arguments.value("name", "")));
    if (permissions_.decide({call.name, target, std::nullopt, false}) == Decision::deny)
      throw Error("Permission denied for " + call.name);
    ToolCall effective = call;
    const auto hook = run_hooks("PreToolUse",
                                {{"hook_event_name", "PreToolUse"},
                                 {"tool_name", call.name},
                                 {"tool_input", call.arguments},
                                 {"cwd", config_.cwd.string()}},
                                cancel);
    if (hook.contains("updatedInput"))
      effective.arguments = hook["updatedInput"];
    auto result = execute_inner(effective, cancel);
    if (hook.contains("additionalContext"))
      result.content += "\nHook context: " + hook["additionalContext"].get<std::string>();
    try {
      run_hooks("PostToolUse",
                {{"hook_event_name", "PostToolUse"},
                 {"tool_name", call.name},
                 {"tool_input", effective.arguments},
                 {"tool_response", result.content},
                 {"cwd", config_.cwd.string()}},
                cancel);
    } catch (const Error &e) {
      if (e.code == 130)
        throw;
      result.content = "Tool completed; PostToolUse hook failed: " + std::string(e.what()) + "\n" +
                       result.content;
      result.error = true;
      result.metadata["action_completed"] = true;
    }
    if (result.content.size() > config_.tool_output_limit) {
      fs::create_directories(config_.home / "tool-output");
      const auto spill = config_.home / "tool-output" / (unique_id() + ".txt");
      files_.atomic_write(spill, result.content, true);
      result.content.resize(config_.tool_output_limit);
      result.content += "\n[Output truncated; saved to " + spill.string() + "]";
      result.metadata["spill_path"] = spill.string();
    }
    result.content =
        Json::parse(Json(result.content).dump(-1, ' ', false, Json::error_handler_t::replace))
            .get<std::string>();
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
    return {e.what(), true, Json::object()};
  } catch (const std::exception &e) {
    return {std::string("Tool failed: ") + e.what(), true, Json::object()};
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
      const auto ls = lines(patch);
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
        while (i + 1 < ls.size() && ls[i].rfind("*** ", 0) != 0) {
          if (ls[i].rfind("@@", 0) != 0)
            throw Error("Expected @@ patch hunk");
          ++i;
          std::vector<std::string> before, after;
          while (i + 1 < ls.size() && ls[i].rfind("@@", 0) != 0 && ls[i].rfind("*** ", 0) != 0) {
            const auto &l = ls[i++];
            if (l.empty())
              throw Error("Patch lines require a prefix");
            if (l[0] == ' ' || l[0] == '-')
              before.push_back(l.substr(1));
            if (l[0] == ' ' || l[0] == '+')
              after.push_back(l.substr(1));
            if (l[0] != ' ' && l[0] != '-' && l[0] != '+')
              throw Error("Invalid patch line");
          }
          if (i < ls.size() && ls[i] == "*** End of File")
            ++i;
          const auto old = join_lines(before), replacement = join_lines(after);
          if (old.empty())
            throw Error("Update hunk needs context");
          auto at = text.find(old);
          if (at == std::string::npos && !text.empty() && text.back() != '\n') {
            text += '\n';
            at = text.find(old);
          }
          if (at == std::string::npos || text.find(old, at + old.size()) != std::string::npos)
            throw Error("Patch context missing or ambiguous");
          text.replace(at, old.size(), replacement);
        }
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
    if (spec.timeout_ms < 1 || spec.timeout_ms > 3600000)
      throw Error("Timeout must be 1..3600000 ms");
    if (name == "TaskCreate" || a.value("run_in_background", false)) {
      const auto id = processes_.start(spec);
      task_ids_.push_back(id);
      return {id, false, {{"lifetime", "current codet process"}}};
    }
    const auto r = processes_.run(spec, cancel);
    return {r.output, r.exit_code != 0, {{"exit_code", r.exit_code}, {"truncated", r.truncated}}};
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
    return {r.output,
            r.exit_code != 0 && !r.running,
            {{"running", r.running}, {"exit_code", r.exit_code}, {"truncated", r.truncated}}};
  }
  if (name == "TaskList") {
    Json jobs = Json::array();
    for (const auto &id : task_ids_) {
      const auto r = processes_.poll(id);
      jobs.push_back({{"id", id}, {"running", r.running}, {"exit_code", r.exit_code}});
    }
    return {jobs.dump(), false, Json::object()};
  }
  if (name == "Glob") {
    const auto start = a.contains("path") ? path(a["path"].get<std::string>()) : config_.cwd;
    authorize({name, start.string(), start, false});
    const auto pattern = a.at("pattern").get<std::string>();
    if (pattern.size() > 4096)
      throw Error("Pattern too large");
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
  std::set<fs::path> seen;
  std::function<void(const fs::path &, int)> include = [&](const fs::path &input, int depth) {
    const auto p = files_.canonical(input);
    if (depth > 16 || seen.count(p) || !fs::exists(p) || out.size() >= 65536)
      return;
    if (!within(p, root) && !within(p, home))
      return;
    seen.insert(p);
    const auto text = files_.read(p, 65536);
    out += "\n# Instructions: " + p.string() + "\n";
    for (const auto &l : lines(text)) {
      if (l.rfind("@include ", 0) == 0)
        include(p.parent_path() / l.substr(9), depth + 1);
      else if (!l.empty() && l[0] == '@' && l.find(' ') == std::string::npos)
        include(p.parent_path() / l.substr(1), depth + 1);
      else
        out += l + "\n";
      if (out.size() >= 65536)
        break;
    }
  };
  for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
    include(*it / "AGENTS.md", 0);
    include(*it / "CLAUDE.md", 0);
  }
  const auto memory = config_.home / "memory";
  size_t count = 0;
  if (fs::exists(memory))
    for (const auto &entry : fs::directory_iterator(memory))
      if (!entry.is_symlink() && entry.is_regular_file() && entry.path().extension() == ".md" &&
          ++count <= 200 && out.size() < 98304) {
        auto text = files_.read(entry.path(), 65536);
        out += "\n# Memory: " + entry.path().filename().string() + "\n" +
               text.substr(0, 98304 - out.size());
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
