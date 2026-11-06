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
  struct PendingSubmission {
    InputLine line;
    std::string tail, view, label;
    RequestKind kind = RequestKind::message;
    bool omitted = false;
    bool tail_attempted = false, redraw = false;
    uint64_t boundary = 0;
    size_t columns = 0;
    bool failed = false;
    PresentationToken receipt{};
  };
  std::optional<PendingSubmission> pending_;
  std::optional<InputOrigin> origin_;
  StreamDestination binding_;
  bool binding_initialized_ = false;
  BoundaryAccess boundaries_;
  bool inspection_due_ = false;
  bool boundary_busy_ = false;
  size_t columns_ = 0, painted_ = 0;
  const size_t test_columns_;
  uint64_t revision_ = 0, request_revision_ = 0;
  uint64_t painted_revision_ = 0, boundary_generation_ = 0;
  bool append_valid_ = false;
  PresentationToken boundary_token_;
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
    if (binding_initialized_ && binding_.descriptor == next.descriptor &&
        binding_.buffer == next.buffer)
      return;
    binding_ = next;
    binding_initialized_ = true;
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
    boundary_token_ = state.token;
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
  void finish_line() noexcept {
    dirty_ = visible_ = explained_ = append_valid_ = false;
    painted_ = previous_bytes_ = committed_ = 0;
    painted_revision_ = 0;
    guidance_due_ = due_ = true;
  }
  DiagnosticResult separate(const PresentationToken *expected = nullptr) {
    if (dirty_) {
      Internal guard(internal_);
      const auto result = output_.present_receipt({}, true, true, expected).result;
      if (result == DiagnosticResult::deferred)
        return DiagnosticResult::deferred;
      if (result == DiagnosticResult::success)
        (void)boundaries().separate(terminal_);
      dirty_ = false;
      if (result == DiagnosticResult::failure) {
        visible_ = append_valid_ = false;
        painted_ = previous_bytes_ = 0;
        return DiagnosticResult::failure;
      }
    }
    visible_ = false;
    painted_ = 0;
    previous_bytes_ = 0;
    return DiagnosticResult::success;
  }
  bool separate_for_flush() {
    const auto result = separate();
    if (result == DiagnosticResult::success)
      return true;
    due_ = result == DiagnosticResult::deferred;
    return false;
  }
  void abandon_geometry() noexcept {
    boundary_busy_ = false;
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
  DiagnosticResult geometry() {
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
      const auto result = separate();
      if (result != DiagnosticResult::success) {
        due_ = result == DiagnosticResult::deferred;
        return result;
      }
      columns_ = columns;
      due_ = true;
    }
    return DiagnosticResult::success;
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
  std::string_view frozen_view(const PendingSubmission &pending) {
    const size_t width = columns_ ? columns_ : 80;
    auto label = std::string_view(pending.label);
    if (!pending.line.discarded && label.size() + 8 > width)
      label = pending.kind == RequestKind::message    ? "in> "
              : pending.kind == RequestKind::approval ? "a> "
                                                      : "q> ";
    size_t bytes = 0;
    const auto append = [&](std::string_view text) {
      std::memcpy(row_.data() + bytes, text.data(), text.size());
      bytes += text.size();
    };
    append(label);
    if (!label.empty() && label.back() != ' ')
      append(" ");
    size_t available = width - bytes;
    size_t used = CursesText::width(pending.view), offset = 0;
    const bool omitted = pending.omitted || used > available;
    if (omitted && available)
      --available;
    while (offset < pending.view.size() && used > available) {
      used -= CursesText::token_columns(pending.view, offset);
      offset += rendered_token(pending.view, offset).bytes;
    }
    if (omitted)
      append("<");
    append(std::string_view(pending.view).substr(offset));
    return {row_.data(), bytes};
  }
  DiagnosticResult paint() {
    const bool keyboard = echo_update_ && (visible_ || dirty_);
    const auto present = [&](std::initializer_list<std::string_view> parts, bool newline) {
      return output_.present_result(parts, newline, keyboard);
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
          const auto result = present({text}, false);
          if (result == DiagnosticResult::deferred)
            return result;
          visible_ = result == DiagnosticResult::success;
        }
        if (visible_) {
          committed_ = end;
          painted_revision_ = input_.revision();
        } else
          append_valid_ = false;
        return visible_ ? DiagnosticResult::success : DiagnosticResult::failure;
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
        return DiagnosticResult::success;
      }
      const auto result =
          append
              ? present({std::string_view(row_.data() + previous_bytes_, bytes - previous_bytes_)},
                        false)
              : present({was_dirty ? "\r" : "", std::string_view(spaces_.data(), painted_),
                         painted_ ? "\r" : "", std::string_view(row_.data(), bytes)},
                        false);
      if (result == DiagnosticResult::deferred)
        return result;
      visible_ = result == DiagnosticResult::success;
      if (visible_) {
        painted_ = used;
        previous_bytes_ = bytes;
        std::copy_n(row_.data(), bytes, previous_row_.data());
      } else {
        previous_bytes_ = 0;
        painted_ = std::max(painted_, used);
      }
    } else {
      if (was_dirty) {
        const auto result = separate();
        if (result != DiagnosticResult::success)
          return result;
      }
      dirty_ = true;
      boundaries().open(terminal_);
      const auto result = present({std::string_view(row_.data(), bytes)}, false);
      if (result == DiagnosticResult::deferred)
        return result;
      visible_ = result == DiagnosticResult::success;
      if (visible_)
        committed_ = input_.input().committed_end();
      painted_ = 0;
    }
    append_valid_ = visible_;
    if (visible_)
      painted_revision_ = input_.revision();
    return visible_ ? DiagnosticResult::success : DiagnosticResult::failure;
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
  bool pending_presentation() const noexcept {
    return due_ || dirty_;
  }
  bool presenting() const noexcept override {
    return internal_;
  }
  bool before_output(int descriptor, const struct stat &value) override {
    if (internal_ || !output_.has_terminal_association())
      return false;
    if (binding_.descriptor < 0) {
      if (descriptor == STDOUT_FILENO)
        destination(value);
      boundaries().bind(descriptor, value);
    }
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
    if (pending_)
      return;
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
    output_.begin_presentation();
    if (geometry() != DiagnosticResult::success || !due_)
      return;
    Internal guard(internal_);
    if (!eligible()) {
      if (owned_ && dirty_ && !columns_) {
        if (!separate_for_flush())
          return;
      } else if (owned_ && dirty_) {
        const auto parts = {std::string_view("\r"), std::string_view(spaces_.data(), painted_),
                            std::string_view("\r")};
        const auto result = output_.present_result(parts, false, echo_update_);
        if (result == DiagnosticResult::deferred)
          return;
        dirty_ = visible_ = false;
        painted_ = previous_bytes_ = 0;
        if (result == DiagnosticResult::success)
          (void)boundaries().separate(terminal_);
      }
      due_ = false;
      return;
    }
    if (input_.stale() && !explained_) {
      if (!separate_for_flush())
        return;
      dirty_ = true;
      boundaries().open(terminal_);
      const auto result = output_.present_result({RequestInput::discard_instruction}, true);
      if (result == DiagnosticResult::deferred)
        return;
      explained_ = result == DiagnosticResult::success;
      if (explained_) {
        dirty_ = false;
        (void)boundaries().separate(terminal_);
      }
    }
    if (!input_.stale() && guidance_due_ && input_.kind() != RequestKind::message &&
        prefix(columns_ ? columns_ : 80) != input_.guidance()) {
      if (!separate_for_flush())
        return;
      dirty_ = true;
      boundaries().open(terminal_);
      const auto result = output_.present_result({input_.guidance()}, true);
      if (result == DiagnosticResult::deferred)
        return;
      guidance_due_ = result != DiagnosticResult::success;
      if (!guidance_due_) {
        dirty_ = false;
        (void)boundaries().separate(terminal_);
      }
    }
    due_ = paint() == DiagnosticResult::deferred; // Deferral is not a failed delivery attempt.
    if (binding_.descriptor >= 0) {
      const auto state = boundaries().snapshot(STDOUT_FILENO, terminal_);
      if (!state.available || state.generation != boundary_generation_)
        visible_ = append_valid_ = false; // The next flush observes the intervening boundary.
    }
  }
  void append(char byte) {
    if (pending_ || !input_.append(byte))
      return;
    Internal echo(echo_update_);
    due_ = true;
    flush();
  }
  void erase_character() {
    if (pending_ || input_.input().empty())
      return;
    input_.erase_character();
    Internal echo(echo_update_);
    due_ = true;
    flush();
  }
  DiagnosticResult submitted() {
    Internal guard(internal_);
    resolve_presentation();
    observe_boundary();
    if (boundary_busy_)
      return DiagnosticResult::deferred;
    const auto geometry_result = geometry();
    if (geometry_result == DiagnosticResult::deferred)
      return geometry_result;
    if (geometry_result == DiagnosticResult::failure) {
      if (pending_)
        pending_->tail_attempted = true;
      finish_line();
      return geometry_result;
    }
    if (pending_ && pending_->tail_attempted && !pending_->failed &&
        (!visible_ || pending_->boundary != boundary_generation_ ||
         pending_->columns != columns_)) {
      pending_->tail_attempted = false;
      pending_->redraw = true;
    }
    if (!pending_) {
      if (owned_ && !columns_ && !input_.stale() && visible_ &&
          committed_ < input_.input().prepared().size()) {
        const auto result =
            output_.present_result({input_.input().prepared().substr(committed_)}, false, true);
        if (result == DiagnosticResult::deferred)
          return result;
      }
    } else if (!pending_->tail_attempted) {
      auto &submission = *pending_;
      // The mutable controller is already empty. Only the frozen view can describe this line.
      // A record completion or geometry change invalidates even an otherwise empty tail.
      const bool append = visible_ && !submission.redraw &&
                          submission.boundary == boundary_generation_ &&
                          submission.columns == columns_;
      const bool display =
          !submission.line.text.empty() || submission.line.request || submission.line.discarded;
      const auto text = !display ? std::string_view{}
                        : append ? std::string_view(submission.tail)
                                 : frozen_view(submission);
      // Empty lines have no paint that could refresh a receipt after rebinding.
      // Their boundary is admitted against the current observed destination instead.
      if (!display)
        submission.receipt = boundary_token_;
      if (!text.empty()) {
        if (!append && !columns_ && dirty_) {
          const auto separation = separate();
          if (separation != DiagnosticResult::success) {
            if (separation == DiagnosticResult::failure) {
              submission.failed = submission.tail_attempted = true;
              finish_line();
            }
            return separation;
          }
        }
        const auto receipt =
            !append && columns_ && dirty_ && visible_
                ? output_.present_receipt({"\r", std::string_view(spaces_.data(), painted_),
                                           painted_ ? "\r" : "", text},
                                          false, true)
                : output_.present_receipt({text}, false, true);
        if (receipt.result == DiagnosticResult::deferred)
          return receipt.result;
        submission.receipt = receipt.token;
        submission.failed = receipt.result == DiagnosticResult::failure;
        visible_ = receipt.result == DiagnosticResult::success;
        dirty_ = true;
        boundaries().open(terminal_);
      }
      submission.boundary =
          submission.receipt.valid ? submission.receipt.generation : boundary_generation_;
      submission.columns = columns_;
      submission.tail_attempted = true;
    }
    const auto *expected =
        pending_ && pending_->receipt.valid && !pending_->failed ? &pending_->receipt : nullptr;
    const auto result = separate(expected);
    if (result == DiagnosticResult::deferred)
      return result;
    // A successful final newline completes this line. Later output belongs to a later record.
    finish_line();
    return result;
  }
  void origin(std::string session, uint64_t turn) {
    origin_ = InputOrigin{std::move(session), turn};
  }
  std::optional<InputLine> discard_pending() noexcept {
    if (!pending_)
      return {};
    auto line = std::move(pending_->line);
    pending_.reset();
    finish_line();
    return line;
  }
  bool submission_pending() const noexcept {
    return pending_.has_value();
  }
  std::optional<InputLine> try_take() {
    if (!pending_) {
      std::string tail;
      bool redraw = !visible_ || painted_revision_ != input_.revision();
      if (owned_ && !columns_ && !input_.stale() && committed_ < input_.input().prepared().size()) {
        const auto remaining = input_.input().prepared().size() - committed_;
        redraw = redraw ||
                 remaining > 12; // At most three held UTF-8 bytes; otherwise use the bounded view.
        if (!redraw)
          tail = input_.input().prepared().substr(committed_);
      }
      const auto suffix = input_.input().tail(256);
      auto view = input_.stale() ? std::string{} : std::string(suffix.text);
      auto label = std::string(prefix(256));
      const auto kind = input_.kind();
      const bool omitted = !input_.stale() && suffix.omitted;
      auto origin = origin_; // Allocate before consuming the controller.
      pending_.emplace(PendingSubmission{input_.take(), std::move(tail), std::move(view),
                                         std::move(label), kind, omitted, false, redraw,
                                         boundary_generation_, columns_});
      pending_->line.origin = std::move(origin);
      pending_->receipt = boundary_token_;
    }
    const auto submission_result = submitted();
    if (!pending_)
      throw Error("Pending input submission lost its owner");
    if (submission_result == DiagnosticResult::deferred) {
      pending_->line.presentation_held = true;
      return {};
    }
    auto line = std::move(pending_->line);
    pending_.reset();
    finish_line();
    return line;
  }
  InputLine take() {
    auto line = try_take();
    if (!line)
      throw Error("Input presentation is pending");
    return std::move(*line);
  }
};
} // namespace codet::detail
