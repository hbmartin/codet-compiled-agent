#pragma once
#include "codet/detail/diagnostic_stream.hpp"
namespace codet::detail {
inline constexpr std::string_view history_omission = "[UI history excerpt truncated]";
// Each retained row owns its payload and one line charge. A final LF already pays it.
inline size_t history_bytes(std::string_view text) noexcept {
  return text.size() + static_cast<size_t>(!text.empty() && text.back() != '\n');
}
struct HistoryExcerpt {
  size_t prefix;
  bool omitted;
};
inline HistoryExcerpt history_excerpt(std::string_view text, size_t budget,
                                      std::string_view marker = history_omission,
                                      bool overflow = false) noexcept {
  if (!overflow && history_bytes(text) <= budget)
    return {text.size(), false};
  if (budget < marker.size() + 1)
    return {0, false};
  const auto room = budget - marker.size() - 1;
  auto prefix = rendered_prefix(text, room);
  if (prefix && text[prefix - 1] != '\n' && prefix == room)
    prefix = rendered_prefix(text.substr(0, prefix), room - 1);
  return {prefix, true};
}
} // namespace codet::detail
