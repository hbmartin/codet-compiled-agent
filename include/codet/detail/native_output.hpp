#pragma once
#include "codet/core.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
namespace codet::detail {
enum class OutputMode : uint8_t { required, diagnostic, immediate };
enum class OutputResult : uint8_t { success, cancelled, failure };
// Owned by one invocation and borrowed by output/diagnostic controllers, including final catches.
struct IoContext {
  const Cancellation *cancel = nullptr;
  explicit IoContext(const Cancellation &value) noexcept : cancel(&value) {}
};
// One logical delivery owns its interruption budget, including inspection and streamed chunks.
class OutputAttempt {
  const Cancellation *cancel_;
  OutputMode mode_;
  unsigned retries_ = 8;
  bool exhausted_ = false;

public:
  OutputAttempt(const Cancellation *cancel, OutputMode mode) noexcept
      : cancel_(cancel), mode_(mode) {}
  bool cancelled() const noexcept {
    return mode_ == OutputMode::required && cancel_ && cancel_->requested.load();
  }
  bool immediate() const noexcept {
    return mode_ == OutputMode::immediate ||
           (mode_ == OutputMode::diagnostic && (!cancel_ || cancel_->requested.load()));
  }
  bool uncancellable_required() const noexcept {
    return mode_ == OutputMode::required && !cancel_;
  }
  bool interrupted() noexcept {
    if (cancelled())
      return false;
    if (!immediate())
      return true;
    if (!retries_) {
      exhausted_ = true;
      return false;
    }
    --retries_;
    return true;
  }
  template <class Operation> auto inspect(Operation operation) noexcept {
    if (exhausted_) {
      errno = EINTR;
      return static_cast<decltype(operation())>(-1);
    }
    auto result = operation();
    while (result < 0 && errno == EINTR && interrupted())
      result = operation();
    return result;
  }
  bool exhausted() const noexcept {
    return exhausted_;
  }
};
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
  bool pipe_ = false, socket_ = false, nonblocking_ = false, valid_ = true;
  size_t pipe_limit_ = 512;
  OutputAttempt *attempt_ = nullptr;
  bool cancelled() const noexcept {
    return attempt_->cancelled();
  }
  bool immediate() const noexcept {
    return attempt_->immediate();
  }
  static ssize_t native_write(void *, int descriptor, const void *bytes, size_t size) {
    return ::write(descriptor, bytes, size);
  }
  static ssize_t native_vector(void *, int descriptor, const iovec *parts, int count) {
    return ::writev(descriptor, parts, count);
  }
  bool ready(bool socket_wait = false) noexcept {
    if (!socket_wait && (!pipe_ || attempt_->uncancellable_required()))
      return true;
    for (;;) {
      if (cancelled() || (socket_wait && immediate()))
        return false;
      pollfd descriptor{descriptor_, POLLOUT, 0};
      const auto result = ::poll(&descriptor, 1, immediate() ? 0 : 100);
      const auto error = errno;
      if (cancelled())
        return false;
      if (result > 0)
        return (descriptor.revents & (POLLNVAL | POLLERR | POLLHUP)) == 0;
      if (result < 0 && error == EINTR && attempt_->interrupted())
        continue;
      if (immediate() || result < 0)
        return false;
    }
  }
  bool retry(int error) noexcept {
    if (error == EINTR)
      return attempt_->interrupted();
    return (error == EAGAIN || error == EWOULDBLOCK) && socket_ && !nonblocking_ && ready(true);
  }
  ssize_t send(const iovec *parts, int count) noexcept {
    if (socket_) {
      msghdr message{};
      message.msg_iov = const_cast<iovec *>(parts);
      message.msg_iovlen = static_cast<decltype(message.msg_iovlen)>(count);
      // Darwin's sosend checks MSG_NBIO for send-buffer space; MSG_DONTWAIT alone
      // only makes socket-buffer locking nonblocking. This ABI exists in Tiger XNU too.
#ifdef __APPLE__
      constexpr int no_wait = MSG_DONTWAIT | 0x20000; // MSG_NBIO (kernel socket.h).
#else
      constexpr int no_wait = MSG_DONTWAIT;
#endif
      return ::sendmsg(descriptor_, &message, no_wait);
    }
    return count == 1 && write_ ? write_(context_, descriptor_, parts[0].iov_base, parts[0].iov_len)
                                : vector_(context_, descriptor_, parts, count);
  }
  bool bytes(std::string_view part) {
    while (!part.empty()) {
      if (cancelled())
        return false;
      if (!valid_ || !ready())
        return false;
      const auto size = std::min(part.size(), limit_);
      const iovec span{const_cast<char *>(part.data()), size};
      const auto written = send(&span, 1);
      const auto error = errno;
      if (cancelled())
        return false;
      if (written < 0 && retry(error))
        continue;
      if (written <= 0)
        return false;
      part.remove_prefix(static_cast<size_t>(written));
    }
    return true;
  }
  bool gathered(std::initializer_list<std::string_view> parts, bool newline) {
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
      if (cancelled())
        return false;
      if (!valid_ || !ready())
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
      const auto written = send(batch, count);
      const auto error = errno;
      if (cancelled())
        return false;
      if (written < 0 && retry(error))
        continue;
      if (written <= 0 || static_cast<size_t>(written) > total)
        return false;
      cursor.advance(static_cast<size_t>(written));
    }
    return true;
  }

public:
  explicit DescriptorOutput(int descriptor, Write write = native_write, void *context = nullptr,
                            const Cancellation *cancel = nullptr,
                            OutputAttempt *inspection = nullptr) noexcept
      : descriptor_(descriptor), context_(context), write_(write), cancel_(cancel) {
    if (write_ != native_write)
      return;
    vector_ = native_vector;
    struct stat status {};
    const auto inspect = [&](auto operation) {
      return inspection ? inspection->inspect(operation) : operation();
    };
    const auto flags = inspect([&] { return ::fcntl(descriptor_, F_GETFL); });
    valid_ = flags >= 0 && inspect([&] { return ::fstat(descriptor_, &status); }) == 0;
    nonblocking_ = valid_ && (flags & O_NONBLOCK);
    pipe_ = valid_ && S_ISFIFO(status.st_mode) && !nonblocking_;
    socket_ = valid_ && S_ISSOCK(status.st_mode);
    if (pipe_) {
      const auto bound = inspect([&] { return ::fpathconf(descriptor_, _PC_PIPE_BUF); });
      pipe_limit_ = std::min(static_cast<size_t>(INT_MAX),
                             bound > 0 ? static_cast<size_t>(bound) : size_t{512});
    }
  }
  DescriptorOutput(int descriptor, const Cancellation *cancel) noexcept
      : DescriptorOutput(descriptor, native_write, nullptr, cancel) {}
  DescriptorOutput(int descriptor, OutputAttempt &attempt) noexcept
      : DescriptorOutput(descriptor, native_write, nullptr, nullptr, &attempt) {}
  DescriptorOutput(int descriptor, const Cancellation &cancel) noexcept
      : DescriptorOutput(descriptor, native_write, nullptr, &cancel) {}
  DescriptorOutput(int descriptor, WriteVector write, void *context,
                   const Cancellation *cancel = nullptr) noexcept
      : DescriptorOutput(descriptor, static_cast<Write>(nullptr), context, cancel) {
    vector_ = write;
  }

private:
  bool perform(std::initializer_list<std::string_view> parts, bool newline) {
    if (cancelled())
      return false;
    if (write_ && parts.size() == 1 && !newline)
      return bytes(*parts.begin());
    if (vector_)
      return gathered(parts, newline);
    for (auto part : parts)
      if (!bytes(part))
        return false;
    return !newline || bytes("\n");
  }

public:
  OutputResult result(std::initializer_list<std::string_view> parts, bool newline,
                      OutputAttempt &attempt) noexcept {
    attempt_ = &attempt;
    limit_ = pipe_ && !attempt.uncancellable_required() ? pipe_limit_ : size_t{INT_MAX};
    const bool success = !attempt.exhausted() && perform(parts, newline);
    attempt_ = nullptr;
    return attempt.cancelled() ? OutputResult::cancelled
           : success           ? OutputResult::success
                               : OutputResult::failure;
  }
  OutputResult result(std::initializer_list<std::string_view> parts, bool newline,
                      OutputMode mode) noexcept {
    OutputAttempt attempt(cancel_, mode);
    return result(parts, newline, attempt);
  }
  bool write(std::initializer_list<std::string_view> parts, bool newline, bool secondary = false) {
    const auto status =
        result(parts, newline,
               secondary && cancel_ && cancel_->requested.load() ? OutputMode::immediate
                                                                 : OutputMode::required);
    if (status == OutputResult::cancelled)
      throw Error("Interrupted", 130);
    return status == OutputResult::success;
  }
};
} // namespace codet::detail
