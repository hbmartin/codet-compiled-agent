#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/tool_registry.hpp"
#include <algorithm>
#include <sstream>

namespace codet {
void SseDecoder::line(std::string l) {
  if (!l.empty() && l.back() == '\r')
    l.pop_back();
  if (l.empty()) {
    if (!data_.empty()) {
      if (data_.back() == '\n')
        data_.pop_back();
      sink_(event_, data_);
    }
    event_.clear();
    data_.clear();
    return;
  }
  if (l[0] == ':')
    return;
  const auto pos = l.find(':');
  const std::string key = l.substr(0, pos);
  std::string value = pos == std::string::npos ? "" : l.substr(pos + 1);
  if (!value.empty() && value[0] == ' ')
    value.erase(0, 1);
  if (key == "event")
    event_ = value;
  else if (key == "data") {
    data_ += value;
    data_ += '\n';
  }
}
void SseDecoder::feed(const std::string &bytes) {
  pending_ += bytes;
  if (pending_.size() > size_t{16} * size_t{1024} * 1024)
    throw Error("SSE frame exceeds limit");
  size_t pos;
  while ((pos = pending_.find('\n')) != std::string::npos) {
    line(pending_.substr(0, pos));
    pending_.erase(0, pos + 1);
  }
}
void SseDecoder::finish() {
  if (!pending_.empty()) {
    line(pending_);
    pending_.clear();
  }
  line("");
}
namespace {
std::string join_url(std::string base, const std::string &suffix) {
  while (!base.empty() && base.back() == '/')
    base.pop_back();
  return base + suffix;
}
std::string tool_text(const Message &m) {
  std::string text = m.text;
  if (m.role == "tool") {
    const auto value = Json::parse(m.text, nullptr, false);
    if (value.is_object() && value.contains("content") && value.contains("is_error"))
      text = (value.value("is_error", false) ? std::string("Tool error: ") : std::string{}) +
             value.at("content").get<std::string>();
  }
  if (!m.replay.empty() && m.replay.back().contains("codet_image_notes"))
    for (const auto &note : m.replay.back().at("codet_image_notes"))
      text += "\n" + note.get<std::string>();
  return text;
}
std::vector<Message> prepare_images(const Config &config, const std::vector<Message> &input) {
  auto messages = input;
  size_t count = 0, bytes = 0;
  for (auto m = messages.rbegin(); m != messages.rend(); ++m) {
    Json prepared = Json::array(), notes = Json::array();
    for (auto a = m->images.rbegin(); a != m->images.rend(); ++a) {
      try {
        if (!image_input_enabled(config) || count >= 4)
          throw Error("Image omitted from request budget; use view_image to reload");
        const auto data = image_data_url(*a, config);
        const auto n = (data.size() - data.find(',') - 1) / 4 * 3;
        if (n > image_file_limit - bytes)
          throw Error("Image omitted from request byte budget; use view_image to reload");
        bytes += n;
        ++count;
        prepared.insert(prepared.begin(), Json({{"artifact", *a}, {"url", data}}));
      } catch (const std::exception &e) {
        notes.push_back("[Image artifact " + a->path + ": " + safe_text(e.what()) + "]");
      }
    }
    // Transient transport preparation is separate from stored provider reasoning replay.
    m->replay.push_back({{"codet_prepared_images", prepared}, {"codet_image_notes", notes}});
  }
  return messages;
}
const Json &prepared_images(const Message &m) {
  static const Json empty = Json::array();
  if (!m.replay.empty() && m.replay.back().contains("codet_prepared_images"))
    return m.replay.back().at("codet_prepared_images");
  return empty;
}
Json clean_replay(const Message &m) {
  auto replay = m.replay;
  if (!replay.empty() && replay.back().contains("codet_prepared_images"))
    replay.erase(replay.end() - 1);
  return replay;
}
Json chat_messages(const std::string &system, const std::vector<Message> &messages, bool ollama) {
  Json out = Json::array({{{"role", "system"}, {"content", system}}});
  Json pending = Json::array();
  auto flush = [&] {
    if (pending.empty())
      return;
    Json item = {{"role", "user"}, {"content", "Images from preceding tool results"}};
    if (ollama) {
      item["images"] = Json::array();
      for (const auto &image : pending) {
        const std::string url = image.at("url");
        item["images"].push_back(url.substr(url.find(',') + 1));
      }
    } else {
      item["content"] =
          Json::array({{{"type", "text"}, {"text", "Images from preceding tool results"}}});
      for (const auto &image : pending)
        item["content"].push_back(
            {{"type", "image_url"},
             {"image_url",
              {{"url", image.at("url")},
               {"detail",
                image.at("artifact").value("detail", "high") == "original" ? "high" : "auto"}}}});
    }
    out.push_back(item);
    pending = Json::array();
  };
  std::map<std::string, std::string> names;
  for (const auto &m : messages) {
    if (m.role != "tool")
      flush();
    Json j = {{"role", m.role}, {"content", tool_text(m)}};
    for (const auto &image : prepared_images(m))
      pending.push_back(image);
    if (m.role == "tool") {
      if (ollama)
        j["tool_name"] = names.at(m.tool_call_id);
      else
        j["tool_call_id"] = m.tool_call_id;
    }
    if (!m.calls.empty()) {
      j["tool_calls"] = Json::array();
      for (const auto &t : m.calls) {
        names[t.id] = tool_wire_name(t.name);
        Json call = {{"type", "function"},
                     {"function",
                      {{"name", tool_wire_name(t.name)},
                       {"arguments", ollama ? t.arguments : Json(t.arguments.dump())}}}};
        if (!ollama)
          call["id"] = t.id;
        j["tool_calls"].push_back(std::move(call));
      }
    }
    out.push_back(j);
  }
  flush();
  return out;
}
Json functions(const Json &tools) {
  Json out = Json::array();
  for (const auto &t : tools)
    out.push_back({{"type", "function"},
                   {"function",
                    {{"name", tool_wire_name(t["name"].get<std::string>())},
                     {"description", t["description"]},
                     {"parameters", t["input_schema"]}}}});
  return out;
}
Json parse_object(const std::string &text) {
  auto j = Json::parse(text, nullptr, false);
  if (!j.is_object())
    throw Error("Provider returned malformed JSON");
  return j;
}
void usage(Usage &u, const Json &j) {
  u.input = j.value("input_tokens", j.value("prompt_tokens", u.input));
  u.output = j.value("output_tokens", j.value("completion_tokens", u.output));
  if (j.contains("input_tokens_details"))
    u.cached = j["input_tokens_details"].value("cached_tokens", int64_t{0});
  if (j.contains("output_tokens_details"))
    u.reasoning = j["output_tokens_details"].value("reasoning_tokens", int64_t{0});
  if (j.contains("prompt_tokens_details"))
    u.cached = j["prompt_tokens_details"].value("cached_tokens", int64_t{0});
  if (j.contains("cache_read_input_tokens"))
    u.cached = j["cache_read_input_tokens"];
}
void append_calls(Message &m, const Json &calls) {
  for (const auto &t : calls) {
    const auto &f = t.at("function");
    ToolCall call;
    call.id = t.value("id", unique_id());
    call.name = tool_logical_name(f.at("name"));
    call.arguments = f["arguments"].is_string() ? parse_object(f["arguments"].get<std::string>())
                                                : f["arguments"];
    m.calls.push_back(std::move(call));
  }
}
void codex_output(ProviderResult &result, const Json &response) {
  result.message.text.clear();
  result.message.calls.clear();
  result.message.replay = Json::array();
  for (const auto &item : response.value("output", Json::array())) {
    const auto type = item.value("type", "");
    if (type == "message") {
      for (const auto &c : item.value("content", Json::array()))
        if (c.value("type", "") == "output_text")
          result.message.text += c.value("text", "");
    } else if (type == "function_call")
      result.message.calls.push_back(
          {item.at("call_id"),
           tool_logical_name(item.value("namespace", std::string{}).empty()
                                 ? item.at("name").get<std::string>()
                                 : item.at("namespace").get<std::string>() + "." +
                                       item.at("name").get<std::string>()),
           parse_object(item.value("arguments", "{}"))});
    else if (type == "custom_tool_call")
      result.message.calls.push_back(
          {item.at("call_id"), item.value("name", ""), {{"patch", item.value("input", "")}}});
    if (type != "reasoning" || !item.value("encrypted_content", "").empty())
      result.message.replay.push_back(item);
  }
  if (response.contains("usage"))
    usage(result.usage, response["usage"]);
  result.stop_reason = response.value("status", "completed");
}
} // namespace
HttpRequest ApiProvider::request(const Config &c, const std::string &system,
                                 const std::vector<Message> &original_messages, const Json &tools,
                                 const Json &auth) const {
  const auto messages = prepare_images(c, original_messages);
  HttpRequest r;
  r.timeout_seconds = c.request_timeout;
  r.connect_timeout_seconds = c.connect_timeout;
  r.idle_timeout_seconds = c.stream_idle_timeout;
  r.headers = {{"Content-Type", "application/json"}, {"Accept", "text/event-stream"}};
  const auto it = c.providers.find(c.provider);
  ProviderConfig p = it == c.providers.end() ? ProviderConfig{} : it->second;
  std::string key = p.api_key;
  if (key.empty())
    key = env(c.provider == "anthropic" ? "ANTHROPIC_API_KEY"
              : c.provider == "google"  ? "GOOGLE_API_KEY"
              : c.provider == "ollama"  ? "OLLAMA_API_KEY"
                                        : "OPENAI_API_KEY");
  if (key.empty() && c.provider == "google")
    key = env("GOOGLE_GENERATIVE_AI_API_KEY");
  if (key.empty())
    key = credentials_.key(c.provider);
  Json body;
  if (c.provider == "codex") {
    r.url = p.base_url.empty() ? "https://chatgpt.com/backend-api/codex/responses" : p.base_url;
    if (!auth.contains("access_token"))
      throw Error("Missing imported ChatGPT credentials");
    r.headers["Authorization"] = "Bearer " + auth["access_token"].get<std::string>();
    r.headers["originator"] = "codex_cli_rs";
    if (auth.contains("account_id"))
      r.headers["ChatGPT-Account-Id"] = auth["account_id"].get<std::string>();
    Json input = Json::array({{{"role", "developer"}, {"content", system}}});
    for (const auto &m : messages) {
      if (m.role == "assistant" && !clean_replay(m).empty() &&
          (m.replay_provider.empty() || m.replay_provider == c.provider)) {
        for (const auto &item : clean_replay(m))
          input.push_back(item);
        continue;
      }
      if (m.role == "tool") {
        // Custom apply_patch results must retain their wire type during replay.
        bool custom = false;
        for (const auto &old : messages)
          for (const auto &item : old.replay)
            if (item.value("type", "") == "custom_tool_call" &&
                item.value("call_id", "") == m.tool_call_id)
              custom = true;
        input.push_back({{"type", custom ? "custom_tool_call_output" : "function_call_output"},
                         {"call_id", m.tool_call_id},
                         {"output", tool_text(m)}});
        if (!prepared_images(m).empty()) {
          auto &output = input.back()["output"];
          output = Json::array({{{"type", "input_text"}, {"text", tool_text(m)}}});
          for (const auto &image : prepared_images(m))
            output.push_back(
                {{"type", "input_image"}, {"image_url", image.at("url")}, {"detail", "high"}});
        }
      } else {
        input.push_back(
            {{"role", m.role == "system" ? "developer" : m.role}, {"content", tool_text(m)}});
        if (!prepared_images(m).empty()) {
          Json content = Json::array();
          for (const auto &image : prepared_images(m))
            content.push_back(
                {{"type", "input_image"}, {"image_url", image.at("url")}, {"detail", "high"}});
          input.push_back({{"role", "user"}, {"content", content}});
        }
        for (const auto &t : m.calls)
          input.push_back({{"type", "function_call"},
                           {"call_id", t.id},
                           {"name", tool_wire_name(t.name)},
                           {"arguments", t.arguments.dump()}});
        for (auto &item : input)
          if (item.value("type", "") == "function_call" && item.contains("name")) {
            const auto qualified = item.at("name").get<std::string>();
            const auto p = qualified.find("__");
            if (p != std::string::npos) {
              item["namespace"] = qualified.substr(0, p);
              item["name"] = qualified.substr(p + 2);
            }
          }
      }
    }
    Json ft = Json::array();
    for (const auto &t : tools) {
      if (t["name"] == "apply_patch") {
        ft.push_back(
            {{"type", "custom"},
             {"name", "apply_patch"},
             {"description",
              "Apply a patch using the *** Begin Patch envelope. Supply raw patch text, not JSON."},
             {"format",
              {{"type", "grammar"},
               {"syntax", "lark"},
               {"definition", R"(start: begin_patch hunk+ end_patch
begin_patch: "*** Begin Patch" LF
end_patch: "*** End Patch" LF?
hunk: add_hunk | delete_hunk | update_hunk
add_hunk: "*** Add File: " filename LF add_line+
delete_hunk: "*** Delete File: " filename LF
update_hunk: "*** Update File: " filename LF change_move? change?
filename: /(.+)/
add_line: "+" /(.*)/ LF -> line
change_move: "*** Move to: " filename LF
change: (change_context | change_line)+ eof_line?
change_context: ("@@" | "@@ " /(.+)/) LF
change_line: ("+" | "-" | " ") /(.*)/ LF
eof_line: "*** End of File" LF
%import common.LF
)"}}}});
      } else
        ft.push_back({{"type", "function"},
                      {"name", tool_wire_name(t["name"].get<std::string>())},
                      {"description", t["description"]},
                      {"parameters", t["input_schema"]},
                      {"strict", false}});
    }
    Json namespaced = Json::array();
    std::map<std::string, Json> groups;
    for (auto tool : ft) {
      const auto name = tool.at("name").get<std::string>();
      const auto split = name.find("__");
      if (split == std::string::npos)
        namespaced.push_back(tool);
      else {
        const auto ns = name.substr(0, split);
        tool["name"] = name.substr(split + 2);
        if (!groups.count(ns))
          groups[ns] = {{"type", "namespace"},
                        {"name", ns},
                        {"description", "Tools for " + ns},
                        {"tools", Json::array()}};
        groups[ns]["tools"].push_back(tool);
      }
    }
    ft = namespaced;
    for (const auto &group : groups)
      ft.push_back(group.second);
    body = {{"model", c.model},
            {"input", input},
            {"store", false},
            {"stream", true},
            {"parallel_tool_calls", false},
            {"include", Json::array({"reasoning.encrypted_content"})}};
    if (!ft.empty())
      body["tools"] = ft;
    if (!c.effort.empty())
      body["reasoning"] = {{"effort", c.effort}, {"summary", "auto"}};
  } else if (c.provider == "anthropic") {
    if (key.empty())
      throw Error("Missing Anthropic API key");
    r.url = join_url(p.base_url.empty() ? "https://api.anthropic.com" : p.base_url, "/v1/messages");
    r.headers["x-api-key"] = key;
    r.headers["anthropic-version"] = "2023-06-01";
    Json ms = Json::array();
    for (const auto &m : messages) {
      Json content = Json::array();
      std::string role = m.role == "tool" ? "user" : m.role;
      if (role == "system") {
        content.push_back({{"type", "text"}, {"text", tool_text(m)}});
        role = "user";
      } else if (m.role == "tool")
        content.push_back(
            {{"type", "tool_result"}, {"tool_use_id", m.tool_call_id}, {"content", tool_text(m)}});
      else {
        if (m.role == "assistant" && !clean_replay(m).empty() &&
            (m.replay_provider.empty() || m.replay_provider == c.provider))
          content = clean_replay(m);
        else {
          if (!tool_text(m).empty())
            content.push_back({{"type", "text"}, {"text", tool_text(m)}});
          for (const auto &t : m.calls)
            content.push_back({{"type", "tool_use"},
                               {"id", t.id},
                               {"name", tool_wire_name(t.name)},
                               {"input", t.arguments}});
        }
      }
      if (!prepared_images(m).empty()) {
        Json blocks = Json::array();
        if (m.role == "tool")
          blocks.push_back({{"type", "text"}, {"text", tool_text(m)}});
        for (const auto &image : prepared_images(m)) {
          const std::string url = image.at("url");
          blocks.push_back({{"type", "image"},
                            {"source",
                             {{"type", "base64"},
                              {"media_type", image.at("artifact").at("mime")},
                              {"data", url.substr(url.find(',') + 1)}}}});
        }
        if (m.role == "tool")
          content[0]["content"] = blocks;
        else
          for (const auto &block : blocks)
            content.push_back(block);
      }
      if (content.empty())
        continue;
      if (!ms.empty() && ms.back()["role"] == role)
        for (const auto &item : content)
          ms.back()["content"].push_back(item);
      else
        ms.push_back({{"role", role}, {"content", content}});
    }
    body = {{"model", c.model},
            {"system", Json::array({{{"type", "text"},
                                     {"text", system},
                                     {"cache_control", {{"type", "ephemeral"}}}}})},
            {"messages", ms},
            {"max_tokens", c.max_output_tokens},
            {"stream", true}};
    if (!tools.empty()) {
      auto declarations = tools;
      for (auto &tool : declarations)
        tool["name"] = tool_wire_name(tool.at("name"));
      body["tools"] = std::move(declarations);
    }
    if (!c.effort.empty()) {
      body["thinking"] = {{"type", "adaptive"}};
      body["output_config"] = {{"effort", c.effort}};
    }
  } else if (c.provider == "google") {
    if (key.empty())
      throw Error("Missing Google API key");
    r.url = join_url(p.base_url.empty() ? "https://generativelanguage.googleapis.com" : p.base_url,
                     "/v1beta/models/" + url_encode(c.model) + ":streamGenerateContent?alt=sse");
    r.headers["x-goog-api-key"] = key;
    Json contents = Json::array();
    std::map<std::string, std::string> names;
    for (const auto &m : messages) {
      Json parts = Json::array();
      for (const auto &t : m.calls)
        names[t.id] = tool_wire_name(t.name);
      if (m.role == "assistant" && !clean_replay(m).empty() &&
          (m.replay_provider.empty() || m.replay_provider == c.provider))
        parts = clean_replay(m);
      else if (m.role == "tool")
        parts.push_back(
            {{"functionResponse",
              {{"name", names[m.tool_call_id]}, {"response", {{"result", tool_text(m)}}}}}});
      else {
        if (!tool_text(m).empty())
          parts.push_back({{"text", tool_text(m)}});
        for (const auto &t : m.calls)
          parts.push_back(
              {{"functionCall", {{"name", tool_wire_name(t.name)}, {"args", t.arguments}}}});
      }
      for (const auto &image : prepared_images(m)) {
        const std::string url = image.at("url");
        parts.push_back({{"inlineData",
                          {{"mimeType", image.at("artifact").at("mime")},
                           {"data", url.substr(url.find(',') + 1)}}}});
      }
      if (!parts.empty()) {
        const std::string role = m.role == "assistant" ? "model" : "user";
        if (!contents.empty() && contents.back()["role"] == role)
          for (const auto &part : parts)
            contents.back()["parts"].push_back(part);
        else
          contents.push_back({{"role", role}, {"parts", parts}});
      }
    }
    Json declarations = Json::array();
    for (const auto &t : tools) {
      Json declaration = {{"name", tool_wire_name(t["name"].get<std::string>())},
                          {"description", t["description"]}};
      const auto &schema = t["input_schema"];
      const bool no_arguments =
          t["name"] != "StructuredOutput" && schema.value("type", "") == "object" &&
          schema.contains("properties") && schema["properties"].empty() &&
          schema.value("required", Json::array()).empty() && !schema.contains("patternProperties");
      if (!no_arguments)
        declaration["parametersJsonSchema"] = schema;
      declarations.push_back(declaration);
    }
    body = {{"systemInstruction", {{"parts", Json::array({{{"text", system}}})}}},
            {"contents", contents},
            {"generationConfig", {{"maxOutputTokens", c.max_output_tokens}}}};
    if (!declarations.empty())
      body["tools"] = Json::array({{{"functionDeclarations", declarations}}});
    if (!c.effort.empty())
      throw Error("Google reasoning effort override is not supported by this compatibility subset; "
                  "remove model_reasoning_effort");
  } else {
    const bool ollama = c.provider == "ollama";
    if (!ollama && key.empty())
      throw Error("Missing OpenAI API key");
    r.url = join_url(p.base_url.empty()
                         ? (ollama ? "http://localhost:11434" : "https://api.openai.com/v1")
                         : p.base_url,
                     ollama ? "/api/chat" : "/chat/completions");
    if (!key.empty())
      r.headers["Authorization"] = "Bearer " + key;
    body = {{"model", c.model},
            {"messages", chat_messages(system, messages, ollama)},
            {"stream", true}};
    if (!tools.empty())
      body["tools"] = functions(tools);
    if (ollama) {
      body["options"] = {{"num_predict", c.max_output_tokens}};
      r.headers["Accept"] = "application/x-ndjson";
    } else {
      body["max_completion_tokens"] = c.max_output_tokens;
      body["stream_options"] = {{"include_usage", true}};
      if (!c.effort.empty())
        body["reasoning_effort"] = c.effort;
    }
  }
  r.body = body.dump();
  return r;
}
ProviderResult ApiProvider::complete(const Config &c, const std::string &system,
                                     const std::vector<Message> &messages, const Json &tools,
                                     const Cancellation &cancel, const ChunkSink &sink) {
  const Json auth = c.provider == "codex" ? credentials_.codex(http_, cancel) : Json::object();
  const auto req = request(c, system, messages, tools, auth);
  for (int attempt = 0; attempt < 4; ++attempt) {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.replay_provider = c.provider;
    bool completed = false, saw = false;
    std::map<size_t, Json> blocks;
    std::map<size_t, std::string> arguments;
    std::map<size_t, ToolCall> calls;
    auto text = [&](const std::string &value) {
      result.message.text += value;
      if (sink)
        sink(value);
    };
    auto frame = [&](const std::string &event, const std::string &data) {
      if (data == "[DONE]") {
        completed = true;
        return;
      }
      const auto j = parse_object(data);
      saw = true;
      const auto type = j.value("type", event);
      if (j.contains("error") || type == "error" || type == "response.failed")
        throw Error("Provider stream reported an error");
      if (c.provider == "codex") {
        if (type == "response.output_text.delta")
          text(j.value("delta", ""));
        if (type == "response.completed") {
          codex_output(result, j.at("response"));
          completed = true;
        }
        if (type == "response.incomplete")
          throw Error("Provider response incomplete (output limit or interruption)");
      } else if (c.provider == "anthropic") {
        const size_t index = j.value("index", size_t{0});
        if (type == "message_start" && j.contains("message"))
          usage(result.usage, j["message"].value("usage", Json::object()));
        if (type == "content_block_start")
          blocks[index] = j.at("content_block");
        if (type == "content_block_delta") {
          const auto &delta = j.at("delta");
          const auto dt = delta.value("type", "");
          if (dt == "text_delta") {
            const auto value = delta.value("text", "");
            text(value);
            blocks[index]["text"] = blocks[index].value("text", "") + value;
          }
          if (dt == "input_json_delta")
            arguments[index] += delta.value("partial_json", "");
          if (dt == "thinking_delta")
            blocks[index]["thinking"] =
                blocks[index].value("thinking", "") + delta.value("thinking", "");
          if (dt == "signature_delta")
            blocks[index]["signature"] =
                blocks[index].value("signature", "") + delta.value("signature", "");
        }
        if (type == "message_delta") {
          usage(result.usage, j.value("usage", Json::object()));
          result.stop_reason = j.value("delta", Json::object()).value("stop_reason", "");
        }
        if (type == "message_stop")
          completed = true;
      } else if (c.provider == "google") {
        for (const auto &candidate : j.value("candidates", Json::array())) {
          for (const auto &part :
               candidate.value("content", Json::object()).value("parts", Json::array())) {
            result.message.replay.push_back(part);
            if (part.contains("text") && !part.value("thought", false))
              text(part["text"]);
            if (part.contains("functionCall")) {
              auto f = part["functionCall"];
              result.message.calls.push_back(
                  {unique_id(), tool_logical_name(f.at("name")), f.value("args", Json::object())});
            }
          }
          if (candidate.contains("finishReason")) {
            result.stop_reason = candidate["finishReason"];
            completed = true;
          }
        }
        if (j.contains("usageMetadata")) {
          const auto &u = j["usageMetadata"];
          result.usage.input = u.value("promptTokenCount", 0);
          result.usage.output = u.value("candidatesTokenCount", 0);
          result.usage.cached = u.value("cachedContentTokenCount", 0);
          result.usage.reasoning = u.value("thoughtsTokenCount", 0);
        }
      } else if (c.provider == "ollama") {
        if (j.contains("message")) {
          const auto &m = j["message"];
          text(m.value("content", ""));
          if (m.contains("tool_calls"))
            append_calls(result.message, m["tool_calls"]);
        }
        if (j.value("done", false)) {
          completed = true;
          result.usage.input = j.value("prompt_eval_count", 0);
          result.usage.output = j.value("eval_count", 0);
          result.stop_reason = j.value("done_reason", "stop");
        }
      } else {
        if (j.contains("usage") && !j["usage"].is_null())
          usage(result.usage, j["usage"]);
        for (const auto &choice : j.value("choices", Json::array())) {
          const auto delta = choice.value("delta", Json::object());
          if (delta.contains("content") && delta["content"].is_string())
            text(delta["content"]);
          for (const auto &tc : delta.value("tool_calls", Json::array())) {
            const auto index = tc.value("index", size_t{0});
            auto &call = calls[index];
            if (tc.contains("id"))
              call.id = tc["id"];
            if (tc.contains("function")) {
              const auto &f = tc["function"];
              call.name += f.value("name", "");
              arguments[index] += f.value("arguments", "");
            }
          }
          if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
            result.stop_reason = choice["finish_reason"];
            completed = true;
          }
        }
      }
    };
    SseDecoder sse(frame);
    std::string ndjson;
    HttpResponse response;
    try {
      response = http_.perform(req, cancel, [&](const std::string &chunk) {
        if (c.provider == "ollama") {
          ndjson += chunk;
          size_t pos;
          while ((pos = ndjson.find('\n')) != std::string::npos) {
            auto line = ndjson.substr(0, pos);
            ndjson.erase(0, pos + 1);
            if (!line.empty())
              frame("", line);
          }
        } else
          sse.feed(chunk);
      });
    } catch (const Error &e) {
      if (e.code == 130 || saw || attempt == 3)
        throw;
      clock_.sleep_ms(250 * (1 << attempt), cancel);
      continue;
    }
    if (response.status == 429 || response.status == 408 || response.status >= 500) {
      if (attempt == 3)
        throw Error("Provider temporarily unavailable (HTTP " + std::to_string(response.status) +
                    ")");
      clock_.sleep_ms(250 * (1 << attempt), cancel);
      continue;
    }
    if (response.status < 200 || response.status >= 300)
      throw Error("Provider rejected request (HTTP " + std::to_string(response.status) +
                  "); check credentials, model, and endpoint");
    if (c.provider == "ollama") {
      if (!ndjson.empty())
        frame("", ndjson);
    } else
      sse.finish();
    // A JSON error body must not be mistaken for a successful empty stream.
    if (!saw || !completed)
      throw Error("Truncated or malformed provider stream");
    if (c.provider == "anthropic")
      for (auto &entry : blocks) {
        auto &b = entry.second;
        if (b.value("type", "") == "tool_use") {
          if (!arguments[entry.first].empty())
            b["input"] = parse_object(arguments[entry.first]);
          result.message.calls.push_back(
              {b.at("id"), tool_logical_name(b.at("name")), b.value("input", Json::object())});
        }
        result.message.replay.push_back(b);
      }
    if (c.provider == "openai")
      for (auto &entry : calls) {
        auto &call = entry.second;
        if (call.id.empty() || call.name.empty())
          throw Error("Malformed streamed tool call");
        call.name = tool_logical_name(call.name);
        call.arguments = parse_object(arguments[entry.first]);
        result.message.calls.push_back(call);
      }
    if (result.stop_reason == "length" || result.stop_reason == "max_tokens" ||
        result.stop_reason == "MAX_TOKENS")
      throw Error("Provider output token limit reached");
    return result;
  }
  throw Error("Provider retry limit exceeded");
}
} // namespace codet
