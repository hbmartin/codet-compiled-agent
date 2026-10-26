#pragma once
#include "codet/detail/native_output.hpp"
#include "codet/detail/terminal_resources.hpp"
#include <memory>
#ifdef __linux__
#include <stdio_ext.h>
#endif
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
  struct Bundle {
    SCREEN *screen = nullptr;
    EditLine *editor = nullptr;
    History *history = nullptr;
    FILE *streams[3]{};
    char output_buffer[4096]{};
    int sink = -1;
    bool quarantined = false;
  };
  // Deliberately no destructor registration: native libraries may retain pointers into this bundle.
  inline static Bundle *quarantine_ = nullptr;
  std::unique_ptr<Bundle> held_;
  bool cleanup_ = false, detached_ = false;
  int input() const noexcept {
    return held_ && held_->streams[0] ? ::fileno(held_->streams[0]) : STDIN_FILENO;
  }
  Bundle &bundle() {
    if (cleanup_ || quarantine_)
      throw Error("Native terminal resources are closed or quarantined");
    if (!held_)
      held_ = std::make_unique<Bundle>();
    return *held_;
  }
  static int duplicate(int source) noexcept {
    const int descriptor = ::fcntl(source, F_DUPFD, 3);
    if (descriptor >= 0 && ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
      ::close(descriptor);
      return -1;
    }
    return descriptor;
  }
  void streams(bool error_stream) {
    auto &owned = bundle();
    if (owned.sink < 0) {
      const int opened = ::open("/dev/null", O_RDWR);
      if (opened < 0)
        throw Error("Cannot reserve terminal cleanup sink");
      owned.sink = duplicate(opened);
      ::close(opened);
      if (owned.sink < 0)
        throw Error("Cannot reserve terminal cleanup sink");
    }
    for (int i = 0; i != (error_stream ? 3 : 2); ++i) {
      if (owned.streams[i])
        continue;
      int descriptor = duplicate(i);
      if (descriptor < 0 && i == 2 && errno == EBADF)
        descriptor = duplicate(owned.sink);
      if (descriptor < 0)
        throw Error("Cannot duplicate terminal stream");
      owned.streams[i] = ::fdopen(descriptor, i == 0 ? "r" : "w");
      if (!owned.streams[i]) {
        ::close(descriptor);
        throw Error("Cannot open private terminal stream");
      }
      if (std::setvbuf(owned.streams[i], i == 1 ? owned.output_buffer : nullptr,
                       i == 1 ? _IOFBF : _IONBF, i == 1 ? sizeof(owned.output_buffer) : 0) != 0)
        throw Error("Cannot prepare private terminal stream");
    }
  }
  static void purge(FILE *stream) noexcept {
#ifdef __linux__
    ::__fpurge(stream);
#else
    (void)::fpurge(stream);
#endif
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
    if (held_) {
      stop_screen();
      if (held_->editor)
        stop_editor(held_->editor);
      if (held_->history)
        stop_history(held_->history);
    }
    end_cleanup();
  }
  bool quarantined() const noexcept {
    return held_ ? held_->quarantined : quarantine_ != nullptr;
  }
  void begin_cleanup() noexcept override {
    if (std::exchange(cleanup_, true))
      return;
    detached_ =
        foreign(input()) || (held_ && held_->streams[1] && foreign(::fileno(held_->streams[1])));
    if (!detached_ || !held_)
      return;
    bool failed = false;
    for (auto *stream : held_->streams) {
      if (!stream)
        continue;
      int result;
      do {
        result = ::dup2(held_->sink, ::fileno(stream));
      } while (result < 0 && errno == EINTR);
      failed |= result < 0;
    }
    if (failed) {
      for (auto *stream : held_->streams)
        if (stream)
          purge(stream);
      held_->quarantined = true;
      // Do not write a warning into another foreground program's terminal.
      if (!::isatty(STDERR_FILENO)) {
        DescriptorOutput warning(STDERR_FILENO);
        (void)warning.result({"codet: terminal cleanup quarantined after failed detachment"}, true,
                             OutputMode::immediate);
      }
    }
  }
  void end_cleanup() noexcept override {
    if (!held_)
      return;
    if (held_->quarantined) {
      quarantine_ = held_.release();
      return;
    }
    for (auto &stream : held_->streams)
      if (auto *owned = std::exchange(stream, nullptr))
        (void)std::fclose(owned);
    if (held_->sink >= 0)
      ::close(std::exchange(held_->sink, -1));
    held_.reset();
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
    if (held_ && held_->screen)
      throw Error("Native terminal screen already initialized");
    streams(false);
    held_->screen = ::newterm(nullptr, held_->streams[1], held_->streams[0]);
    if (!held_->screen)
      return false;
    (void)::set_term(held_->screen);
    return true;
  }
  void stop_screen() noexcept override {
    begin_cleanup();
    if (held_ && !held_->quarantined)
      if (auto *held = std::exchange(held_->screen, nullptr)) {
        TerminalCleanupSignals signals;
        (void)::set_term(held);
        (void)::endwin();
        ::delscreen(held);
      }
  }
  EditLine *start_editor() override {
    streams(true);
    if (held_->editor)
      throw Error("Native terminal editor already initialized");
    return held_->editor =
               ::el_init("codet", held_->streams[0], held_->streams[1], held_->streams[2]);
  }
  void stop_editor(EditLine *editor) noexcept override {
    begin_cleanup();
    if (held_ && !held_->quarantined) {
      TerminalCleanupSignals signals;
      ::el_end(editor);
      held_->editor = nullptr;
    }
  }
  History *start_history() override {
    auto &owned = bundle();
    if (owned.history)
      throw Error("Native terminal history already initialized");
    return owned.history = ::history_init();
  }
  void stop_history(History *entries) noexcept override {
    if (held_ && !held_->quarantined) {
      ::history_end(entries);
      held_->history = nullptr;
    }
  }
};
} // namespace codet::detail
