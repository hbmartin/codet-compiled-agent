#pragma once
#include "codet/detail/cli_output.hpp"
#include "codet/detail/request_input_controller.hpp"

namespace codet::detail {
// Main-thread presentation owner. Input intent remains in RequestInputController.
class LinePrompt final : public LineOutputObserver {
  RequestInputController &input_;
  CliOutput &output_;
  IoContext &io_;
  const std::string &prompt_;
  struct stat terminal_ {};
  LineOutputObserver *previous_;
  bool owned_ = false, dirty_ = false, visible_ = false, due_ = true;
  bool internal_ = false, explained_ = false;
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
  }

public:
  LinePrompt(RequestInputController &input, CliOutput &output, IoContext &io,
             const std::string &prompt, const struct stat &terminal) noexcept
      : input_(input), output_(output), io_(io), prompt_(prompt), terminal_(terminal),
        previous_(io.line_output) {
    io_.line_output = this;
  }
  ~LinePrompt() override {
    io_.line_output = previous_;
  }
  LinePrompt(const LinePrompt &) = delete;
  LinePrompt &operator=(const LinePrompt &) = delete;
  void before_output(const struct stat &destination) override {
    if (internal_ || !S_ISCHR(destination.st_mode) || destination.st_dev != terminal_.st_dev ||
        destination.st_rdev != terminal_.st_rdev)
      return;
    separate();
    due_ = true;
  }
  void acquire() noexcept {
    owned_ = true;
    due_ = true;
  }
  void editor() noexcept {
    owned_ = dirty_ = visible_ = false;
    due_ = true;
  }
  void release() noexcept {
    owned_ = false;
  }
  void request_changed() noexcept {
    visible_ = false;
    due_ = true;
    if (!input_.stale())
      explained_ = false;
  }
  void flush() {
    if (!due_ || !eligible())
      return;
    separate();
    Internal guard(internal_);
    if (input_.stale() && !explained_)
      (void)output_.display({RequestInput::discard_instruction});
    dirty_ = true; // A failed write may have emitted a prefix.
    if (input_.stale()) {
      visible_ = output_.display({RequestInput::discard_prompt}, false);
      explained_ |= visible_;
    } else {
      const auto prefix =
          input_.kind() == RequestKind::message ? std::string_view(prompt_) : input_.guidance();
      visible_ = input_.kind() == RequestKind::message
                     ? output_.display({prefix, input_.input().raw()}, false)
                     : output_.display({prefix, " ", input_.input().raw()}, false);
    }
    due_ = false; // Retry only after another edit, request change or output event.
  }
  void append(char byte) {
    input_.append(byte);
    if (input_.stale())
      return;
    if (!visible_) {
      due_ = true;
      flush();
    } else {
      Internal guard(internal_);
      dirty_ = true;
      output_.echo({std::string_view(&byte, 1)});
    }
  }
  void erase_character() {
    if (input_.input().empty())
      return;
    input_.erase_character();
    if (!input_.stale() && visible_) {
      Internal guard(internal_);
      output_.echo({"\b \b"});
    } else if (!input_.stale()) {
      due_ = true;
      flush();
    }
  }
  void submitted() {
    Internal guard(internal_);
    output_.echo({"\n"});
    dirty_ = visible_ = explained_ = false;
    due_ = true;
  }
};
} // namespace codet::detail
