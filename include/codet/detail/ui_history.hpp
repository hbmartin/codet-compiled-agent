#pragma once
#include "codet/detail/diagnostic_stream.hpp"
#include <algorithm>
#include <deque>
namespace codet::detail {
// Rendered payload only. The pending approval document has separate ownership.
class UiHistory {
  std::deque<std::string> lines_;
  size_t bytes_ = 0;
  static constexpr std::string_view omission = "[UI history excerpt truncated]";

public:
  static constexpr size_t byte_limit = size_t{256} * 1024, line_limit = 1000;
  static constexpr size_t approval_limit = size_t{16} * 1024;
  size_t bytes() const noexcept {
    return bytes_;
  }
  const std::deque<std::string> &lines() const noexcept {
    return lines_;
  }
  void add(std::string_view text, size_t budget = byte_limit) {
    budget = std::min(budget, byte_limit);
    if (budget <= omission.size() + 1)
      return;
    const bool truncated = text.size() + 1 > budget;
    const auto count =
        rendered_prefix(text, truncated ? budget - omission.size() - 2 : text.size());
    // Never construct a full-input copy or an unbounded split collection.
    size_t offset = 0;
    const auto append = [&](std::string_view line) {
      lines_.emplace_back(line);
      bytes_ += line.size() + 1;
      while (bytes_ > byte_limit || lines_.size() > line_limit) {
        bytes_ -= lines_.front().size() + 1;
        lines_.pop_front();
      }
    };
    while (offset < count) {
      const auto end = text.substr(0, count).find('\n', offset);
      const auto next = end == std::string_view::npos ? count : end;
      append(text.substr(offset, next - offset));
      offset = next + 1;
    }
    if (truncated)
      append(omission);
  }
};
} // namespace codet::detail
