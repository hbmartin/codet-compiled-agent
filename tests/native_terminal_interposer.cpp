// Host-only observations; native calls still execute against a real PTY.
#include "native_symbol.hpp"
#include "native_terminal_metrics.hpp"
#include <curses.h>
#include <dlfcn.h>
#include <termios.h>
#include <unistd.h>
namespace codet::test {
NativeTerminalMetrics native_terminal_metrics;
}
extern "C" int endwin() {
  using End = int (*)();
  static const auto original = reinterpret_cast<End>(codet::test::native_symbol("endwin"));
  const auto status = original();
  if (codet::test::native_terminal_metrics.observe)
    ++codet::test::native_terminal_metrics.ends;
  return status;
}
extern "C" void delscreen(SCREEN *screen) {
  using Delete = void (*)(SCREEN *);
  static const auto original = reinterpret_cast<Delete>(codet::test::native_symbol("delscreen"));
  auto &metrics = codet::test::native_terminal_metrics;
  if (metrics.observe) {
    metrics.ended_before_delete = metrics.ends == 1 && ::isendwin();
    ++metrics.deletes;
  }
  original(screen);
}
extern "C" int tcsetattr(int fd, int action, const termios *settings)
#ifdef __linux__
    noexcept
#endif
{
  using Set = int (*)(int, int, const termios *);
  static const auto original = reinterpret_cast<Set>(codet::test::native_symbol("tcsetattr"));
  if (codet::test::native_terminal_metrics.observe) {
    ++codet::test::native_terminal_metrics.settings;
    if (::isatty(fd))
      ++codet::test::native_terminal_metrics.live_settings;
  }
  return original(fd, action, settings);
}
extern "C" int dup2(int from, int to)
#ifdef __linux__
    noexcept
#endif
{
  using Duplicate = int (*)(int, int);
  static const auto original = reinterpret_cast<Duplicate>(codet::test::native_symbol("dup2"));
  if (codet::test::native_terminal_metrics.observe &&
      codet::test::native_terminal_metrics.detach_failure) {
    errno = EBADF;
    return -1;
  }
  return original(from, to);
}
extern "C" pid_t tcgetpgrp(int fd)
#ifdef __linux__
    noexcept
#endif
{
  using Foreground = pid_t (*)(int);
  static const auto original =
      reinterpret_cast<Foreground>(codet::test::native_symbol("tcgetpgrp"));
  if (codet::test::native_terminal_metrics.observe &&
      codet::test::native_terminal_metrics.query_failure) {
    errno = EIO;
    return -1;
  }
  return original(fd);
}

extern "C" FILE *fdopen(int descriptor, const char *mode) {
  using Open = FILE *(*)(int, const char *);
  static const auto original = reinterpret_cast<Open>(codet::test::native_symbol("fdopen"));
  auto *stream = original(descriptor, mode);
  auto &metrics = codet::test::native_terminal_metrics;
  if (stream && metrics.observe_streams && metrics.stream_count < 3)
    metrics.streams[metrics.stream_count++] = stream;
  return stream;
}
