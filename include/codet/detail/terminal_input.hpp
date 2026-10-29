#pragma once
#include <cerrno>
#include <poll.h>
#include <sys/select.h>
#include <unistd.h>
namespace codet::detail {
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
  struct timeval limit {
    milliseconds / 1000, (milliseconds % 1000) * 1000
  };
  return ::select(descriptor + 1, &readers, nullptr, nullptr, &limit);
}
} // namespace codet::detail
