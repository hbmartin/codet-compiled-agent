// Host-only, calling-thread injection of socket readiness races and partial native sends.
#include "native_io_metrics.hpp"
#include "native_stat_interposer.hpp"
#include "native_symbol.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
namespace codet::test {
thread_local NativeIoMetrics native_io_metrics;
}
namespace {
void after_output(int descriptor, bool newline) {
  auto &metrics = codet::test::native_io_metrics;
  if (descriptor == metrics.interrupt_fd)
    if (auto callback = metrics.after_output) {
      metrics.after_output = nullptr;
      callback(metrics.after_output_context, newline);
    }
}
} // namespace
extern "C" ssize_t sendmsg(int descriptor, const msghdr *message, int flags) {
  using Send = ssize_t (*)(int, const msghdr *, int);
  static const auto original = reinterpret_cast<Send>(codet::test::native_symbol("sendmsg"));
  auto &metrics = codet::test::native_io_metrics;
  if (!metrics.observe)
    return original(descriptor, message, flags);
  ++metrics.sends;
  if (metrics.interrupted) {
    --metrics.interrupted;
    errno = EINTR;
    return -1;
  }
  if (metrics.races) {
    --metrics.races;
    errno = EAGAIN;
    return -1;
  }
  iovec part = message->msg_iov[0];
  part.iov_len = std::min(part.iov_len, metrics.chunk);
  auto shortened = *message;
  shortened.msg_iov = &part;
  shortened.msg_iovlen = 1;
  return original(descriptor, &shortened, flags);
}
extern "C" int poll(pollfd *descriptors, nfds_t count, int timeout) {
  using Poll = int (*)(pollfd *, nfds_t, int);
  static const auto original = reinterpret_cast<Poll>(codet::test::native_symbol("poll"));
  auto &metrics = codet::test::native_io_metrics;
  if (count == 1 && descriptors[0].fd == metrics.interrupt_fd && metrics.input_polls_failed) {
    --metrics.input_polls_failed;
    errno = metrics.input_error;
    return -1;
  }
  if (count == 1 && descriptors[0].fd == metrics.interrupt_fd && metrics.polls_interrupted) {
    --metrics.polls_interrupted;
    errno = EINTR;
    return -1;
  }
  if (metrics.observe && count == 1 && descriptors[0].events == POLLOUT) {
    ++metrics.waits;
    descriptors[0].revents = POLLOUT;
    return 1;
  }
  return original(descriptors, count, timeout);
}
extern "C" ssize_t write(int descriptor, const void *bytes, size_t size) {
  using Write = ssize_t (*)(int, const void *, size_t);
  static const auto original = reinterpret_cast<Write>(codet::test::native_symbol("write"));
  auto &metrics = codet::test::native_io_metrics;
  if (descriptor == metrics.interrupt_fd) {
    if (auto gate = metrics.before_output) {
      metrics.before_output = nullptr;
      gate(metrics.output_context);
    }
    ++metrics.writes;
    if (metrics.scalar_partial) {
      metrics.scalar_partial = false;
      metrics.scalar_failure = true;
      return original(descriptor, bytes, std::min(size, size_t{2}));
    }
    if (metrics.scalar_failure) {
      metrics.scalar_failure = false;
      errno = EIO;
      return -1;
    }
    if (metrics.writes_interrupted) {
      --metrics.writes_interrupted;
      errno = EINTR;
      return -1;
    }
  }
  const auto answer = original(descriptor, bytes, size);
  after_output(descriptor, answer > 0 && static_cast<const char *>(bytes)[answer - 1] == '\n');
  return answer;
}
extern "C" ssize_t writev(int descriptor, const iovec *parts, int count) {
  using Write = ssize_t (*)(int, const iovec *, int);
  static const auto original = reinterpret_cast<Write>(codet::test::native_symbol("writev"));
  auto &metrics = codet::test::native_io_metrics;
  if (descriptor == metrics.interrupt_fd) {
    if (auto gate = metrics.before_output) {
      metrics.before_output = nullptr;
      gate(metrics.output_context);
    }
    ++metrics.vectors;
    if (metrics.vector_partial && count) {
      metrics.vector_partial = false;
      metrics.vector_failure = true;
      iovec part = parts[0];
      part.iov_len = std::min(part.iov_len, size_t{2});
      return original(descriptor, &part, 1);
    }
    if (metrics.vector_failure) {
      metrics.vector_failure = false;
      errno = EIO;
      return -1;
    }
    if (metrics.vectors_interrupted) {
      --metrics.vectors_interrupted;
      errno = EINTR;
      return -1;
    }
  }
  const auto answer = original(descriptor, parts, count);
  if (descriptor == metrics.interrupt_fd && metrics.after_output) {
    size_t remaining = answer > 0 ? static_cast<size_t>(answer) : 0;
    bool newline = false;
    for (int i = 0; i < count && remaining; ++i) {
      const auto consumed = std::min(remaining, parts[i].iov_len);
      if (consumed)
        newline = static_cast<const char *>(parts[i].iov_base)[consumed - 1] == '\n';
      remaining -= consumed;
    }
    after_output(descriptor, newline);
  }
  return answer;
}
void codet::test::observed_stat(int descriptor) noexcept {
  auto &metrics = native_io_metrics;
  if (descriptor == metrics.interrupt_fd)
    if (auto gate = metrics.after_stat) {
      metrics.after_stat = nullptr;
      gate(metrics.stat_context);
    }
}
bool codet::test::interrupt_stat(int descriptor) noexcept {
  auto &metrics = native_io_metrics;
  if (descriptor == metrics.interrupt_fd) {
    ++metrics.inspections;
    if (metrics.stats_interrupted) {
      --metrics.stats_interrupted;
      return true;
    }
  }
  return false;
}
extern "C" long fpathconf(int descriptor, int name)
#ifdef __linux__
    noexcept
#endif
{
  using Bound = long (*)(int, int);
  static const auto original = reinterpret_cast<Bound>(codet::test::native_symbol("fpathconf"));
  auto &metrics = codet::test::native_io_metrics;
  if (descriptor == metrics.interrupt_fd) {
    if (auto gate = metrics.before_bound) {
      metrics.before_bound = nullptr;
      gate(metrics.bound_context);
    }
    ++metrics.bound_queries;
    if (metrics.bounds_interrupted) {
      --metrics.bounds_interrupted;
      errno = EINTR;
      return -1;
    }
    if (metrics.bound_indeterminate)
      return -1; // POSIX permits an indeterminate limit to leave errno unchanged.
  }
  return original(descriptor, name);
}
