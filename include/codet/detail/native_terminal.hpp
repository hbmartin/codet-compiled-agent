#pragma once
#include "codet/detail/terminal_resources.hpp"
#include <cerrno>
#include <cstdio>
#include <curses.h>
#include <histedit.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

namespace codet::detail {
class TerminalCleanupSignals {
  sigset_t previous_{};
  bool blocked_;

public:
  TerminalCleanupSignals() noexcept {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTTOU);
    blocked_ = ::pthread_sigmask(SIG_BLOCK, &mask, &previous_) == 0;
  }
  TerminalCleanupSignals(const TerminalCleanupSignals &) = delete;
  TerminalCleanupSignals &operator=(const TerminalCleanupSignals &) = delete;
  ~TerminalCleanupSignals() {
    if (blocked_)
      (void)::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
  }
  bool ready() const noexcept {
    return blocked_;
  }
};
inline bool same_terminal_settings(const termios &left, const termios &right) noexcept {
  if (left.c_iflag != right.c_iflag || left.c_oflag != right.c_oflag ||
      left.c_cflag != right.c_cflag || left.c_lflag != right.c_lflag ||
      ::cfgetispeed(&left) != ::cfgetispeed(&right) ||
      ::cfgetospeed(&left) != ::cfgetospeed(&right))
    return false;
#ifdef __linux__
  if (left.c_line != right.c_line)
    return false;
#endif
  for (size_t i = 0; i < NCCS; ++i)
    if (left.c_cc[i] != right.c_cc[i])
      return false;
  return true;
}
// Include only at native terminal call sites: curses intentionally exposes macros here.
class NativeTerminalResourceOps final : public TerminalResourceOps {
  SCREEN *screen_ = nullptr;

public:
  bool capture(termios &settings) noexcept override {
    return ::tcgetattr(STDIN_FILENO, &settings) == 0;
  }
  bool apply(const termios &settings) noexcept override {
    int status;
    do {
      status = ::tcsetattr(STDIN_FILENO, TCSANOW, &settings);
    } while (status < 0 && errno == EINTR);
    return status == 0;
  }
  void restore(const termios &settings) noexcept override {
    TerminalCleanupSignals signals;
    restore_active(settings);
  }
  void restore_active(const termios &settings) noexcept override {
    termios current{};
    if (capture(current) && same_terminal_settings(current, settings))
      return;
    (void)apply(settings);
  }
  bool start_screen() override {
    if (screen_)
      throw Error("Native terminal screen already initialized");
    screen_ = ::newterm(nullptr, stdout, stdin);
    if (!screen_)
      return false;
    (void)::set_term(screen_);
    return true;
  }
  void stop_screen() noexcept override {
    if (auto *held = std::exchange(screen_, nullptr)) {
      TerminalCleanupSignals signals;
      (void)::set_term(held);
      (void)::endwin();
      ::delscreen(held);
    }
  }
  EditLine *start_editor() override {
    return ::el_init("codet", stdin, stdout, stderr);
  }
  void stop_editor(EditLine *editor) noexcept override {
    TerminalCleanupSignals signals;
    ::el_end(editor);
  }
  History *start_history() override {
    return ::history_init();
  }
  void stop_history(History *entries) noexcept override {
    ::history_end(entries);
  }
};
} // namespace codet::detail
