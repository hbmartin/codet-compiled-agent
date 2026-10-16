#include "codet/core.hpp"
#include "codet/output.hpp"
#include <iostream>
#include <nlohmann/json-schema.hpp>
#include <set>
#include <sstream>

namespace codet {
RunResult Agent::run(Session &session, const std::string &prompt, const Cancellation &cancel,
                     const Json &schema) {
  auto owner = state_.acquire(session.id);
  session = state_.load_id(session.id, false);
  RunResult out;
  Json structured;
  session.provider = config_.provider;
  session.model = config_.model;
  tools_.use_session(session.id);
  state_.reconcile(session);
  std::unique_ptr<nlohmann::json_schema::json_validator> validator;
  if (!schema.is_null()) {
    if (!schema.is_object() || schema.value("type", "") != "object")
      throw Error("Output schema must describe an object", 2);
    const auto draft = schema.value("$schema", "http://json-schema.org/draft-07/schema#");
    if (draft != "http://json-schema.org/draft-07/schema#" &&
        draft != "https://json-schema.org/draft-07/schema")
      throw Error("Only JSON Schema Draft 7 is supported", 2);
    nlohmann::json_schema::schema_loader loader = [](const nlohmann::json_uri &, Json &) {
      throw Error("External schema references are disabled", 2);
    };
    validator = std::make_unique<nlohmann::json_schema::json_validator>(loader);
    try {
      validator->set_root_schema(schema);
    } catch (const std::exception &e) {
      throw Error(std::string("Invalid output schema: ") + e.what(), 2);
    }
  }
  const auto emit = [&](const Json &event) {
    if (events_)
      events_(safe_json(event));
  };
  emit({{"type", "thread.started"}, {"thread_id", session.id}});
  emit({{"type", "turn.started"}});
  state_.begin_turn(session.id);
  const auto recover = [&](const std::string &status) {
    try {
      state_.reconcile(session);
    } catch (const std::exception &e) {
      report_diagnostic(diagnostics_, "Transcript recovery failed: " + std::string(e.what()));
    }
    try {
      state_.finish_turn(session.id, status);
    } catch (const std::exception &e) {
      report_diagnostic(diagnostics_, "Turn status update failed: " + std::string(e.what()));
    }
  };
  try {
    const auto submitted = tools_.run_hooks("UserPromptSubmit",
                                            {{"hook_event_name", "UserPromptSubmit"},
                                             {"prompt", prompt},
                                             {"cwd", config_.cwd.string()},
                                             {"session_id", session.id}},
                                            cancel);
    if (submitted.blocked || submitted.stopped)
      throw Error("Prompt blocked by hook: " + submitted.reason);
    const auto expanded = safe_text(tools_.expand_command(prompt));
    Session candidate = session;
    if (!submitted.additional_context.empty()) {
      Message context;
      context.role = "system";
      context.text = "UserPromptSubmit hook context:\n" + submitted.additional_context;
      candidate.messages.push_back(context);
    }
    Message user;
    user.role = "user";
    user.text = expanded;
    candidate.messages.push_back(user);
    state_.save(candidate);
    session = std::move(candidate);
    const std::string system =
        "You are Codet, a single-agent terminal coding assistant. Complete the user's request "
        "using tools. Read existing files before editing. Respect permission denials; do not retry "
        "without a policy change. Shell commands run under application authorization, not an OS "
        "sandbox. Search file contents by running system grep through Bash. Choose appropriate "
        "grep flags. Prefer unquoted literal patterns and paths only when no quoting is needed, "
        "so scoped allow rules can match simple commands. Otherwise quote arguments correctly; "
        "never remove necessary quoting to evade permissions. Respect shell permissions. "
        "Use grep -I to skip binary files, -e for leading-hyphen patterns, -F for literals or -E "
        "for extended patterns. Use find with directory pruning to skip .git, build and "
        "node_modules; shell grep has no automatic exclusions. Command output is a bounded "
        "head/tail preview. Check spill_path, spill_bytes, total_output_bytes and spill_truncated "
        "metadata before assuming the capture is complete. Page retained text with Read "
        "offset/limit "
        "line ranges, or authorized Bash dd for byte ranges/long lines; files may expire. "
        "Glob finds file names; there is no dedicated Grep tool. Never claim tests ran unless "
        "you ran them. Use Memory for durable project facts. "
        "No subagents, goals, scheduling, MCP, or editor protocols are available. Working "
        "directory: " +
        config_.cwd.string() + tools_.instructions() +
        (validator ? "\nFinish by calling StructuredOutput with a value satisfying its schema."
                   : "");
    Json schemas = tools_.schemas();
    if (validator)
      schemas.push_back(
          {{"name", "StructuredOutput"},
           {"description",
            "Provide the final structured answer matching the requested JSON Schema."},
           {"input_schema", schema}});
    unsigned stop_blocks = 0;
    size_t requests = 0;
    while (requests < config_.max_turns) {
      cancel.check();
      // Compact only between completed requests/tool batches; no pending calls are lost.
      auto transcript = Json(session.messages).dump();
      if (transcript.size() + system.size() > config_.context_tokens * 3 &&
          session.messages.size() > 2 && requests + 1 < config_.max_turns) {
        const auto compact_hook = tools_.run_hooks(
            "PreCompact",
            {{"hook_event_name", "PreCompact"}, {"session_id", session.id}, {"trigger", "auto"}},
            cancel);
        if (compact_hook.stopped)
          throw Error(compact_hook.reason);
        std::string bounded = transcript;
        const auto limit = std::max(size_t{512}, config_.context_tokens * 2);
        if (bounded.size() > limit)
          bounded = render_output({transcript, "", transcript.size(), true}, limit).text;
        Message request;
        request.role = "user";
        request.text = "Summarize this completed conversation for continuation. Preserve the "
                       "current task, constraints, file changes, tests, permissions, failures, and "
                       "next steps. Do not execute tools.\n" +
                       bounded;
        ++requests;
        const auto summary =
            provider_.complete(config_, "Write a concise factual continuation summary.", {request},
                               Json::array(), cancel, {});
        out.usage.input += summary.usage.input;
        out.usage.output += summary.usage.output;
        out.usage.cached += summary.usage.cached;
        Message compact;
        compact.role = "system";
        compact.text = "Conversation summary:\n" + summary.message.text;
        session.messages = {compact, user};
        state_.save(session);
        emit({{"type", "codet.compacted"}, {"messages_before", Json::parse(transcript).size()}});
      }
      const auto item_id = unique_id();
      emit({{"type", "item.started"},
            {"item", {{"id", item_id}, {"type", "agent_message"}, {"text", ""}}}});
      ++requests;
      auto response = provider_.complete(
          config_, system, session.messages, schemas, cancel, [&](const std::string &delta) {
            emit({{"type", "item.updated"},
                  {"item", {{"id", item_id}, {"type", "agent_message"}, {"text_delta", delta}}}});
          });
      out.usage.input += response.usage.input;
      out.usage.output += response.usage.output;
      out.usage.cached += response.usage.cached;
      out.usage.reasoning += response.usage.reasoning;
      std::set<std::string> ids;
      for (const auto &message : session.messages)
        for (const auto &call : message.calls)
          ids.insert(call.id);
      for (const auto &call : response.message.calls)
        if (call.id.empty() || !ids.insert(call.id).second)
          throw Error("Provider returned duplicate or empty tool call ids");
      session.messages.push_back(response.message);
      state_.save(session);
      emit({{"type", "item.completed"},
            {"item",
             {{"id", item_id}, {"type", "agent_message"}, {"text", response.message.text}}}});
      if (response.message.calls.empty()) {
        if (validator && structured.is_null())
          throw Error("Model completed without a valid StructuredOutput result");
        out.text = response.message.text;
        out.structured = structured;
        try {
          const auto stop = tools_.run_hooks("Stop",
                                             {{"hook_event_name", "Stop"},
                                              {"session_id", session.id},
                                              {"last_assistant_message", out.text},
                                              {"stop_hook_active", stop_blocks > 0}},
                                             cancel);
          if (stop.stopped)
            report_diagnostic(diagnostics_, "Stop hook stopped execution: " + stop.reason);
          else if (stop.blocked)
            throw Error(stop.reason);
        } catch (const Error &e) {
          if (e.code == 130 || ++stop_blocks > 3)
            throw;
          Message blocked;
          blocked.role = "user";
          blocked.text = "Completion hook blocked the stop: " + std::string(e.what()) +
                         ". Address its requirements without repeating completed side effects.";
          session.messages.push_back(blocked);
          state_.save(session);
          continue;
        }
        state_.finish_turn(session.id, "complete");
        emit({{"type", "turn.completed"},
              {"usage",
               {{"input_tokens", out.usage.input},
                {"cached_input_tokens", out.usage.cached},
                {"output_tokens", out.usage.output},
                {"reasoning_output_tokens", out.usage.reasoning}}}});
        return out;
      }
      for (const auto &call : response.message.calls) {
        cancel.check();
        state_.tool_started(session.id, call);
        const auto type = call.name == "Bash" || call.name == "TaskCreate" ? "command_execution"
                          : call.name == "Write" || call.name == "Edit" ||
                                  call.name == "ApplyPatch" || call.name == "BatchEdit"
                              ? "file_change"
                          : call.name == "WebSearch" ? "web_search"
                                                     : "codet.tool";
        Json item = {
            {"id", call.id}, {"type", type}, {"tool", call.name}, {"status", "in_progress"}};
        if (call.name == "Bash" || call.name == "TaskCreate")
          item["command"] = call.arguments.value("command", "");
        emit({{"type", "item.started"}, {"item", item}});
        ToolResult result;
        if (call.name == "StructuredOutput" && validator) {
          try {
            validator->validate(call.arguments);
            structured = call.arguments;
            result.content = "Structured output accepted";
          } catch (const std::exception &e) {
            result = {std::string("Schema validation failed: ") + e.what(), true, Json::object()};
          }
        } else
          result = tools_.execute(call, cancel);
        Message tool;
        tool.role = "tool";
        tool.tool_call_id = call.id;
        tool.text = Json({{"content", result.content},
                          {"is_error", result.error},
                          {"metadata", result.metadata}})
                        .dump(-1, ' ', false, Json::error_handler_t::replace);
        state_.tool_finished(session.id, call, Json(tool));
        session.messages.push_back(tool);
        state_.save(session);
        item["status"] = result.error ? "failed" : "completed";
        item["output"] = result.content;
        item["metadata"] = result.metadata;
        emit({{"type", "item.completed"}, {"item", item}});
        if (result.metadata.contains("hook_stop"))
          throw Error("Stopped by PostToolUse hook: " +
                      result.metadata.at("hook_stop").get<std::string>());
      }
    }
    throw Error("Maximum model turns reached");
  } catch (const Error &e) {
    recover(e.code == 130 ? "interrupted" : "failed");
    emit({{"type", "turn.failed"}, {"error", {{"message", e.what()}}}});
    throw;
  } catch (const std::exception &e) {
    recover("failed");
    emit({{"type", "turn.failed"}, {"error", {{"message", e.what()}}}});
    throw Error(safe_text(e.what()));
  }
}
} // namespace codet
