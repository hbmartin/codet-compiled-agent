#pragma once
#include "codet/core.hpp"
#include <termios.h>
#include <utility>

// libedit's opaque handles; keep native headers and their macros out of this owner.
struct editline;
typedef struct editline EditLine;
struct history;
typedef struct history History;

namespace codet::detail {
class TerminalResourceOps {
public:
  virtual ~TerminalResourceOps() = default;
  virtual bool capture(termios &) noexcept = 0;
  virtual void restore(const termios &) noexcept = 0;
  virtual void restore_active(const termios &settings) noexcept {
    restore(settings);
  }
  virtual bool apply(const termios &) noexcept = 0;
  virtual bool start_screen() = 0;
  virtual void stop_screen() noexcept = 0;
  virtual EditLine *start_editor() = 0;
  virtual void stop_editor(EditLine *) noexcept = 0;
  virtual History *start_history() = 0;
  virtual void stop_history(History *) noexcept = 0;
};
// This member is fully constructed before Terminal acquires native resources or renders.
class TerminalResources {
  TerminalResourceOps &ops_;
  bool screen_ = false, closed_ = false, attempted_ = false, saved_ = false;
  termios original_{};
  EditLine *editor_ = nullptr;
  History *entries_ = nullptr;
  void require_open() const {
    if (closed_)
      throw Error("Terminal resources are closed");
  }
  void prepare_restore() {
    require_open();
    if (!std::exchange(attempted_, true))
      saved_ = ops_.capture(original_);
  }

public:
  explicit TerminalResources(TerminalResourceOps &ops) noexcept : ops_(ops) {}
  TerminalResources(const TerminalResources &) = delete;
  TerminalResources &operator=(const TerminalResources &) = delete;
  void start_screen() {
    require_open();
    if (screen_)
      throw Error("Terminal screen already initialized");
    prepare_restore();
    if (!ops_.start_screen())
      throw Error("Cannot initialize terminal");
    screen_ = true;
  }
  void start_editor() {
    require_open();
    if (editor_)
      throw Error("Terminal editor already initialized");
    prepare_restore();
    editor_ = ops_.start_editor();
    if (!editor_)
      throw Error("Cannot initialize line editor");
  }
  void start_history() {
    require_open();
    if (entries_)
      throw Error("Terminal history already initialized");
    entries_ = ops_.start_history();
  }
  EditLine *editor() const noexcept {
    return editor_;
  }
  History *entries() const noexcept {
    return entries_;
  }
  bool saved() const noexcept {
    return saved_;
  }
  void begin_active() {
    prepare_restore();
    if (!saved_)
      return;
    auto settings = original_;
    settings.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (!ops_.apply(settings))
      throw Error("Cannot prepare active terminal input");
  }
  void end_active() noexcept {
    if (!closed_ && saved_)
      ops_.restore_active(original_);
  }
  void close() noexcept {
    if (std::exchange(closed_, true))
      return;
    if (std::exchange(screen_, false))
      ops_.stop_screen();
    if (auto *held = std::exchange(editor_, nullptr))
      ops_.stop_editor(held);
    if (auto *held = std::exchange(entries_, nullptr))
      ops_.stop_history(held);
    if (std::exchange(saved_, false))
      ops_.restore(original_);
  }
  ~TerminalResources() {
    close();
  }
};
} // namespace codet::detail
