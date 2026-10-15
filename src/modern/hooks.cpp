#include "codet/core.hpp"
#include <fnmatch.h>
#include <iostream>

namespace codet {
HookOutcome Tools::run_hooks(const std::string &event, const Json &input,
                             const Cancellation &cancel, int budget_ms) {
  const auto began = std::chrono::steady_clock::now();
  HookOutcome out;
  size_t diagnostic_bytes = 0;
  const auto append = [&](std::string &target, const std::string &text) {
    target = bounded_text(target + (target.empty() ? "" : "\n") + text, config_.tool_output_limit);
  };
  const auto diagnose = [&](const std::string &text) {
    if (diagnostic_bytes >= config_.tool_output_limit)
      return;
    const auto message = bounded_text(text, config_.tool_output_limit - diagnostic_bytes);
    diagnostic_bytes += message.size();
    out.diagnostics.push_back(message);
    report_diagnostic(diagnostics_,
                      (event == "SessionEnd" ? "SessionEnd hook cleanup failed: " : "Hook: ") +
                          message);
  };
  if (!config_.hooks.contains(event))
    return out;
  const auto &hooks = config_.hooks[event];
  if (!hooks.is_array())
    throw Error("Hook event must be an array", 2);
  for (const auto &hook : hooks) {
    cancel.check();
    if (budget_ms > 0 &&
        std::chrono::steady_clock::now() - began >= std::chrono::milliseconds(budget_ms)) {
      diagnose("Hook cleanup budget exhausted");
      break;
    }
    fs::path file;
    ProcessResult result;
    try {
      auto matcher = hook.value("matcher", "*");
      const auto subject =
          event == "SessionStart" ? input.value("source", "startup") : input.value("tool_name", "");
      if (event == "SessionStart" && matcher.empty())
        matcher = "*";
      if (fnmatch(matcher.c_str(), subject.c_str(), 0))
        continue;
      ProcessSpec spec;
      spec.cwd = config_.cwd;
      spec.separate_stderr = true;
      spec.output_limit = config_.tool_output_limit;
      spec.timeout_ms = hook.value("timeout_ms", 10000);
      if (spec.timeout_ms < 1 || spec.timeout_ms > 120000)
        throw Error("Hook timeout must be 1..120000 ms");
      fs::create_directories(config_.home / "hook-input");
      file = config_.home / "hook-input" / (unique_id() + ".json");
      files_.atomic_write(file, safe_json(input).dump() + "\n", true);
      if (budget_ms > 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - began)
                                 .count();
        if (elapsed >= budget_ms)
          throw Error("Hook cleanup budget exhausted");
        spec.timeout_ms = std::min(spec.timeout_ms, static_cast<int>(budget_ms - elapsed));
      }
      spec.argv = {"/bin/sh",
                   "-c",
                   "exec /bin/sh -c \"$1\" < \"$2\"",
                   "codet-hook",
                   hook.at("command").get<std::string>(),
                   file.string()};
      result = processes_.run(spec, cancel);
      files_.remove(file);
      file.clear();
    } catch (const std::exception &error) {
      if (!file.empty()) {
        try {
          files_.remove(file);
        } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original hook error.
        }
      }
      cancel.check();
      if (dynamic_cast<const std::bad_alloc *>(&error))
        throw;
      if (const auto *e = dynamic_cast<const Error *>(&error); e && e->code == 130)
        throw;
      diagnose(event + ": " + error.what());
      continue;
    }
    if (result.stdout_truncated)
      diagnose(event + " stdout capture truncated");
    if (result.stderr_truncated)
      diagnose(event + " stderr capture truncated");
    const auto stdout_text = safe_text(result.output),
               stderr_text = safe_text(result.stderr_output);
    const auto nonempty = [](const std::string &s) {
      return s.find_first_not_of(" \t\r\n") != std::string::npos;
    };
    const bool can_block = event == "PreToolUse" || event == "PermissionRequest" ||
                           event == "PostToolUse" || event == "UserPromptSubmit" || event == "Stop";
    if (result.exit_code != 0) {
      if (result.exit_code == 2 && can_block && nonempty(stderr_text)) {
        out.blocked = true;
        append(out.reason, stderr_text);
        if (event == "PermissionRequest")
          out.permission = Decision::deny;
      } else
        diagnose(event + " exited with code " + std::to_string(result.exit_code) +
                 (stderr_text.empty() ? "" : ": " + stderr_text));
      continue;
    }
    if (nonempty(stderr_text)) {
      auto diagnostic = event;
      diagnostic += " stderr: ";
      diagnostic += stderr_text;
      diagnose(diagnostic);
    }
    if (event == "SessionEnd" || event == "PostToolUseFailure" || !nonempty(stdout_text))
      continue;
    const auto first = stdout_text.find_first_not_of(" \t\r\n");
    const bool json_like = stdout_text[first] == '{' || stdout_text[first] == '[';
    if (json_like && result.stdout_truncated) {
      diagnose(event + " returned truncated JSON output");
      continue;
    }
    const auto value = Json::parse(stdout_text, nullptr, false);
    if (!value.is_object()) {
      if (json_like || event == "Stop")
        diagnose(event + " returned invalid JSON output");
      else if (event == "SessionStart" || event == "UserPromptSubmit")
        append(out.additional_context, stdout_text);
      continue;
    }
    try {
      const auto specific = value.value("hookSpecificOutput", Json::object());
      if (!specific.is_object())
        throw Error("hookSpecificOutput must be an object");
      const auto context = specific.value("additionalContext", "");
      if (value.contains("systemMessage"))
        diagnose(event + ": " + value.at("systemMessage").get<std::string>());
      bool block = false, stop = false;
      std::string reason = value.value("reason", "");
      std::optional<Decision> permission;
      std::optional<Json> updated;
      const bool keep_going = value.value("continue", true);
      if (!keep_going) {
        if (event != "SessionStart" && event != "UserPromptSubmit" && event != "Stop" &&
            event != "PostToolUse" && event != "PreCompact")
          throw Error("continue:false is unsupported for " + event);
        stop = true;
        reason = value.value("stopReason", event + " stopped execution");
      } else if (event == "PreToolUse") {
        const auto decision = specific.value("permissionDecision", "");
        if (decision == "deny") {
          block = true;
          reason = specific.value("permissionDecisionReason", "");
        } else if (decision == "allow") {
          if (specific.contains("updatedInput"))
            updated = specific["updatedInput"];
        } else if (!decision.empty() || specific.contains("updatedInput"))
          throw Error("Unsupported pre-tool permission decision");
        else if (value.value("decision", "") == "block")
          block = true;
        else if (value.contains("decision"))
          throw Error("Unsupported pre-tool decision");
      } else if (event == "PermissionRequest") {
        auto decision = specific.value("decision", Json::object());
        // Retain Codet's existing spelling as a compatibility alias.
        const auto behavior = decision.value("behavior", specific.value("permissionDecision", ""));
        if (behavior == "allow")
          permission = Decision::allow;
        else if (behavior == "deny") {
          block = true;
          permission = Decision::deny;
          reason = decision.value("message", specific.value("permissionDecisionReason", ""));
        } else if (!behavior.empty())
          throw Error("Unsupported approval decision");
      } else if (event == "UserPromptSubmit" || event == "PostToolUse" || event == "Stop") {
        const auto decision = value.value("decision", "");
        if (decision == "block")
          block = true;
        else if (!decision.empty())
          throw Error("Unsupported hook decision");
      }
      if (block && !nonempty(reason))
        throw Error("Blocking decision requires a nonempty reason");
      if (updated && !updated->is_object())
        throw Error("updatedInput must be an object");
      if (block || stop) {
        out.blocked = out.blocked || block;
        out.stopped = out.stopped || stop;
        append(out.reason, reason);
      }
      if (permission && (out.permission != Decision::deny))
        out.permission = permission;
      if (updated)
        out.updated_input = updated;
      if (!context.empty() && event != "PreCompact")
        append(out.additional_context, context);
    } catch (const std::exception &e) {
      diagnose(event + ": " + e.what());
    }
    out.additional_context = bounded_text(out.additional_context, config_.tool_output_limit);
    out.reason = bounded_text(out.reason, config_.tool_output_limit);
  }
  return out;
}
} // namespace codet
