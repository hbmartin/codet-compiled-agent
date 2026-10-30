#pragma once
#include <atomic>
#include <cerrno>
#include <poll.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
namespace codet::detail {
// Give resource-pressure failures a bounded backoff, returning to the caller's cancellation
// check even when the sleep itself is interrupted. A permanent error is never reported as EOF.
inline bool retry_terminal_input(int error, const std::atomic<bool> *cancel = nullptr,
                                 int (*pause)(const timespec *, timespec *) = ::nanosleep) noexcept {
  if (error != EINTR && error != EAGAIN && error != ENOMEM)
    return false;
  if (error != EINTR && (!cancel || !cancel->load())) {
    const timespec delay{0, 25000000};
    (void)pause(&delay, nullptr);
  }
  return true;
}
// Darwin poll rejects the /dev/tty alias even when it is a valid controlling terminal.
// select is available on Tiger and preserves the same bounded input/cancellation interval.
inline int wait_terminal_input(int descriptor, int milliseconds) noexcept {
  struct pollfd input {
    descriptor, POLLIN, 0
  };
  const auto ready = ::poll(&input, 1, milliseconds);
  if (ready <= 0 || !(input.revents & POLLNVAL))
    return ready;
  if (descriptor < 0 || descriptor >= FD_SETSIZE || !::isatty(descriptor)) {
    errno = EBADF;
    return -1;
  }
  fd_set readers;
  FD_ZERO(&readers);
  FD_SET(descriptor, &readers);
  struct timeval limit {};
  limit.tv_sec = milliseconds / 1000;
  limit.tv_usec = static_cast<decltype(limit.tv_usec)>((milliseconds % 1000) * 1000);
  return ::select(descriptor + 1, &readers, nullptr, nullptr, &limit);
}
} // namespace codet::detail
