#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include "codet/detail/native_output.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <poll.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace codet::detail {
// Required headless records fail closed; line-mode display may attempt later records.
class CliOutput {
  DiagnosticWriter writer_;
  DescriptorOutput descriptor_{STDOUT_FILENO};
  bool native_required_;
  bool failed_ = false;
  bool cancelled_ = false;

public:
  CliOutput() noexcept : writer_(std::cout), native_required_(true) {}
  explicit CliOutput(const Cancellation &cancel) noexcept
      : writer_(std::cout), descriptor_(STDOUT_FILENO, cancel), native_required_(true) {}
  explicit CliOutput(IoContext &io) noexcept
      : writer_(std::cout, &io), descriptor_(STDOUT_FILENO, &*io.cancel), native_required_(true) {}
  explicit CliOutput(std::ostream &stream) noexcept : writer_(stream), native_required_(false) {}
  explicit CliOutput(DescriptorOutput descriptor) noexcept
      : writer_(std::cout), descriptor_(descriptor), native_required_(true) {}
  bool failed() const noexcept {
    return failed_;
  }
  int status(int prior) const noexcept {
    return prior ? prior : cancelled_ ? 130 : failed_ ? 1 : 0;
  }
  bool display(std::initializer_list<std::string_view> parts, bool newline = true) {
    try {
      const bool written = writer_.write(parts, newline, true);
      failed_ |= !written;
      return written;
    } catch (const std::ios_base::failure &) {
      failed_ = true;
      return false;
    } catch (...) {
      failed_ = true;
      throw;
    }
  }
  void required(std::initializer_list<std::string_view> parts, bool newline = true,
                bool reporting = false) {
    if (cancelled_)
      throw Error("Interrupted", 130);
    if (failed_)
      throw Error("Cannot write standard output");
    try {
      if (native_required_ ? descriptor_.write(parts, newline, reporting)
                           : writer_.write(parts, newline, true))
        return;
    } catch (const Error &error) {
      if (error.code == 130)
        cancelled_ = true;
      else
        failed_ = true;
      throw;
    } catch (...) {
      failed_ = true;
      throw;
    }
    failed_ = true;
    throw Error("Cannot write standard output");
  }
  void secondary(std::initializer_list<std::string_view> parts) noexcept {
    if (!failed_ && !cancelled_)
      try {
        required(parts, true, true);
      } catch (...) {
        // Reporting a previous operation failure must preserve that failure.
        return;
      }
  }
  void answer(const std::string &text, bool terminal) {
    if (terminal)
      required({terminal_text(text)});
    else
      required({text});
  }
  template <class Build> void secondary_record(Build &&build) noexcept {
    if (!failed_ && !cancelled_)
      try {
        const auto record = build();
        secondary({record});
      } catch (...) {
        // Building a secondary report does not imply that stdout delivery failed.
        return;
      }
  }
  void echo(std::initializer_list<std::string_view> parts) {
    // Keyboard echo is not a required reply, and never clears an earlier delivery failure.
    try {
      (void)writer_.write(parts, false, true);
    } catch (const std::ios_base::failure &) {
      return;
    }
  }
};
// One bounded application buffer for a complete text export. Destruction never publishes it.
class TextExport {
  CliOutput &output_;
  std::array<char, size_t{64} * 1024> bytes_{};
  size_t used_ = 0;

public:
  explicit TextExport(CliOutput &output) noexcept : output_(output) {}
  void finish() {
    if (used_) {
      output_.required({std::string_view(bytes_.data(), used_)}, false);
      used_ = 0;
    }
  }
  void append(std::string_view text) {
    while (!text.empty()) {
      const auto count = std::min(text.size(), bytes_.size() - used_);
      std::memcpy(bytes_.data() + used_, text.data(), count);
      used_ += count;
      text.remove_prefix(count);
      if (used_ == bytes_.size())
        finish();
    }
  }
  void append_human(std::string_view text) {
    while (!text.empty()) {
      const auto next = terminal_text_into(text, bytes_.data() + used_, bytes_.size() - used_);
      used_ += next.written;
      text.remove_prefix(next.consumed);
      if (!next.consumed || used_ == bytes_.size())
        finish();
    }
  }
};
} // namespace codet::detail
