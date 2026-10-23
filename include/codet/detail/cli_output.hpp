#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <iostream>

namespace codet::detail {
// Required headless records fail closed; line-mode display may attempt later records.
class CliOutput {
  DiagnosticWriter writer_;
  bool failed_ = false;

public:
  explicit CliOutput(std::ostream &stream = std::cout) noexcept : writer_(stream) {}
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
      if (writer_.write(parts, newline, true))
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
        (void)display(parts);
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
        failed_ = true;
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
} // namespace codet::detail
