#pragma once
#include "codet/core.hpp"
#include <cstdio>
#include <curses.h>
#include <histedit.h>
#include <termios.h>
#include <unistd.h>

namespace codet::detail {
class TerminalResourceOps {
public:
  virtual ~TerminalResourceOps() = default;
  virtual bool capture(termios &) noexcept = 0;
  virtual void restore(const termios &) noexcept = 0;
  virtual bool start_screen() = 0;
  virtual void stop_screen() noexcept = 0;
  virtual EditLine *start_editor() = 0;
  virtual void stop_editor(EditLine *) noexcept = 0;
  virtual History *start_history() = 0;
  virtual void stop_history(History *) noexcept = 0;
};
class NativeTerminalResourceOps final : public TerminalResourceOps {
public:
  bool capture(termios &settings) noexcept override {
    return ::tcgetattr(STDIN_FILENO, &settings) == 0;
  }
  void restore(const termios &settings) noexcept override {
    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &settings);
  }
  bool start_screen() override {
    return ::initscr() != nullptr;
  }
  void stop_screen() noexcept override {
    (void)::endwin();
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
// This member is fully constructed before Terminal acquires native resources or renders.
class TerminalResources {
  TerminalResourceOps &ops_;
  bool screen_ = false;

public:
  termios original{};
  bool saved;
  EditLine *editor = nullptr;
  History *entries = nullptr;
  explicit TerminalResources(TerminalResourceOps &ops) noexcept
      : ops_(ops), saved(ops_.capture(original)) {}
  TerminalResources(const TerminalResources &) = delete;
  TerminalResources &operator=(const TerminalResources &) = delete;
  void start_screen() {
    if (!ops_.start_screen())
      throw Error("Cannot initialize terminal");
    screen_ = true;
  }
  void start_editor() {
    editor = ops_.start_editor();
    if (!editor)
      throw Error("Cannot initialize line editor");
  }
  void start_history() {
    entries = ops_.start_history();
  }
  void close() noexcept {
    if (std::exchange(screen_, false))
      ops_.stop_screen();
    if (auto *held = std::exchange(editor, nullptr))
      ops_.stop_editor(held);
    if (auto *held = std::exchange(entries, nullptr))
      ops_.stop_history(held);
    if (std::exchange(saved, false))
      ops_.restore(original);
  }
  ~TerminalResources() {
    close();
  }
};
} // namespace codet::detail
