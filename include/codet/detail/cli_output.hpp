#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>
#include <unistd.h>

namespace codet::detail {
// Borrow the descriptor; never retain bytes for a later flush or alter its flags.
class DescriptorOutput {
public:
  using Write = ssize_t (*)(void *, int, const void *, size_t);

private:
  int descriptor_;
  void *context_;
  Write write_;
  static ssize_t native_write(void *, int descriptor, const void *bytes, size_t size) {
    return ::write(descriptor, bytes, size);
  }
  bool bytes(std::string_view part) {
    while (!part.empty()) {
      const auto size =
          std::min(part.size(), static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
      const auto written = write_(context_, descriptor_, part.data(), size);
      if (written < 0 && errno == EINTR)
        continue;
      if (written <= 0)
        return false;
      part.remove_prefix(static_cast<size_t>(written));
    }
    return true;
  }

public:
  explicit DescriptorOutput(int descriptor, Write write = native_write,
                            void *context = nullptr) noexcept
      : descriptor_(descriptor), context_(context), write_(write) {}
  bool write(std::initializer_list<std::string_view> parts, bool newline) {
    for (auto part : parts)
      if (!bytes(part))
        return false;
    return !newline || bytes("\n");
  }
};
// Required headless records fail closed; line-mode display may attempt later records.
class CliOutput {
  DiagnosticWriter writer_;
  DescriptorOutput descriptor_{STDOUT_FILENO};
  bool native_required_;
  bool failed_ = false;

public:
  CliOutput() noexcept : writer_(std::cout), native_required_(true) {}
  explicit CliOutput(std::ostream &stream) noexcept : writer_(stream), native_required_(false) {}
  explicit CliOutput(DescriptorOutput descriptor) noexcept
      : writer_(std::cout), descriptor_(descriptor), native_required_(true) {}
  bool failed() const noexcept {
    return failed_;
  }
  int status(int prior) const noexcept {
    return prior ? prior : failed_ ? 1 : 0;
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
  void required(std::initializer_list<std::string_view> parts, bool newline = true) {
    if (failed_)
      throw Error("Cannot write standard output");
    try {
      if (native_required_ ? descriptor_.write(parts, newline)
                           : writer_.write(parts, newline, true))
        return;
    } catch (...) {
      failed_ = true;
      throw;
    }
    failed_ = true;
    throw Error("Cannot write standard output");
  }
  void secondary(std::initializer_list<std::string_view> parts) noexcept {
    if (!failed_)
      try {
        required(parts);
      } catch (...) {
        // Reporting a previous operation failure must preserve that failure.
        failed_ = true;
      }
  }
  template <class Build> void secondary_record(Build &&build) noexcept {
    if (!failed_)
      try {
        const auto record = build();
        secondary({record});
      } catch (...) {
        // Building a secondary report does not imply that stdout delivery failed.
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
  std::array<char, 64 * 1024> bytes_{};
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
};
} // namespace codet::detail
