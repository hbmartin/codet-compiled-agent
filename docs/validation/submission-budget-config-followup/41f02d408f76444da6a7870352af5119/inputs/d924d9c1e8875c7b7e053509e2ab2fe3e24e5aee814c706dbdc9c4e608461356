#include "codet/computer.hpp"
#include "codet/core.hpp"

namespace codet {
fs::path invocation_directory(const Config &c) {
  return c.state_root.empty() ? c.home : c.state_root;
}
bool uses_responses(const Config &c) {
  if (c.provider == "codex")
    return true;
  if (c.provider != "openai")
    return false;
  const auto found = c.providers.find("openai");
  if (found == c.providers.end())
    return true;
  const auto &p = found->second;
  if (p.wire_api != "auto")
    return p.wire_api == "responses";
  auto base = p.base_url;
  while (!base.empty() && base.back() == '/')
    base.pop_back();
  return base.empty() || base == "https://api.openai.com/v1";
}
ProviderError::ProviderError(const std::string &message, int status, std::string raw)
    : Error(message), http_status(status), body(std::move(raw)) {}
Json ProviderError::details() const {
  Json out = {{"message", what()}, {"http_status", http_status}};
  if (safe_text(body) == body)
    out["body"] = body;
  else {
    out["body_base64"] = base64_encode(body);
    out["body_encoding"] = "base64";
  }
  return out;
}
std::string semantic_transcript(const std::vector<Message> &messages) {
  Json transcript = Json::array();
  for (const auto &m : messages) {
    Json item = {{"role", m.role}};
    if (!m.origin.empty())
      item["origin"] = m.origin;
    if (m.role == "tool") {
      item["call_id"] = m.tool_call_id;
      const auto result = Json::parse(m.text, nullptr, false);
      if (result.is_object()) {
        item["text"] = result.value("content", Json());
        item["is_error"] = result.value("is_error", Json());
        const auto metadata = result.value("metadata", Json::object());
        if (metadata.is_object())
          for (const auto *key : {"recovery", "action_completed", "already_completed", "exit_code",
                                  "session_id", "spill_path", "hook_stop", "artifact"})
            if (metadata.contains(key))
              item[key] = metadata.at(key);
      } else
        item["text"] = m.text;
    } else if (!m.assistant_items.empty()) {
      item["text"] = Json::array();
      for (const auto &text : m.assistant_items)
        item["text"].push_back({{"phase", text.phase}, {"text", text.text}});
    } else
      item["text"] = m.text;
    if (!m.calls.empty())
      item["calls"] = m.calls;
    if (!m.checkpoint_state.is_null())
      item["recovery_state"] = m.checkpoint_state;
    if (!m.result_file.empty())
      item["result_reference"] = m.result_file;
    if (!m.artifact_index.empty())
      item["artifact_reference"] = m.artifact_index;
    for (const auto &image : m.images)
      item["images"].push_back({{"path", image.path}, {"mime", image.mime}});
    transcript.push_back(std::move(item));
  }
  return safe_json(transcript).dump();
}
} // namespace codet
