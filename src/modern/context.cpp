#include "codet/context.hpp"
#include "codet/support.hpp"
#include <algorithm>
#include <array>
#include <deque>

namespace codet {
namespace {
size_t weight(unsigned char byte, unsigned depth) {
  static const auto table = [] {
    std::array<std::array<size_t, 256>, 4> out{};
    for (size_t i = 0; i < 256; ++i) {
      std::string value(1, static_cast<char>(i));
      for (unsigned d = 0; d < 4; ++d) {
        out[d][i] = i >= 128 ? 1 : value.size();
        if (i < 128) {
          value = Json(value).dump();
          value = value.substr(1, value.size() - 2);
        }
      }
    }
    return out;
  }();
  return table.at(depth)[byte];
}
size_t encoded_size(const std::string &text, unsigned depth) {
  size_t out = 0;
  for (unsigned char ch : text)
    out += weight(ch, depth);
  return out;
}
Json essential(const Json &metadata) {
  Json out = Json::object();
  for (const auto *key : {"exit_code",
                          "session_id",
                          "action_completed",
                          "already_completed",
                          "recovery",
                          "total_output_bytes",
                          "omitted_output_bytes",
                          "original_token_count",
                          "truncated",
                          "spill_bytes",
                          "spill_path",
                          "spill_truncated",
                          "hook_stop",
                          "screenshot_id",
                          "snapshot_id",
                          "display_id",
                          "width",
                          "height",
                          "bounds",
                          "artifact",
                          "action_sent",
                          "application_success_observed",
                          "next_byte_offset",
                          "next_entry_offset",
                          "scan_limited",
                          "bytes_read",
                          "next_line",
                          "total_lines",
                          "total_bytes",
                          "eof"})
    if (metadata.contains(key))
      out[key] = metadata.at(key);
  for (const auto *key : {"capture_error", "post_action_error"})
    if (metadata.contains(key) && metadata.at(key).is_string())
      out[key] = bounded_text(metadata.at(key).get<std::string>(), 512);
  return out;
}
Message clipped(const Message &source, const std::string &name, const std::string &reference,
                size_t extra) {
  auto out = source;
  out.previewed = true;
  out.result_file = source.result_file.empty() ? reference : source.result_file;
  auto envelope = Json::parse(source.text, nullptr, false);
  if (!envelope.is_object())
    envelope = {{"content", source.text}, {"is_error", false}, {"metadata", Json::object()}};
  auto metadata = essential(envelope.value("metadata", Json::object()));
  metadata["result_path"] = out.result_file;
  metadata["projection_truncated"] = true;
  if (name == "read_file") {
    for (const auto *key :
         {"next_byte_offset", "next_line", "next_entry_offset", "eof", "bytes_read"})
      metadata.erase(key);
    metadata["inspect_archived_page"] = true;
  }
  const auto original = envelope.value("content", "");
  const bool exec = name == "exec_command" || name == "write_stdin";
  auto inner = exec ? Json::parse(original, nullptr, false) : Json();
  const bool process = exec && inner.is_object() && inner.contains("output");
  std::string text = process ? inner.value("output", "") : original;
  auto small = process ? essential(inner) : Json();
  const auto finish = [&](const std::string &page, uint64_t omitted) {
    auto meta = metadata;
    meta["model_omitted_bytes"] = omitted;
    if (process) {
      auto data = small;
      data["output"] = page;
      data["result_path"] = out.result_file;
      data["projection_truncated"] = true;
      if (data.contains("total_output_bytes"))
        data["original_token_count"] = (data.at("total_output_bytes").get<uint64_t>() + 3) / 4;
      out.text = Json({{"content", data.dump()},
                       {"is_error", envelope.value("is_error", false)},
                       {"metadata", meta}})
                     .dump();
    } else
      out.text = Json({{"content", page},
                       {"is_error", envelope.value("is_error", false)},
                       {"metadata", meta}})
                     .dump();
  };
  finish("", text.size());
  // Only the added page text changes the already-measured minimal wire envelope.
  // Reserve room for changing omission counters without charging provider wrappers again.
  const auto page = context_excerpt(text, extra > 256 ? extra - 256 : 0, process ? 2 : 1);
  finish(page.text, page.omitted);
  return out;
}
std::map<std::string, std::string> call_names(const std::vector<Message> &messages) {
  std::map<std::string, std::string> names;
  for (const auto &m : messages)
    for (const auto &c : m.calls)
      names[c.id] = c.name;
  return names;
}
} // namespace
RenderedOutput context_excerpt(const std::string &input, size_t limit, unsigned depth,
                               ContextMetrics *metrics) {
  const auto text = safe_text(input);
  if (metrics) {
    metrics->scanned_bytes += input.size() + text.size();
    ++metrics->materializations;
  }
  const auto marker = "\n[... " + std::to_string(text.size()) + " bytes omitted ...]\n";
  const auto reserve = encoded_size(marker, depth);
  const auto room = limit > reserve ? limit - reserve : 0;
  const auto half = room / 2;
  size_t cost = 0, prefix = 0, prefix_cost = 0, suffix_cost = 0;
  bool prefix_done = false;
  std::deque<std::pair<size_t, size_t>> suffix;
  for (size_t at = 0; at < text.size();) {
    const auto ch = static_cast<unsigned char>(text[at]);
    const size_t width = ch < 128 ? 1 : ch < 0xe0 ? 2 : ch < 0xf0 ? 3 : 4;
    size_t unit = 0;
    for (size_t i = 0; i < width; ++i)
      unit += weight(static_cast<unsigned char>(text[at + i]), depth);
    cost += unit;
    if (!prefix_done && prefix_cost + unit <= half) {
      prefix = at + width;
      prefix_cost += unit;
    } else
      prefix_done = true;
    suffix.emplace_back(at, unit);
    suffix_cost += unit;
    while (!suffix.empty() && suffix_cost > room - half) {
      suffix_cost -= suffix.front().second;
      suffix.pop_front();
    }
    at += width;
  }
  if (cost <= limit)
    return {text, 0};
  if (limit < reserve)
    return {"", text.size()};
  const auto tail = suffix.empty() ? text.size() : suffix.front().first;
  const auto omitted = tail - prefix;
  return {text.substr(0, prefix) + "\n[... " + std::to_string(omitted) + " bytes omitted ...]\n" +
              text.substr(tail),
          omitted};
}
void ensure_message_ids(std::vector<Message> &messages) {
  for (size_t i = 0; i < messages.size(); ++i)
    if (messages[i].message_id.empty())
      messages[i].message_id =
          digest(safe_json(Json(messages[i])).dump() + ":" + std::to_string(i));
}
std::vector<size_t> protected_history(const std::vector<Message> &messages) {
  size_t human = messages.size(), batch = messages.size();
  for (size_t i = 0; i < messages.size(); ++i) {
    if (messages[i].role == "user" && (messages[i].origin.empty() || messages[i].origin == "human"))
      human = i;
    if (messages[i].role == "assistant" && !messages[i].calls.empty())
      batch = i;
  }
  std::vector<size_t> keep;
  for (size_t i = 0; i < messages.size(); ++i)
    if (i == human || i >= batch || (i > human && messages[i].origin == "hook"))
      keep.push_back(i);
  return keep;
}
std::vector<Message> project_history(std::vector<Message> &messages, size_t available,
                                     Tools &tools) {
  ensure_message_ids(messages);
  const auto names = call_names(messages);
  const auto full = tools.history_size(messages);
  if (full.total <= available) {
    for (auto &message : messages)
      if (message.role == "tool")
        message.previewed = true;
    return messages;
  }
  std::vector<Message> result = messages;
  std::vector<bool> reducible(messages.size(), false);
  size_t count = 0;
  const auto name = [&](size_t i) {
    const auto found = names.find(messages[i].tool_call_id);
    return found == names.end() ? std::string{} : found->second;
  };
  std::vector<size_t> payloads(messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    if (messages[i].role != "tool" || messages[i].previewed)
      continue;
    const auto envelope = Json::parse(messages[i].text, nullptr, false);
    const bool file_page = envelope.is_object() &&
                           envelope.value("metadata", Json::object()).is_object() &&
                           envelope.value("metadata", Json::object()).contains("next_byte_offset");
    if (name(i) == "read_file" && file_page)
      continue; // Contiguous source pages must retire as a whole, never skip their middle.
    auto floor = clipped(messages[i], name(i), tools.result_reference(messages[i]), 0);
    payloads[i] = tools.result_bytes(floor, name(i));
    if (payloads[i] < tools.result_bytes(messages[i], name(i))) {
      result[i] = std::move(floor);
      reducible[i] = true;
      ++count;
    }
  }
  const auto minimum = tools.history_size(result);
  if (minimum.total > available)
    throw ContextCapacity("Context cannot fit human input, tool arguments and minimal results; "
                          "increase context_tokens");
  size_t extra = available - minimum.total;
  for (size_t i = 0; i < messages.size(); ++i) {
    if (reducible[i]) {
      const auto allowance = extra / count;
      const auto restore = full.messages[i] - minimum.messages[i];
      size_t used = 0;
      if (restore <= allowance) {
        result[i] = messages[i];
        used = restore;
      } else {
        const auto reference = tools.retain_result(messages[i]);
        auto preview = clipped(messages[i], name(i), reference, allowance);
        const auto bytes = tools.result_bytes(preview, name(i));
        if (bytes >= payloads[i] && bytes - payloads[i] <= allowance) {
          used = bytes - payloads[i];
          result[i] = std::move(preview);
        }
      }
      extra -= used;
      --count;
    }
    if (result[i].role == "tool")
      result[i].previewed = true;
  }
  if (tools.history_bytes(result) > available)
    throw ContextCapacity("Stable previews cannot fit; compact the completed conversation");
  messages = result;
  return result;
}
size_t minimal_batch_bytes(const std::vector<Message> &protected_messages,
                           const std::vector<ToolCall> &calls, Tools &tools) {
  auto messages = protected_messages;
  for (const auto &c : calls) {
    Message m;
    m.role = "tool";
    m.message_id = "00000000-0000-0000-0000-000000000000";
    m.tool_call_id = c.id;
    m.text =
        Json({{"content", ""}, {"is_error", false}, {"metadata", {{"action_completed", true}}}})
            .dump();
    messages.push_back(clipped(m, c.name, tools.result_reference(m), 0));
  }
  return tools.history_bytes(messages);
}
} // namespace codet
