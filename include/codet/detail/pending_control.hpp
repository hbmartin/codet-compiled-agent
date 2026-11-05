#pragma once
#include "codet/detail/local_command.hpp"
#include <array>
#include <limits>
#include <string_view>

namespace codet::detail {
// Extra input owns a control-only line, never a second ordinary message.
template <size_t OverflowLimit = std::numeric_limits<size_t>::max()> class PendingControl {
  std::array<char, 6> prefix_{};
  size_t size_ = 0, overflow_ = 0;
  bool started_ = false, saturated_ = false;

public:
  bool started() const noexcept {
    return started_;
  }
  void clear() noexcept {
    size_ = overflow_ = 0;
    started_ = saturated_ = false;
  }
  void append(char byte) noexcept {
    started_ = true;
    if (saturated_)
      return;
    if (size_ < prefix_.size() && !overflow_)
      prefix_[size_++] = byte;
    else if (overflow_ == OverflowLimit)
      saturated_ = true;
    else
      ++overflow_;
  }
  void erase() noexcept {
    if (saturated_)
      return; // Position cannot be recovered safely before the next line boundary.
    if (overflow_)
      --overflow_;
    else if (size_)
      --size_;
  }
  // The ignored control draft owns its suffix, not the independently frozen submission.
  template <class Advance>
  auto handoff(bool cancelled, Advance advance) const -> decltype(advance()) {
    if (cancelled)
      return {};
    return advance();
  }
  bool submit() noexcept {
    const std::string_view text(prefix_.data(), size_);
    const bool quit = !overflow_ && !saturated_ && is_quit_command(text);
    clear();
    return quit;
  }
};
} // namespace codet::detail
