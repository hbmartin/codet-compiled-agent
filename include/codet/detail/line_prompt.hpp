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
  LineOutputObserver *previous_;
  bool owned_ = false, dirty_ = false, visible_ = false, due_ = true;
  bool internal_ = false, explained_ = false;
  bool echo_update_ = false;
  StreamDestination binding_;
  BoundaryAccess boundaries_;
  bool inspection_due_ = false;
  bool boundary_busy_ = false;
  size_t columns_ = 0, painted_ = 0;
  const size_t test_columns_;
  uint64_t revision_ = 0, request_revision_ = 0;
  uint64_t painted_revision_ = 0, boundary_generation_ = 0;
  bool append_valid_ = false;
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
  BoundaryAccess boundaries() const noexcept {
    return boundaries_;
  }
  void bind_errors() noexcept {
    struct stat errors {};
    OutputAttempt attempt(io_.cancel, OutputMode::diagnostic);
    if (binding_.descriptor >= 0 && inspect_output_destination(STDERR_FILENO, errors, attempt))
      boundaries().bind(STDERR_FILENO, errors);
  }
  void resolve_presentation() noexcept {
    const auto next = output_.presentation_destination();
    if (binding_.descriptor == next.descriptor && binding_.buffer == next.buffer)
      return;
    binding_ = next;
    if (binding_.descriptor < 0)
      io_.line_destinations.reset();
    boundaries_ = BoundaryAccess(binding_.descriptor >= 0 ? nullptr : &io_.line_destinations);
    abandon_geometry();
    boundaries().bind(STDOUT_FILENO, terminal_);
    bind_errors();
    dirty_ = boundaries().row(terminal_);
    explained_ = false;
    guidance_due_ = due_ = inspection_due_ = true;
  }
  void observe_boundary() noexcept {
    if (binding_.descriptor < 0)
      return;
    const auto state = boundaries().snapshot(STDOUT_FILENO, terminal_);
    if (!state.available) {
      abandon_geometry();
      return;
    }
    boundary_busy_ = state.active != 0;
    if (boundary_generation_ != state.generation) {
      visible_ = append_valid_ = false;
      dirty_ = state.row;
      painted_ = previous_bytes_ = 0;
      due_ = true;
      boundary_generation_ = state.generation;
    }
  }
  bool eligible() const noexcept {
    return owned_ &&
           (input_.stale() || input_.kind() != RequestKind::message || !input_.input().empty());
  }
  void separate() {
    if (dirty_) {
      Internal guard(internal_);
      if (output_.present({"\n"}, false, true))
        (void)boundaries().separate(terminal_);
      dirty_ = false;
    }
    visible_ = false;
    painted_ = 0;
    previous_bytes_ = 0;
  }
  void abandon_geometry() noexcept {
    append_valid_ = false;
    dirty_ = visible_ = false;
    painted_ = columns_ = 0;
    previous_bytes_ = 0;
  }
  void destination(const struct stat &value) noexcept {
    if (!same_destination(terminal_, value)) {
      bind_errors();
      boundaries().bind(STDOUT_FILENO, value);
      abandon_geometry();
      terminal_ = value;
      dirty_ = boundaries().row(value);
      explained_ = false;
      guidance_due_ = true;
      due_ = true;
    }
  }
  void geometry() {
    size_t columns = test_columns_;
    const int descriptor = binding_.descriptor;
    if (descriptor >= 0) {
      struct stat value {};
      OutputAttempt attempt(io_.cancel, OutputMode::diagnostic);
      if (inspect_output_destination(descriptor, value, attempt)) {
        destination(value);
        struct winsize size {};
        columns =
            S_ISCHR(value.st_mode) && ::ioctl(descriptor, TIOCGWINSZ, &size) == 0 ? size.ws_col : 0;
      } else {
        abandon_geometry();
        // A failed inspection invalidates geometry, not the last confirmed identity.
        dirty_ = boundaries().row(terminal_);
        columns = 0;
      }
    }
    observe_boundary();
    columns = columns >= 10 ? std::min(columns - 1, size_t{256}) : 0;
    if (columns_ != columns) {
      // Never erase using geometry established before a resize.
      separate();
      columns_ = columns;
      due_ = true;
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
      return output_.present(parts, newline, keyboard);
    };
    const bool compatible = append_valid_ && input_.appended_since(painted_revision_);
    if (!columns_ && compatible && visible_ && !input_.stale() &&
        input_.input().changed_offset() >= committed_) {
      const auto end = input_.input().committed_end();
      if (end >= committed_) {
        const auto text = input_.input().prepared().substr(committed_, end - committed_);
        if (!text.empty()) {
          dirty_ = true;
          boundaries().open(terminal_);
          visible_ = present({text}, false);
        }
        if (visible_) {
          committed_ = end;
          painted_revision_ = input_.revision();
        } else
          append_valid_ = false;
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
    boundaries().open(terminal_);
    if (columns_) {
      const bool append = compatible && visible_ && bytes >= previous_bytes_ &&
                          std::memcmp(row_.data(), previous_row_.data(), previous_bytes_) == 0;
      if (append && bytes == previous_bytes_) {
        painted_revision_ = input_.revision();
        return;
      }
      visible_ =
          append
              ? present({std::string_view(row_.data() + previous_bytes_, bytes - previous_bytes_)},
                        false)
              : present({was_dirty ? "\r" : "", std::string_view(spaces_.data(), painted_),
                         painted_ ? "\r" : "", std::string_view(row_.data(), bytes)},
                        false);
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
      boundaries().open(terminal_);
      visible_ = present({std::string_view(row_.data(), bytes)}, false);
      if (visible_)
        committed_ = input_.input().committed_end();
      painted_ = 0;
    }
    append_valid_ = visible_;
    if (visible_)
      painted_revision_ = input_.revision();
  }

public:
  // A fixed geometry is useful for injected streams; native stdout is always inspected afresh.
  LinePrompt(RequestInputController &input, CliOutput &output, IoContext &io,
             const std::string &prompt, const struct stat &terminal, size_t columns = 0) noexcept
      : input_(input), output_(output), io_(io), prompt_(prompt), terminal_(terminal),
        previous_(io.line_output), test_columns_(columns) {
    spaces_.fill(' ');
    resolve_presentation();
    boundary_generation_ = boundaries().snapshot(STDOUT_FILENO, terminal_).generation;
    io_.line_output = this;
  }
  ~LinePrompt() override {
    io_.line_output = previous_;
  }
  LinePrompt(const LinePrompt &) = delete;
  LinePrompt &operator=(const LinePrompt &) = delete;
  bool presenting() const noexcept override {
    return internal_;
  }
  bool before_output(int descriptor, const struct stat &value) override {
    if (internal_)
      return false;
    if (descriptor == STDOUT_FILENO)
      destination(value);
    boundaries().bind(descriptor, value);
    inspection_due_ = true; // Refresh stdout once at the end of the output batch, not per record.
    const bool separator = binding_.descriptor < 0 && boundaries().separate(value);
    if (S_ISCHR(value.st_mode) && same_destination(value, terminal_)) {
      dirty_ = visible_ = false;
      painted_ = previous_bytes_ = 0;
      due_ = true;
    }
    return separator;
  }
  void acquire() noexcept {
    append_valid_ = false;
    owned_ = true;
    due_ = true;
  }
  void editor() noexcept {
    owned_ = dirty_ = visible_ = false;
    painted_ = 0;
    due_ = true;
  }
  void release() noexcept {
    append_valid_ = false;
    owned_ = false;
  }
  void flush() {
    resolve_presentation();
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
    if (!owned_ || (!eligible() && !dirty_)) {
      due_ = inspection_due_ = false;
      return;
    }
    observe_boundary();
    if (boundary_busy_)
      return;
    if (!due_ && !inspection_due_)
      return;
    inspection_due_ = false;
    geometry();
    if (!due_)
      return;
    Internal guard(internal_);
    output_.begin_presentation();
    if (!eligible()) {
      if (owned_ && dirty_ && !columns_)
        separate();
      else if (owned_ && dirty_) {
        const auto parts = {std::string_view("\r"), std::string_view(spaces_.data(), painted_),
                            std::string_view("\r")};
        if (echo_update_)
          (void)output_.present(parts, false, true);
        else
          (void)output_.present(parts, false);
        dirty_ = visible_ = false;
        painted_ = 0;
        (void)boundaries().separate(terminal_);
      }
      due_ = false;
      return;
    }
    if (input_.stale() && !explained_) {
      separate();
      dirty_ = true;
      boundaries().open(terminal_);
      explained_ = output_.present({RequestInput::discard_instruction}, true);
      if (output_.presentation_deferred())
        return;
      if (explained_) {
        dirty_ = false;
        (void)boundaries().separate(terminal_);
      }
    }
    if (!input_.stale() && guidance_due_ && input_.kind() != RequestKind::message &&
        prefix(columns_ ? columns_ : 80) != input_.guidance()) {
      separate();
      dirty_ = true;
      boundaries().open(terminal_);
      guidance_due_ = !output_.present({input_.guidance()}, true);
      if (output_.presentation_deferred())
        return;
      if (!guidance_due_) {
        dirty_ = false;
        (void)boundaries().separate(terminal_);
      }
    }
    paint();
    due_ = output_.presentation_deferred(); // Deferral is not a failed delivery attempt.
    if (binding_.descriptor >= 0) {
      const auto state = boundaries().snapshot(STDOUT_FILENO, terminal_);
      if (!state.available || state.generation != boundary_generation_)
        visible_ = append_valid_ = false; // The next flush observes the intervening boundary.
    }
  }
  void append(char byte) {
    if (!input_.append(byte))
      return;
    Internal echo(echo_update_);
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
    if (owned_ && !columns_ && !input_.stale() &&
        input_.input().committed_end() < input_.input().prepared().size()) {
      // Typing holds valid prefixes; submission must expose the unresolved bytes.
      flush();
      if (!columns_ && visible_ && committed_ <= input_.input().prepared().size())
        (void)output_.present({input_.input().prepared().substr(committed_)}, false, true);
    }
    if (dirty_)
      if (output_.present({"\n"}, false, true))
        (void)boundaries().separate(terminal_);
    dirty_ = visible_ = explained_ = false;
    append_valid_ = false;
    painted_ = 0;
    due_ = true;
  }
  InputLine take() {
    submitted();
    return input_.take();
  }
};
} // namespace codet::detail
