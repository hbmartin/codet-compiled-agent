// Host-only observations; native calls still execute against a real PTY.
#include "native_terminal_metrics.hpp"
#include <curses.h>
#include <dlfcn.h>
#include <termios.h>
namespace codet::test {
NativeTerminalMetrics native_terminal_metrics;
}
extern "C" int endwin() {
  using End = int (*)();
  static const auto original = reinterpret_cast<End>(::dlsym(RTLD_NEXT, "endwin"));
  const auto status = original();
  if (codet::test::native_terminal_metrics.observe)
    ++codet::test::native_terminal_metrics.ends;
  return status;
}
extern "C" void delscreen(SCREEN *screen) {
  using Delete = void (*)(SCREEN *);
  static const auto original = reinterpret_cast<Delete>(::dlsym(RTLD_NEXT, "delscreen"));
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
  static const auto original = reinterpret_cast<Set>(::dlsym(RTLD_NEXT, "tcsetattr"));
  if (codet::test::native_terminal_metrics.observe)
    ++codet::test::native_terminal_metrics.settings;
  return original(fd, action, settings);
}
