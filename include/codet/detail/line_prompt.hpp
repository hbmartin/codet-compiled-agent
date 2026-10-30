#pragma once
#include "codet/detail/cli_output.hpp"
#include "codet/detail/request_input_controller.hpp"
#include <sys/ioctl.h>

namespace codet::detail {
// Main-thread presentation owner. Raw input never crosses this output boundary.
class LinePrompt final : public LineOutputObserver {
  RequestInputController &input_;
  CliOutput &output_;
  IoContext &io_;
  const std::string &prompt_;
  struct stat terminal_ {};
  struct stat abandoned_ {};
  LineOutputObserver *previous_;
  bool owned_ = false, dirty_ = false, visible_ = false, due_ = true;
  bool internal_ = false, explained_ = false, known_ = true;
  bool echo_update_ = false, append_update_ = false;
  bool abandoned_dirty_ = false;
  size_t columns_ = 0, painted_ = 0;
  const size_t test_columns_;
  uint64_t revision_ = 0, request_revision_ = 0;
  bool guidance_due_ = true;
  std::array<char, 1280> row_{};
  std::array<char, 1280> previous_row_{};
  size_t previous_bytes_ = 0, committed_ = 0;
  std::array<char, 256> spaces_{};
  struct Internal {
    bool &flag;
    bool previous;
    explicit Internal(bool &value) noexcept : flag(value), previous(value) {
      flag = true;
    }
    ~Internal() {
      flag = previous;
    }
  };
  static bool same(const struct stat &a, const struct stat &b) noexcept {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
           (a.st_mode & S_IFMT) == (b.st_mode & S_IFMT) && a.st_rdev == b.st_rdev;
  }
  bool eligible() const noexcept {
    return owned_ &&
           (input_.stale() || input_.kind() != RequestKind::message || !input_.input().empty());
  }
  void separate() {
    if (dirty_) {
      Internal guard(internal_);
      output_.echo({"\n"});
      dirty_ = false;
    }
    visible_ = false;
    painted_ = 0;
    previous_bytes_ = 0;
  }
  void abandon_geometry() noexcept {
    if (known_ && dirty_) {
      abandoned_ = terminal_;
      abandoned_dirty_ = true;
    }
    dirty_ = visible_ = false;
    painted_ = columns_ = 0;
  }
  void destination(const struct stat &value) noexcept {
    if (!known_ || !same(terminal_, value)) {
      const bool resumed = abandoned_dirty_ && same(abandoned_, value);
      const bool retained = known_ && dirty_;
      abandon_geometry();
      if (resumed && !retained)
        abandoned_dirty_ = false;
      terminal_ = value;
      known_ = true;
      dirty_ = resumed;
      visible_ = explained_ = false;
      painted_ = columns_ = 0;
      due_ = true;
    }
  }
  void geometry() {
    size_t columns = test_columns_;
    const int descriptor = output_.presentation_descriptor();
    if (descriptor >= 0) {
      struct stat value {};
      if (::fstat(descriptor, &value) == 0) {
        destination(value);
        struct winsize size {};
        columns =
            S_ISCHR(value.st_mode) && ::ioctl(descriptor, TIOCGWINSZ, &size) == 0 ? size.ws_col : 0;
      } else {
        abandon_geometry();
        known_ = false;
        columns = 0;
      }
    }
    columns = columns >= 10 ? std::min(columns - 1, size_t{256}) : 0;
    if (columns_ != columns) {
      // Never erase using geometry established before a resize.
      separate();
      columns_ = columns;
    }
  }
  std::string_view prefix(size_t width) const noexcept {
    if (input_.stale())
      return RequestInput::discard_prompt;
    const auto full =
        input_.kind() == RequestKind::message ? std::string_view(prompt_) : input_.guidance();
    if (full.size() + 8 <= width)
      return full;
    return input_.kind() == RequestKind::message    ? "in> "
           : input_.kind() == RequestKind::approval ? "a> "
                                                    : "q> ";
  }
  void paint() {
    const bool keyboard = echo_update_ && (visible_ || dirty_);
    const auto present = [&](std::initializer_list<std::string_view> parts, bool newline) {
      return keyboard ? output_.echo_attempt(parts, newline) : output_.display(parts, newline);
    };
    if (!columns_ && append_update_ && visible_ && !input_.stale() &&
        input_.input().changed_offset() >= committed_) {
      const auto end = input_.input().committed_end();
      if (end >= committed_) {
        const auto text = input_.input().prepared().substr(committed_, end - committed_);
        if (!text.empty()) {
          dirty_ = true;
          visible_ = present({text}, false);
        }
        if (visible_)
          committed_ = end;
        return;
      }
    }
    const size_t width = columns_ ? columns_ : 80;
    auto label = prefix(width);
    size_t bytes = 0, used = label.size();
    const auto append = [&](std::string_view text) {
      std::memcpy(row_.data() + bytes, text.data(), text.size());
      bytes += text.size();
    };
    append(label);
    if (!label.empty() && label.back() != ' ') {
      append(" ");
      ++used;
    }
    if (!input_.stale()) {
      const auto tail = input_.input().tail(width - used, !columns_);
      if (tail.omitted) {
        append("<");
        ++used;
      }
      append(tail.text);
      used += tail.columns;
    }
    const bool was_dirty = dirty_;
    dirty_ = true; // Even failure may have emitted part of the row.
    if (columns_) {
      const bool append = append_update_ && visible_ && bytes >= previous_bytes_ &&
                          std::memcmp(row_.data(), previous_row_.data(), previous_bytes_) == 0;
      if (append && bytes == previous_bytes_)
        return;
      visible_ = append
                     ? present({std::string_view(row_.data() + previous_bytes_,
                                                 bytes - previous_bytes_)}, false)
                     : present({was_dirty ? "\r" : "", std::string_view(spaces_.data(), painted_),
                                painted_ ? "\r" : "", std::string_view(row_.data(), bytes)}, false);
      if (visible_) {
        painted_ = used;
        previous_bytes_ = bytes;
        std::copy_n(row_.data(), bytes, previous_row_.data());
      } else {
        previous_bytes_ = 0;
        painted_ = std::max(painted_, used);
      }
    } else {
      if (was_dirty)
        separate();
      dirty_ = true;
      visible_ = present({std::string_view(row_.data(), bytes)}, false);
      if (visible_)
        committed_ = input_.input().committed_end();
      painted_ = 0;
    }
  }

public:
  // A fixed geometry is useful for injected streams; native stdout is always inspected afresh.
  LinePrompt(RequestInputController &input, CliOutput &output, IoContext &io,
             const std::string &prompt, const struct stat &terminal, size_t columns = 0) noexcept
      : input_(input), output_(output), io_(io), prompt_(prompt), terminal_(terminal),
        previous_(io.line_output), test_columns_(columns) {
    spaces_.fill(' ');
    io_.line_output = this;
  }
  ~LinePrompt() override {
    io_.line_output = previous_;
  }
  LinePrompt(const LinePrompt &) = delete;
  LinePrompt &operator=(const LinePrompt &) = delete;
  bool before_output(int descriptor, const struct stat &value) override {
    if (internal_)
      return false;
    if (descriptor == STDOUT_FILENO)
      destination(value);
    else if (const auto prompt = output_.presentation_descriptor(); prompt >= 0) {
      struct stat current {};
      if (::fstat(prompt, &current) == 0)
        destination(current);
      else {
        abandon_geometry();
        known_ = false;
      }
    }
    // Retain only a boundary for the last abandoned row, never its cursor geometry.
    // A stderr still targeting that terminal must not invalidate the new stdout row.
    if (abandoned_dirty_ && same(value, abandoned_)) {
      abandoned_dirty_ = false;
      return true;
    }
    if (!known_ || !S_ISCHR(value.st_mode) || !same(value, terminal_))
      return false;
    const bool separator = dirty_;
    dirty_ = visible_ = false;
    painted_ = 0;
    due_ = true;
    return separator;
  }
  void acquire() noexcept {
    owned_ = true;
    due_ = true;
  }
  void editor() noexcept {
    owned_ = dirty_ = visible_ = false;
    painted_ = 0;
    due_ = true;
  }
  void release() noexcept {
    owned_ = false;
  }
  void flush() {
    if (revision_ != input_.revision()) {
      revision_ = input_.revision();
      due_ = true;
      if (!input_.stale())
        explained_ = false;
    }
    if (request_revision_ != input_.request_revision()) {
      request_revision_ = input_.request_revision();
      guidance_due_ = true;
    }
    if (!due_)
      return;
    geometry();
    Internal guard(internal_);
    if (!eligible()) {
      if (owned_ && dirty_ && columns_) {
        const auto parts = {std::string_view("\r"), std::string_view(spaces_.data(), painted_),
                            std::string_view("\r")};
        if (echo_update_)
          output_.echo(parts);
        else
          (void)output_.display(parts, false);
        dirty_ = visible_ = false;
        painted_ = 0;
      }
      due_ = false;
      return;
    }
    if (input_.stale() && !explained_) {
      separate();
      dirty_ = true;
      explained_ = output_.display({RequestInput::discard_instruction});
      if (explained_)
        dirty_ = false;
    }
    if (!input_.stale() && guidance_due_ && input_.kind() != RequestKind::message &&
        prefix(columns_ ? columns_ : 80) != input_.guidance()) {
      separate();
      dirty_ = true;
      guidance_due_ = !output_.display({input_.guidance()});
      if (!guidance_due_)
        dirty_ = false;
    }
    paint();
    due_ = false; // Retry only on a real edit, request change or output event.
  }
  void append(char byte) {
    if (!input_.append(byte))
      return;
    Internal echo(echo_update_);
    Internal append(append_update_);
    due_ = true;
    flush();
  }
  void erase_character() {
    if (input_.input().empty())
      return;
    input_.erase_character();
    Internal echo(echo_update_);
    due_ = true;
    flush();
  }
  void submitted() {
    Internal guard(internal_);
    if (dirty_)
      output_.echo({"\n"});
    dirty_ = visible_ = explained_ = false;
    painted_ = 0;
    due_ = true;
  }
  InputLine take() {
    submitted();
    return input_.take();
  }
};
} // namespace codet::detail
