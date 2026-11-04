#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/process_control.hpp"
#include <fnmatch.h>
#include <iostream>

namespace codet {
HookOutcome Tools::run_hooks(const std::string &event, const Json &input,
                             const Cancellation &cancel, int budget_ms,
                             DiagnosticBuffer *external) {
  const auto began = std::chrono::steady_clock::now();
  HookOutcome out;
  DiagnosticBuffer local(config_.tool_output_limit);
  auto &collector = external ? *external : local;
  struct Notice {
    DiagnosticBuffer &buffer;
    const DiagnosticSink &sink;
    bool active;
    Notice(DiagnosticBuffer &b, const DiagnosticSink &s, bool enabled)
        : buffer(b), sink(s), active(enabled) {}
    ~Notice() {
      if (active && buffer.omitted()) {
        try {
          report_diagnostic(sink, DiagnosticBuffer::notice());
        } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original hook result during
                        // diagnostic cleanup.
        }
      }
    }
  } notice{collector, diagnostics_, !external};
  const auto append = [&](std::string &target, const std::string &text) {
    target = bounded_text(target + (target.empty() ? "" : "\n") + text, config_.tool_output_limit);
  };
  const auto diagnose = [&](const std::string &text,
                            DiagnosticLevel level = DiagnosticLevel::warning) {
    if (auto accepted = collector.add(hook_diagnostic(event, level, text))) {
      out.diagnostics.push_back(*accepted);
      if (!external)
        detail::report_rendered_diagnostic(diagnostics_, {*accepted});
    }
  };
  if (!config_.hooks.contains(event))
    return out;
  const auto &hooks = config_.hooks[event];
  if (!hooks.is_array())
    throw Error("Hook event must be an array", 2);
  for (const auto &hook : hooks) {
    const bool control = event != "SessionEnd" && event != "PostToolUseFailure";
    const auto on_error = hook.value("on_error", "stop");
    if (on_error != "stop" && on_error != "warn")
      throw Error("Hook on_error must be stop or warn", 2);
    const auto fail = [&](const std::string &reason) {
      auto message = event;
      message += ": ";
      message += reason;
      diagnose(message, DiagnosticLevel::error);
      if (control && on_error == "stop")
        throw HookFailure(
            std::string("Control hook ").append(event).append(" failed: ").append(reason));
    };
    cancel.check();
    if (budget_ms > 0 &&
        std::chrono::steady_clock::now() - began >= std::chrono::milliseconds(budget_ms)) {
      diagnose("Hook cleanup budget exhausted", DiagnosticLevel::error);
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
      const bool operation_alias = event == "PermissionRequest" &&
                                   (matcher == "exec_command" || matcher == "write_stdin") &&
                                   matcher == input.value("tool", "");
      if (!operation_alias && fnmatch(matcher.c_str(), subject.c_str(), 0))
        continue;
      ProcessSpec spec;
      spec.cwd = config_.cwd;
      spec.separate_stderr = true;
      spec.output_limit = config_.tool_output_limit;
      spec.timeout_ms = hook.value("timeout_ms", 10000);
      if (spec.timeout_ms < 1 || spec.timeout_ms > 120000)
        throw Error("Hook timeout must be 1..120000 ms");
      fs::create_directories(invocation_directory(config_) / "hook-input");
      file = invocation_directory(config_) / "hook-input" / (unique_id() + ".json");
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
      if (fatal(std::current_exception()))
        throw;
      fail(error.what());
      continue;
    }
    const bool reads_stdout = event != "SessionEnd" && event != "PostToolUseFailure";
    if (result.stdout_truncated && reads_stdout)
      diagnose(event + " stdout capture truncated");
    if (result.stderr_truncated)
      diagnose(event + " stderr capture truncated");
    const auto stdout_text = safe_text(result.output),
               stderr_text = safe_text(result.stderr_output);
    const auto nonempty = [](const std::string &s) {
      return s.find_first_not_of(" \t\r\n") != std::string::npos;
    };
    const bool can_block = control;
    if (result.exit_code != 0) {
      if (result.exit_code == 2 && can_block) {
        out.blocked = true;
        if (event == "SessionStart" || event == "PreCompact")
          out.stopped = true;
        append(out.reason, nonempty(stderr_text) ? stderr_text : event + " vetoed execution");
        if (event == "PermissionRequest")
          out.permission = Decision::deny;
      } else
        fail("exited with code " + std::to_string(result.exit_code) +
             (stderr_text.empty() ? "" : ": " + stderr_text));
      continue;
    }
    if (nonempty(stderr_text)) {
      auto diagnostic = event;
      diagnostic += " stderr: ";
      diagnostic += stderr_text;
      diagnose(diagnostic);
    }
    if (!reads_stdout || !nonempty(stdout_text))
      continue;
    const auto first = stdout_text.find_first_not_of(" \t\r\n");
    const bool json_like = stdout_text[first] == '{' || stdout_text[first] == '[';
    if (json_like && result.stdout_truncated) {
      fail("returned truncated JSON output");
      continue;
    }
    if (result.stdout_truncated && (event == "SessionStart" || event == "UserPromptSubmit")) {
      fail("returned truncated context output");
      continue;
    }
    const auto value = Json::parse(stdout_text, nullptr, false);
    if (!value.is_object()) {
      if (json_like || event == "Stop")
        fail("returned invalid JSON output");
      else if (event == "SessionStart" || event == "UserPromptSubmit")
        append(out.additional_context, stdout_text);
      continue;
    }
    // Recognizable vetoes remain binding even when unrelated optional fields are malformed.
    if (can_block) {
      const auto specific = value.value("hookSpecificOutput", Json());
      const auto decision = specific.is_object() ? specific.value("decision", Json()) : Json();
      const bool stop = value.value("continue", Json()) == false &&
                        (event == "SessionStart" || event == "UserPromptSubmit" ||
                         event == "Stop" || event == "PostToolUse" || event == "PreCompact");
      const bool veto =
          stop ||
          (value.value("decision", Json()) == "block" &&
           (event == "PreToolUse" || event == "UserPromptSubmit" || event == "PostToolUse" ||
            event == "Stop")) ||
          (specific.is_object() && (event == "PreToolUse" || event == "PermissionRequest") &&
           specific.value("permissionDecision", Json()) == "deny") ||
          (event == "PermissionRequest" && decision.is_object() &&
           decision.value("behavior", Json()) == "deny");
      if (veto) {
        out.blocked = true;
        out.stopped = out.stopped || stop;
        if (event == "PermissionRequest")
          out.permission = Decision::deny;
        append(out.reason, event + " vetoed execution");
      }
    }
    try {
      const auto specific = value.value("hookSpecificOutput", Json::object());
      if (!specific.is_object())
        throw Error("hookSpecificOutput must be an object");
      const auto context = specific.value("additionalContext", "");
      if (value.contains("systemMessage"))
        diagnose(event + ": " + value.at("systemMessage").get<std::string>(),
                 DiagnosticLevel::info);
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
        if (behavior == "allow") {
          const auto operation = input.value("tool", "");
          const auto scope = decision.value("tool", "");
          if ((!scope.empty() && scope != operation) ||
              (operation == "write_stdin" && scope != "write_stdin"))
            diagnose("PermissionRequest allow ignored: stdin approval requires "
                     "hookSpecificOutput.decision.tool = write_stdin; decision scope must "
                     "match the requested tool");
          else
            permission = Decision::allow;
        } else if (behavior == "deny") {
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
        reason = event + " vetoed execution";
      if (updated && !updated->is_object())
        throw Error("updatedInput must be an object");
      if (block || stop) {
        out.blocked = out.blocked || block;
        out.stopped = out.stopped || stop;
        if (block && out.reason == event + " vetoed execution")
          out.reason.clear();
        append(out.reason, reason);
      }
      if (permission && (out.permission != Decision::deny))
        out.permission = permission;
      if (updated)
        out.updated_input = updated;
      if (!context.empty() && event != "PreCompact")
        append(out.additional_context, context);
    } catch (const std::exception &e) {
      if (fatal(std::current_exception()))
        throw;
      fail(e.what());
    }
    out.additional_context = bounded_text(out.additional_context, config_.tool_output_limit);
    out.reason = bounded_text(out.reason, config_.tool_output_limit);
  }
  if (!external && collector.omitted())
    out.diagnostics.push_back(DiagnosticBuffer::notice());
  return out;
}
} // namespace codet
