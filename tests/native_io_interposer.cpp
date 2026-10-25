// Host-only, calling-thread injection of socket readiness races and partial native sends.
#include "native_io_metrics.hpp"
#include "native_symbol.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <poll.h>
#include <sys/socket.h>
namespace codet::test {
thread_local NativeIoMetrics native_io_metrics;
}
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
  if (metrics.observe && count == 1 && descriptors[0].events == POLLOUT) {
    ++metrics.waits;
    descriptors[0].revents = POLLOUT;
    return 1;
  }
  return original(descriptors, count, timeout);
}
