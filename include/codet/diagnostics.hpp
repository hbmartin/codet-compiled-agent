#pragma once
#include "codet/core.hpp"
#include <ostream>
namespace codet {
enum class DiagnosticLevel : uint8_t { info, warning, error };
inline std::string hook_diagnostic(const std::string &event, DiagnosticLevel level,
                                   const std::string &text) {
  if (event == "SessionEnd")
    return std::string(level == DiagnosticLevel::error     ? "SessionEnd hook cleanup failed: "
                       : level == DiagnosticLevel::warning ? "SessionEnd hook warning: "
                                                           : "SessionEnd hook: ") +
           text;
  return "Hook: " + text;
}
// One budget includes complete prefixes, UTF-8 repair, newlines and an omission notice.
class DiagnosticBuffer {
  size_t limit_, used_ = 0;
  bool omitted_ = false;
  std::vector<std::string> messages_;

public:
  explicit DiagnosticBuffer(size_t limit) : limit_(limit) {}
  static std::string notice() {
    return "[Additional diagnostics omitted or truncated]";
  }
  std::optional<std::string> add(const std::string &text) {
    const auto reserve = std::min(limit_, notice().size() + 1);
    const auto payload = limit_ - reserve;
    auto clean = safe_text(text);
    if (used_ >= payload) {
      omitted_ = true;
      return {};
    }
    const auto room = payload - used_ - 1;
    if (clean.size() > room) {
      clean = bounded_text(clean, room);
      omitted_ = true;
    }
    if (clean.empty()) {
      omitted_ = true;
      return {};
    }
    used_ += clean.size() + 1;
    messages_.push_back(clean);
    return clean;
  }
  bool omitted() const {
    return omitted_;
  }
  const std::vector<std::string> &messages() const {
    return messages_;
  }
  void flush(std::ostream &stream) {
    for (const auto &message : messages_)
      stream << message << '\n';
    if (omitted_ && limit_)
      stream << bounded_text(notice(), limit_ ? limit_ - 1 : 0) << '\n';
    messages_.clear();
    used_ = 0;
    omitted_ = false;
  }
};
} // namespace codet
