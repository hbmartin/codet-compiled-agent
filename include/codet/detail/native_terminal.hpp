#pragma once
#include "codet/detail/terminal_resources.hpp"
#include <cstdio>
#include <curses.h>
#include <histedit.h>
#include <unistd.h>

namespace codet::detail {
// Include only at native terminal call sites: curses intentionally exposes macros here.
class NativeTerminalResourceOps final : public TerminalResourceOps {
  SCREEN *screen_ = nullptr;

public:
  bool capture(termios &settings) noexcept override {
    return ::tcgetattr(STDIN_FILENO, &settings) == 0;
  }
  bool apply(const termios &settings) noexcept override {
    return ::tcsetattr(STDIN_FILENO, TCSANOW, &settings) == 0;
  }
  void restore(const termios &settings) noexcept override {
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
      (void)::set_term(held);
      (void)::endwin();
      ::delscreen(held);
    }
  }
  EditLine *start_editor() override {
    return ::el_init("codet", stdin, stdout, stderr);
  }
  void stop_editor(EditLine *editor) noexcept override {
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
