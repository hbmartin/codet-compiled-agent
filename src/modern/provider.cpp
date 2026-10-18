#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/support.hpp"
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
bool public_responses_reasoning(const Config &c) {
  return c.provider == "openai" &&
         (!c.effort.empty() || c.model == "gpt-6.1-sol" || c.model == "gpt-6-astra");
}
std::string join_url(std::string base, const std::string &suffix) {
  while (!base.empty() && base.back() == '/')
    base.pop_back();
  return base + suffix;
}
std::string tool_text(const Message &m, const Config &c,
                      const std::map<std::string, std::string> &names) {
  std::string text = m.text;
  if (m.role == "tool") {
    auto value = Json::parse(m.text, nullptr, false);
    if (value.is_object() && value.contains("content") && value.contains("is_error")) {
      auto metadata = value.value("metadata", Json::object());
      const auto resolve = [&](Json &data) {
        if (!data.is_object())
          return;
        for (const auto *key : {"spill_path", "result_path"}) {
          if (!data.contains(key) || !data.at(key).is_string())
            continue;
          const auto ref = data.at(key).get<std::string>();
          if (result_name(ref))
            data[key] = (artifact_directory(c) / ref).string();
          else if (capture_name(ref) && fs::path(ref) == fs::path(ref).filename())
            data[key] = (capture_directory(c) / ref).string();
        }
      };
      resolve(metadata);
      const auto found = names.find(m.tool_call_id);
      if (found != names.end() &&
          (found->second == "exec_command" || found->second == "write_stdin")) {
        auto content = Json::parse(value.at("content").get<std::string>(), nullptr, false);
        if (content.is_object()) {
          resolve(content);
          value["content"] = content.dump();
        }
      }
      if (metadata.is_object() &&
          (metadata.value("projection_truncated", false) || metadata.contains("next_byte_offset") ||
           metadata.contains("next_entry_offset")))
        text = (value.value("is_error", false) ? std::string("Tool error: ") : std::string{}) +
               value.at("content").get<std::string>() + "\n<codet_result>" + metadata.dump() +
               "</codet_result>";
      else
        text = (value.value("is_error", false) ? std::string("Tool error: ") : std::string{}) +
               value.at("content").get<std::string>();
    }
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
// Each wire item (or appended block in a coalesced role) is encoded once for sizing.
// Global framing belongs to fixed; tool payload deltas keep the same surrounding shape.
void account_wire(ProviderHistorySize *size, size_t owner, size_t bytes) {
  if (!size)
    return;
  size->total += bytes;
  ++size->encoded_parts;
  if (owner < size->messages.size())
    size->messages[owner] += bytes;
  else
    size->fixed += bytes;
}
struct WireOutput {
  Json data = Json::array();
  size_t items = 0, blocks = 0;
  std::string role;
  Json take(ProviderHistorySize *size) {
    if (size)
      size->aggregate_items += data.size();
    return std::move(data);
  }
};
void append_wire(WireOutput &out, Json item, size_t owner, ProviderHistorySize *size) {
  if (size)
    account_wire(size, owner, item.dump().size() + (out.items ? 1 : 0));
  ++out.items;
  out.role = item.value("role", "");
  out.blocks = item.contains("content") && item["content"].is_array() ? item["content"].size()
               : item.contains("parts")                               ? item["parts"].size()
                                                                      : 0;
  if (!size)
    out.data.push_back(std::move(item));
}
void extend_wire(WireOutput &out, const std::string &role, const char *field, Json parts,
                 size_t owner, ProviderHistorySize *size) {
  if (parts.empty())
    return;
  if (out.items && out.role == role) {
    for (auto &part : parts) {
      if (size)
        account_wire(size, owner, part.dump().size() + (out.blocks ? 1 : 0));
      else
        out.data.back()[field].push_back(std::move(part));
      ++out.blocks;
    }
  } else
    append_wire(out, {{"role", role}, {field, std::move(parts)}}, owner, size);
}
Json chat_messages(const Config &c, const std::string &system, const std::vector<Message> &messages,
                   bool ollama, ProviderHistorySize *size) {
  std::map<std::string, std::string> tool_names;
  for (const auto &m : messages)
    for (const auto &call : m.calls)
      tool_names[call.id] = call.name;
  WireOutput out;
  append_wire(out, {{"role", "system"}, {"content", system}}, messages.size(), size);
  size_t owner = 0, pending_owner = 0;
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
    append_wire(out, std::move(item), pending_owner, size);
    pending = Json::array();
  };
  std::map<std::string, std::string> names;
  for (owner = 0; owner < messages.size(); ++owner) {
    const auto &m = messages[owner];
    if (m.role != "tool")
      flush();
    Json j = {{"role", m.role}, {"content", tool_text(m, c, tool_names)}};
    for (const auto &image : prepared_images(m)) {
      if (pending.empty())
        pending_owner = owner;
      pending.push_back(image);
    }
    if (m.role == "tool") {
      if (ollama)
        j["tool_name"] = names[m.tool_call_id];
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
    append_wire(out, std::move(j), owner, size);
  }
  flush();
  return out.take(size);
}
Json wire_history(const Config &c, const std::string &system, const std::vector<Message> &messages,
                  const std::map<std::string, std::string> *known_names = nullptr,
                  ProviderHistorySize *size = nullptr) {
  if (size)
    size->messages.resize(messages.size());
  std::map<std::string, std::string> tool_names =
      known_names ? *known_names : std::map<std::string, std::string>{};
  for (const auto &m : messages)
    for (const auto &call : m.calls)
      tool_names[call.id] = call.name;
  if (uses_responses(c)) {
    WireOutput input;
    append_wire(input, {{"role", "developer"}, {"content", system}}, messages.size(), size);
    // Earlier code revisited every prior item after each ordinary message. Count those
    // visits once; each call now takes work bounded by its qualified name length.
    std::vector<size_t> normalizations(messages.size());
    size_t remaining = 0;
    if (c.provider == "codex")
      for (size_t i = messages.size(); i > 0; --i) {
        const auto &m = messages[i - 1];
        if (m.role != "tool" && !(m.role == "assistant" && !clean_replay(m).empty() &&
                                  (m.replay_provider.empty() || m.replay_provider == c.provider)))
          ++remaining;
        normalizations[i - 1] = remaining;
      }
    const auto append = [&](Json item, size_t owner) {
      if (normalizations[owner] && item.value("type", "") == "function_call" &&
          item.contains("name")) {
        auto qualified = item.at("name").get<std::string>();
        for (size_t n = normalizations[owner]; n; --n) {
          const auto split = qualified.find("__");
          if (split == std::string::npos)
            break;
          item["namespace"] = qualified.substr(0, split);
          qualified = qualified.substr(split + 2);
        }
        item["name"] = qualified;
      }
      append_wire(input, std::move(item), owner, size);
    };
    std::map<std::string, bool> custom_calls;
    for (size_t owner = 0; owner < messages.size(); ++owner) {
      const auto &m = messages[owner];
      if (m.role == "assistant" && !clean_replay(m).empty() &&
          (m.replay_provider.empty() || m.replay_provider == c.provider)) {
        for (const auto &item : clean_replay(m)) {
          append(item, owner);
          if (item.contains("call_id") && item["call_id"].is_string())
            custom_calls[item["call_id"].get<std::string>()] =
                item.value("type", "") == "custom_tool_call";
        }
        continue;
      }
      if (m.role == "tool") {
        // Match the call type actually emitted for this provider, including provider changes.
        const bool custom = custom_calls[m.tool_call_id];
        Json item = {{"type", custom ? "custom_tool_call_output" : "function_call_output"},
                     {"call_id", m.tool_call_id},
                     {"output", tool_text(m, c, tool_names)}};
        if (!prepared_images(m).empty()) {
          auto &output = item["output"];
          output = Json::array({{{"type", "input_text"}, {"text", tool_text(m, c, tool_names)}}});
          for (const auto &image : prepared_images(m))
            output.push_back(
                {{"type", "input_image"}, {"image_url", image.at("url")}, {"detail", "high"}});
        }
        append(std::move(item), owner);
      } else {
        append({{"role", m.role == "system" ? "developer" : m.role},
                {"content", tool_text(m, c, tool_names)}},
               owner);
        if (!prepared_images(m).empty()) {
          Json content = Json::array();
          for (const auto &image : prepared_images(m))
            content.push_back(
                {{"type", "input_image"}, {"image_url", image.at("url")}, {"detail", "high"}});
          append({{"role", "user"}, {"content", content}}, owner);
        }
        for (const auto &t : m.calls) {
          const bool custom = c.provider == "codex" && t.name == "apply_patch";
          custom_calls[t.id] = custom;
          if (custom) {
            const auto patch = t.arguments.value("patch", Json());
            append({{"type", "custom_tool_call"},
                    {"call_id", t.id},
                    {"name", "apply_patch"},
                    {"input", patch.is_string() ? patch.get<std::string>() : t.arguments.dump()}},
                   owner);
          } else
            append({{"type", "function_call"},
                    {"call_id", t.id},
                    {"name", tool_wire_name(t.name)},
                    {"arguments", t.arguments.dump()}},
                   owner);
        }
      }
    }
    return input.take(size);
  }
  if (c.provider == "anthropic") {
    WireOutput ms;
    for (size_t owner = 0; owner < messages.size(); ++owner) {
      const auto &m = messages[owner];
      Json content = Json::array();
      std::string role = m.role == "tool" ? "user" : m.role;
      if (role == "system") {
        content.push_back({{"type", "text"}, {"text", tool_text(m, c, tool_names)}});
        if (c.model != "claude-sonnet-5-5")
          role = "user";
      } else if (m.role == "tool")
        content.push_back({{"type", "tool_result"},
                           {"tool_use_id", m.tool_call_id},
                           {"content", tool_text(m, c, tool_names)}});
      else {
        if (m.role == "assistant" && !clean_replay(m).empty() &&
            (m.replay_provider.empty() || m.replay_provider == c.provider))
          content = clean_replay(m);
        else {
          if (!tool_text(m, c, tool_names).empty())
            content.push_back({{"type", "text"}, {"text", tool_text(m, c, tool_names)}});
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
          blocks.push_back({{"type", "text"}, {"text", tool_text(m, c, tool_names)}});
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
      extend_wire(ms, role, "content", std::move(content), owner, size);
    }
    return ms.take(size);
  }
  if (c.provider == "google") {
    WireOutput contents;
    std::map<std::string, std::string> names;
    for (size_t owner = 0; owner < messages.size(); ++owner) {
      const auto &m = messages[owner];
      Json parts = Json::array();
      for (const auto &t : m.calls)
        names[t.id] = tool_wire_name(t.name);
      if (m.role == "assistant" && !clean_replay(m).empty() &&
          (m.replay_provider.empty() || m.replay_provider == c.provider))
        parts = clean_replay(m);
      else if (m.role == "tool")
        parts.push_back({{"functionResponse",
                          {{"name", names[m.tool_call_id]},
                           {"response", {{"result", tool_text(m, c, tool_names)}}}}}});
      else {
        if (!tool_text(m, c, tool_names).empty())
          parts.push_back({{"text", tool_text(m, c, tool_names)}});
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
        extend_wire(contents, role, "parts", std::move(parts), owner, size);
      }
    }
    return contents.take(size);
  }
  return chat_messages(c, system, messages, c.provider == "ollama", size);
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
size_t anthropic_image_bytes(const Json &content, size_t &images) {
  if (!content.is_array())
    return 0;
  size_t bytes = 0;
  for (const auto &block : content) {
    if (!block.is_object())
      continue;
    if (block.value("type", Json()) == "tool_result" && block.contains("content"))
      bytes += anthropic_image_bytes(block.at("content"), images);
    else if (block.value("type", Json()) == "image" && block.contains("source")) {
      const auto &source = block.at("source");
      if (source.is_object() && source.value("type", Json()) == "base64" &&
          source.contains("data") && source.at("data").is_string()) {
        ++images;
        bytes += source.at("data").get_ref<const std::string &>().size();
      }
    }
  }
  return bytes;
}
std::string source_hash(const Message &m) {
  Json value = {
      {"role", m.role}, {"text", m.text}, {"calls", m.calls}, {"replay", clean_replay(m)}};
  for (const auto &image : m.images)
    value["images"].push_back(
        {{"sha256", image.sha256}, {"mime", image.mime}, {"detail", image.detail}});
  for (const auto &item : m.assistant_items)
    value["assistant_text"].push_back({{"text", item.text}, {"phase", item.phase}});
  return digest(safe_json(value).dump());
}
Json epoch_request(Json &state, const Config &c, const std::string &system, const Json &tools,
                   const std::string &key, const std::vector<Message> &messages,
                   const std::map<std::string, std::string> &names) {
  size_t first = 0;
  const auto provider = c.providers.find(c.provider);
  const auto endpoint = provider == c.providers.end() ? std::string{} : provider->second.base_url;
  const auto artifacts = digest(artifact_directory(c).string()),
             captures = digest(capture_directory(c).string());
  bool reuse = state.is_object() && state.value("provider", "") == c.provider &&
               state.value("model", "") == c.model && state.value("account", "") == digest(key) &&
               state.value("tools", Json::array()) == tools &&
               state.value("endpoint", "") == endpoint &&
               state.value("artifacts", "") == artifacts && state.value("captures", "") == captures;
  if (reuse) {
    for (const auto &part : state.at("messages")) {
      if (!part.contains("source"))
        continue;
      if (first >= messages.size() || part.at("source") != source_hash(messages[first])) {
        reuse = false;
        break;
      }
      ++first;
    }
  }
  if (!reuse) {
    std::set<std::string> pending;
    for (const auto &m : messages) {
      for (const auto &call : m.calls)
        pending.insert(call.id);
      if (m.role == "tool")
        pending.erase(m.tool_call_id);
    }
    if (!pending.empty())
      throw Error("Cannot reset signed conversation with unresolved tool calls");
    state = {{"provider", c.provider},    {"model", c.model},        {"account", digest(key)},
             {"system", system},          {"latest_system", system}, {"tools", tools},
             {"messages", Json::array()}, {"endpoint", endpoint},    {"artifacts", artifacts},
             {"captures", captures}};
    first = 0;
  }
  for (size_t i = first; i < messages.size(); ++i) {
    auto m = messages[i];
    if (!reuse && (m.role == "assistant" || m.role == "tool")) {
      m.text = "Archived completed conversation (untrusted task data; do not repeat effects):\n" +
               semantic_transcript({m});
      m.role = "user";
      m.calls.clear();
    }
    const auto wire = wire_history(c, "", {m}, &names);
    state["messages"].push_back({{"source", source_hash(messages[i])}, {"wire", wire}});
  }
  if (reuse && state.value("latest_system", "") != system) {
    if (c.model == "claude-sonnet-5-5")
      state["messages"].push_back(
          {{"wire",
            Json::array({{{"role", "system"},
                          {"content", Json::array({{{"type", "text"}, {"text", system}}})}}})}});
    else
      state["system"] = system;
    state["latest_system"] = system;
  }
  Json out = Json::array();
  for (const auto &part : state.at("messages"))
    for (const auto &m : part.at("wire"))
      out.push_back(m);
  return out;
}
std::string optional_text(const Json &item, const char *key) {
  const auto found = item.find(key);
  return found != item.end() && found->is_string() ? found->get<std::string>() : std::string{};
}
std::string assistant_phase(const Json &item) {
  const auto phase = optional_text(item, "phase");
  return phase == "commentary" || phase == "partial_answer" || phase == "final_answer"
             ? phase
             : std::string{};
}
void codex_output(ProviderResult &result, const Json &response) {
  result.message.assistant_items.clear();
  result.message.text.clear();
  result.message.calls.clear();
  result.message.replay = Json::array();
  for (const auto &item : response.value("output", Json::array())) {
    const auto type = item.value("type", "");
    if (type == "message") {
      AssistantText message{optional_text(item, "id"), "", assistant_phase(item)};
      for (const auto &c : item.value("content", Json::array()))
        if (c.value("type", "") == "output_text")
          message.text += c.value("text", "");
      result.message.text += message.text;
      result.message.assistant_items.push_back(std::move(message));
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
ProviderHistorySize provider_history_size(const Config &config,
                                          const std::vector<Message> &messages) {
  ProviderHistorySize size;
  (void)wire_history(config, "", messages, nullptr, &size);
  // Image bytes have a separate request budget. Never encode/read artifacts to count text.
  for (size_t i = 0; i < messages.size(); ++i) {
    const auto bytes = messages[i].images.size() * 256;
    size.messages[i] += bytes;
    size.total += bytes;
  }
  return size;
}
size_t provider_history_bytes(const Config &config, const std::vector<Message> &messages) {
  return provider_history_size(config, messages).total;
}
size_t provider_result_bytes(const Config &config, const Message &message,
                             const std::string &name) {
  return Json(tool_text(message, config, {{message.tool_call_id, name}})).dump().size();
}
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
  if (uses_responses(c)) {
    if (c.provider == "codex") {
      r.url = p.base_url.empty() ? "https://chatgpt.com/backend-api/codex/responses" : p.base_url;
      if (!auth.contains("access_token"))
        throw Error("Missing imported ChatGPT credentials");
      r.headers["Authorization"] = "Bearer " + auth["access_token"].get<std::string>();
      r.headers["originator"] = "codex_cli_rs";
      if (auth.contains("account_id"))
        r.headers["ChatGPT-Account-Id"] = auth["account_id"].get<std::string>();
    } else {
      if (key.empty())
        throw Error("Missing OpenAI API key");
      r.url = join_url(p.base_url.empty() ? "https://api.openai.com/v1" : p.base_url, "/responses");
      r.headers["Authorization"] = "Bearer " + key;
    }
    const auto input = wire_history(c, system, messages);
    Json ft = Json::array();
    for (const auto &t : tools) {
      if (c.provider == "codex" && t["name"] == "apply_patch") {
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
    if (c.provider == "codex") {
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
    }
    body = {{"model", c.model},
            {"input", input},
            {"store", false},
            {"stream", true},
            {"parallel_tool_calls", false}};
    if (!ft.empty())
      body["tools"] = ft;
    const bool reasoning =
        c.provider == "openai" ? public_responses_reasoning(c) : !c.effort.empty();
    if (reasoning)
      body["reasoning"] = {{"effort", c.effort.empty() ? "medium" : c.effort}, {"summary", "auto"}};
    if (reasoning || c.provider == "codex")
      body["include"] = Json::array({"reasoning.encrypted_content"});
    if (c.provider == "openai") {
      body["max_output_tokens"] = c.max_output_tokens;
      if ((c.model == "gpt-6.1-sol" || c.model == "gpt-6-astra") &&
          (c.effort == "none" || c.effort == "minimal"))
        throw Error("This model requires reasoning effort low or greater", 2);
    }
  } else if (c.provider == "anthropic") {
    if (key.empty())
      throw Error("Missing Anthropic API key");
    r.url = join_url(p.base_url.empty() ? "https://api.anthropic.com" : p.base_url, "/v1/messages");
    r.headers["x-api-key"] = key;
    r.headers["anthropic-version"] = "2023-06-01";
    auto ms = wire_history(c, system, messages);
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
    if (c.model == "claude-sonnet-5-5" || conversation_.is_object()) {
      std::map<std::string, std::string> tool_names;
      for (const auto &m : messages)
        for (const auto &call : m.calls)
          tool_names[call.id] = call.name;
      const auto declarations = body.value("tools", Json::array());
      body["messages"] =
          epoch_request(conversation_, c, system, declarations, key, messages, tool_names);
      body["system"][0]["text"] = conversation_.at("system");
      const auto capacity = static_cast<uint64_t>(c.context_tokens) * 3;
      const auto reserve = static_cast<uint64_t>(c.max_output_tokens) * 3;
      const auto fits = [&] {
        auto text_bytes = body.dump().size();
        size_t images = 0, image_bytes = 0;
        for (const auto &message : body.at("messages"))
          image_bytes += anthropic_image_bytes(message.at("content"), images);
        text_bytes -= image_bytes;
        return images <= 4 && image_bytes / 4 * 3 <= image_file_limit && reserve < capacity &&
               text_bytes <= capacity - reserve;
      };
      if (!fits()) {
        conversation_ = nullptr;
        body["messages"] =
            epoch_request(conversation_, c, system, declarations, key, messages, tool_names);
        body["system"][0]["text"] = conversation_.at("system");
        if (!fits())
          throw Error("Signed conversation cannot fit context_tokens; compact history or increase "
                      "context_tokens");
      }
    }
    if (!c.effort.empty() || c.model == "claude-sonnet-5-5") {
      body["thinking"] = {{"type", "adaptive"}};
      body["output_config"] = {{"effort", c.effort.empty() ? "medium" : c.effort}};
      if (c.model == "claude-sonnet-5-5")
        body["thinking"]["display"] = "summarized";
    }
  } else if (c.provider == "google") {
    if (key.empty())
      throw Error("Missing Google API key");
    r.url = join_url(p.base_url.empty() ? "https://generativelanguage.googleapis.com" : p.base_url,
                     "/v1beta/models/" + url_encode(c.model) + ":streamGenerateContent?alt=sse");
    r.headers["x-goog-api-key"] = key;
    const auto contents = wire_history(c, system, messages);
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
    if (!ollama && !tools.empty() && (c.model == "gpt-6.1-sol" || c.model == "gpt-6-astra"))
      throw Error("This model requires providers.openai.wire_api=responses for tool calling", 2);
    if (!ollama && key.empty())
      throw Error("Missing OpenAI API key");
    r.url = join_url(p.base_url.empty()
                         ? (ollama ? "http://localhost:11434" : "https://api.openai.com/v1")
                         : p.base_url,
                     ollama ? "/api/chat" : "/chat/completions");
    if (!key.empty())
      r.headers["Authorization"] = "Bearer " + key;
    body = {{"model", c.model}, {"messages", wire_history(c, system, messages)}, {"stream", true}};
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
                                     const Cancellation &cancel, const ChunkSink &sink,
                                     const std::function<void(const Json &)> &items) {
  const Json auth = c.provider == "codex" ? credentials_.codex(http_, cancel) : Json::object();
  const auto req = request(c, system, messages, tools, auth);
  for (int attempt = 0; attempt < 4; ++attempt) {
    ProviderResult result;
    result.message.role = "assistant";
    result.message.replay_provider = c.provider;
    bool completed = false, saw = false;
    std::string stream_error;
    bool stream_failed = false;
    std::map<size_t, AssistantText> streamed;
    auto item_event = [&](const std::string &type, const AssistantText &item,
                          const std::string &delta = std::string{}) {
      if (!items)
        return;
      Json value = {{"id", item.id}, {"type", "agent_message"}};
      if (!item.phase.empty())
        value["phase"] = item.phase;
      if (type == "item.updated")
        value["text_delta"] = delta;
      else
        value["text"] = "";
      items({{"type", type}, {"item", value}});
    };
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
      if (stream_failed)
        return;
      if (event == "response.incomplete") {
        saw = true;
        throw ProviderError("Provider response incomplete (output limit or interruption)", 200,
                            data);
      }
      if (event == "error" || event == "response.failed") {
        stream_failed = true;
        stream_error = data;
        saw = true;
        return;
      }
      const auto j = parse_object(data);
      saw = true;
      const auto type = j.value("type", event);
      if (j.contains("error") || type == "error" || type == "response.failed") {
        stream_error = data;
        stream_failed = true;
        return;
      }
      if (uses_responses(c)) {
        if (type == "response.output_item.added" || type == "response.output_text.delta") {
          const auto index = j.value("output_index", size_t{0});
          const auto source = j.value("item", Json::object());
          if (type == "response.output_text.delta" || source.value("type", "") == "message") {
            auto found = streamed.find(index);
            if (found == streamed.end()) {
              auto id = optional_text(source, "id");
              if (id.empty())
                id = optional_text(j, "item_id");
              if (id.empty())
                id = unique_id();
              AssistantText item{id, "", assistant_phase(source)};
              found = streamed.emplace(index, std::move(item)).first;
              item_event("item.started", found->second);
            }
            if (type == "response.output_text.delta") {
              const auto delta = j.value("delta", "");
              found->second.text += delta;
              if (items)
                item_event("item.updated", found->second, delta);
              else
                text(delta);
            }
          }
        }
        if (type == "response.completed") {
          codex_output(result, j.at("response"));
          size_t message_index = 0;
          const auto &output = j.at("response").value("output", Json::array());
          for (size_t i = 0; i < output.size(); ++i) {
            if (output[i].value("type", "") != "message")
              continue;
            auto &item = result.message.assistant_items.at(message_index++);
            if (streamed.count(i)) {
              // The stream's ID remains stable even if the completed envelope omits it.
              item.id = streamed.at(i).id;
            } else {
              if (item.id.empty())
                item.id = unique_id();
              item_event("item.started", item);
            }
          }
          completed = true;
        }
        if (type == "response.incomplete")
          throw ProviderError("Provider response incomplete (output limit or interruption)", 200,
                              data);
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
        throw ProviderError("Provider temporarily unavailable (HTTP " +
                                std::to_string(response.status) + ")",
                            response.status, response.body);
      clock_.sleep_ms(250 * (1 << attempt), cancel);
      continue;
    }
    if (response.status < 200 || response.status >= 300)
      throw ProviderError("Provider rejected request (HTTP " + std::to_string(response.status) +
                              ")",
                          response.status, response.body);
    if (c.provider == "ollama") {
      if (!ndjson.empty())
        frame("", ndjson);
    } else
      sse.finish();
    if (stream_failed)
      throw ProviderError("Provider stream reported an error", response.status, stream_error);
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
    if (c.provider == "openai" && !uses_responses(c))
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
    if (c.provider == "anthropic" && conversation_.is_object()) {
      Json wire = Json::array();
      if (!result.message.replay.empty())
        wire.push_back({{"role", "assistant"}, {"content", result.message.replay}});
      conversation_["messages"].push_back(
          {{"source", source_hash(result.message)}, {"wire", std::move(wire)}});
    } else
      conversation_ = nullptr;
    return result;
  }
  throw Error("Provider retry limit exceeded");
}
} // namespace codet
