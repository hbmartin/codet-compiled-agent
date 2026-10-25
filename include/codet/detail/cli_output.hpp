#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
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
// Borrow the descriptor; never retain bytes for a later flush or alter its flags.
class DescriptorOutput {
public:
  using Write = ssize_t (*)(void *, int, const void *, size_t);
  using WriteVector = ssize_t (*)(void *, int, const iovec *, int);

private:
  int descriptor_;
  void *context_;
  Write write_;
  WriteVector vector_ = nullptr;
  const Cancellation *cancel_;
  size_t limit_ = INT_MAX;
  bool pipe_ = false, valid_ = true;
  static ssize_t native_write(void *, int descriptor, const void *bytes, size_t size) {
    return ::write(descriptor, bytes, size);
  }
  static ssize_t native_vector(void *, int descriptor, const iovec *parts, int count) {
    return ::writev(descriptor, parts, count);
  }
  void check(bool reporting = false) const {
    if (cancel_ && !reporting)
      cancel_->check();
  }
  bool ready(bool reporting) const {
    if (!pipe_)
      return true;
    for (;;) {
      check(reporting);
      pollfd descriptor{descriptor_, POLLOUT, 0};
      const auto result = ::poll(&descriptor, 1, reporting ? 0 : 100);
      const auto error = errno;
      check(reporting);
      if (result > 0)
        return (descriptor.revents & POLLNVAL) == 0;
      if (reporting || (result < 0 && error != EINTR))
        return false;
    }
  }
  bool bytes(std::string_view part, bool reporting) {
    while (!part.empty()) {
      check(reporting);
      if (!valid_ || !ready(reporting))
        return false;
      const auto size = std::min(part.size(), limit_);
      const auto written = write_(context_, descriptor_, part.data(), size);
      const auto error = errno;
      check(reporting);
      if (written < 0 && error == EINTR && !reporting)
        continue;
      if (written <= 0)
        return false;
      part.remove_prefix(static_cast<size_t>(written));
    }
    return true;
  }
  bool gathered(std::initializer_list<std::string_view> parts, bool newline, bool reporting) {
    struct Cursor {
      const std::string_view *part = nullptr, *end = nullptr;
      size_t offset = 0;
      bool newline = false;
      std::string_view view() noexcept {
        while (part != end && offset == part->size()) {
          ++part;
          offset = 0;
        }
        return part != end ? std::string_view(part->data() + offset, part->size() - offset)
               : newline   ? std::string_view("\n")
                           : std::string_view{};
      }
      void advance(size_t count) noexcept {
        while (count) {
          const auto size = std::min(count, view().size());
          if (part != end)
            offset += size;
          else
            newline = false;
          count -= size;
        }
      }
    } cursor{parts.begin(), parts.end(), 0, newline};
    while (!cursor.view().empty()) {
      check(reporting);
      if (!valid_ || !ready(reporting))
        return false;
      iovec batch[16]; // POSIX guarantees at least sixteen vectors.
      size_t total = 0;
      int count = 0;
      auto next = cursor;
      while (count != 16 && total < limit_) {
        const auto piece = next.view();
        if (piece.empty())
          break;
        const auto size = std::min(piece.size(), limit_ - total);
        batch[count++] = {const_cast<char *>(piece.data()), size};
        total += size;
        next.advance(size);
      }
      // A single span gains nothing from a vector syscall, especially on small pipe chunks.
      const auto written = count == 1 && write_
                               ? write_(context_, descriptor_, batch[0].iov_base, batch[0].iov_len)
                               : vector_(context_, descriptor_, batch, count);
      const auto error = errno;
      check(reporting);
      if (written < 0 && error == EINTR && !reporting)
        continue;
      if (written <= 0 || static_cast<size_t>(written) > total)
        return false;
      cursor.advance(static_cast<size_t>(written));
    }
    return true;
  }

public:
  explicit DescriptorOutput(int descriptor, Write write = native_write, void *context = nullptr,
                            const Cancellation *cancel = nullptr) noexcept
      : descriptor_(descriptor), context_(context), write_(write), cancel_(cancel) {
    if (write_ != native_write)
      return;
    vector_ = native_vector;
    struct stat status {};
    const auto flags = ::fcntl(descriptor_, F_GETFL);
    valid_ = flags >= 0 && ::fstat(descriptor_, &status) == 0;
    pipe_ = valid_ && S_ISFIFO(status.st_mode) && !(flags & O_NONBLOCK);
    if (pipe_) {
      const auto bound = ::fpathconf(descriptor_, _PC_PIPE_BUF);
      limit_ = std::min(static_cast<size_t>(INT_MAX),
                        bound > 0 ? static_cast<size_t>(bound) : size_t{512});
    }
  }
  DescriptorOutput(int descriptor, const Cancellation &cancel) noexcept
      : DescriptorOutput(descriptor, native_write, nullptr, &cancel) {}
  DescriptorOutput(int descriptor, WriteVector write, void *context,
                   const Cancellation *cancel = nullptr) noexcept
      : DescriptorOutput(descriptor, static_cast<Write>(nullptr), context, cancel) {
    vector_ = write;
  }
  bool write(std::initializer_list<std::string_view> parts, bool newline, bool secondary = false) {
    // A provider cancellation can still report its final event to an already-writable sink.
    const bool reporting = secondary && cancel_ && cancel_->requested.load();
    check(reporting);
    if (write_ && parts.size() == 1 && !newline)
      return bytes(*parts.begin(), reporting);
    if (vector_)
      return gathered(parts, newline, reporting);
    for (auto part : parts)
      if (!bytes(part, reporting))
        return false;
    return !newline || bytes("\n", reporting);
  }
};
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
