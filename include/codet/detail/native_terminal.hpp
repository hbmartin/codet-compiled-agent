#pragma once
#include "codet/detail/terminal_resources.hpp"
#include <cerrno>
#include <cstdio>
#include <curses.h>
#include <fcntl.h>
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
  FILE *streams_[3]{};
  int sink_ = -1;
  bool cleanup_ = false, detached_ = false;
  int input() const noexcept {
    return streams_[0] ? ::fileno(streams_[0]) : STDIN_FILENO;
  }
  void streams() {
    if (cleanup_)
      throw Error("Native terminal resources are closed");
    if (streams_[0] && streams_[1] && streams_[2])
      return;
    if (sink_ >= 0)
      throw Error("Private terminal stream initialization failed");
    sink_ = ::open("/dev/null", O_RDWR);
    if (sink_ < 0 || ::fcntl(sink_, F_SETFD, FD_CLOEXEC) < 0)
      throw Error("Cannot reserve terminal cleanup sink");
    for (int i = 0; i != 3; ++i) {
      const auto descriptor = ::dup(i);
      if (descriptor < 0)
        throw Error("Cannot duplicate terminal stream");
      if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0 ||
          !(streams_[i] = ::fdopen(descriptor, i == 0 ? "r" : "w"))) {
        ::close(descriptor);
        throw Error("Cannot open private terminal stream");
      }
      if (std::setvbuf(streams_[i], nullptr, _IONBF, 0) != 0)
        throw Error("Cannot prepare private terminal stream");
    }
  }
  static bool foreign(int descriptor) noexcept {
    pid_t group;
    do {
      group = ::tcgetpgrp(descriptor);
    } while (group < 0 && errno == EINTR);
    // Detached PTYs have no job control; preserve their existing restoration behavior.
    return group < 0 ? errno != ENOTTY : group > 0 && group != ::getpgrp();
  }

public:
  NativeTerminalResourceOps() = default;
  NativeTerminalResourceOps(const NativeTerminalResourceOps &) = delete;
  NativeTerminalResourceOps &operator=(const NativeTerminalResourceOps &) = delete;
  ~NativeTerminalResourceOps() override {
    end_cleanup();
  }
  void begin_cleanup() noexcept override {
    if (std::exchange(cleanup_, true))
      return;
    detached_ = foreign(input()) || (streams_[1] && foreign(::fileno(streams_[1])));
    if (!detached_)
      return;
    // Only private descriptors are detached. Native destructors can still free every handle.
    for (auto *stream : streams_) {
      if (!stream)
        continue;
      const auto descriptor = ::fileno(stream);
      int result;
      do {
        result = ::dup2(sink_, descriptor);
      } while (result < 0 && errno == EINTR);
      if (result < 0)
        ::close(descriptor); // A failed detach must never fall back to the live terminal.
    }
  }
  void end_cleanup() noexcept override {
    for (auto &stream : streams_)
      if (auto *held = std::exchange(stream, nullptr))
        (void)std::fclose(held);
    if (sink_ >= 0)
      ::close(std::exchange(sink_, -1));
  }
  bool capture(termios &settings) noexcept override {
    return ::tcgetattr(input(), &settings) == 0;
  }
  bool apply(const termios &settings) noexcept override {
    int status;
    do {
      status = ::tcsetattr(input(), TCSANOW, &settings);
    } while (status < 0 && errno == EINTR);
    return status == 0;
  }
  void restore(const termios &settings) noexcept override {
    begin_cleanup();
    if (detached_)
      return;
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
    streams();
    screen_ = ::newterm(nullptr, streams_[1], streams_[0]);
    if (!screen_)
      return false;
    (void)::set_term(screen_);
    return true;
  }
  void stop_screen() noexcept override {
    begin_cleanup();
    if (auto *held = std::exchange(screen_, nullptr)) {
      TerminalCleanupSignals signals;
      (void)::set_term(held);
      (void)::endwin();
      ::delscreen(held);
    }
  }
  EditLine *start_editor() override {
    streams();
    return ::el_init("codet", streams_[0], streams_[1], streams_[2]);
  }
  void stop_editor(EditLine *editor) noexcept override {
    begin_cleanup();
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
