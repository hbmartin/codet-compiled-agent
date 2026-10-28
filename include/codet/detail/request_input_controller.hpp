#pragma once
#include "codet/detail/curses_input.hpp"
#include "codet/detail/request_input.hpp"

namespace codet::detail {
enum class RequestKind : uint8_t { message, approval, question };
// One owner for raw/presented input, draft intent and the prompt that can accept it.
class RequestInputController {
  RequestInput scope_;
  CursesInput input_;
  RequestKind kind_ = RequestKind::message;
  bool prompt_visible_ = false, prompt_due_ = false, discard_explained_ = false;

public:
  static constexpr std::string_view question_prompt = "Enter a choice number or a text answer.";
  bool stale() const noexcept {
    return scope_.stale();
  }
  bool ordinary_draft() const noexcept {
    return scope_.ordinary_draft();
  }
  RequestKind kind() const noexcept {
    return kind_;
  }
  const CursesInput &input() const noexcept {
    return input_;
  }
  void unicode(bool enabled) {
    input_.unicode(enabled);
  }
  void append(char byte) {
    if (input_.size() == CursesInput::limit)
      return;
    scope_.edit();
    input_ += byte;
  }
  void erase_character() {
    if (input_.empty())
      return;
    input_.erase_character();
    if (input_.empty())
      scope_.edited_empty();
  }
  void presentation_clear() noexcept {
    if (!scope_.stale())
      input_.clear();
  }
  void open(ApprovalIdentity identity, RequestKind kind) {
    const bool was_stale = stale();
    if (scope_.open(std::move(identity)) && !was_stale)
      input_.clear();
    kind_ = kind;
    prompt_visible_ = false;
    prompt_due_ = true;
  }
  void close() noexcept {
    const bool was_stale = stale();
    scope_.close();
    kind_ = RequestKind::message;
    if (!ordinary_draft() && !was_stale)
      input_.clear();
    prompt_visible_ = false;
    prompt_due_ = true;
  }
  InputLine take(std::string text) {
    auto line = scope_.take(std::move(text));
    input_.clear();
    prompt_visible_ = discard_explained_ = false;
    prompt_due_ = true;
    return line;
  }
  InputLine take() {
    return take(input_.raw());
  }
  std::string view(std::string_view prompt, size_t columns) const {
    return stale() ? std::string(RequestInput::discard_prompt.substr(0, columns))
                   : input_.view(prompt, columns);
  }
  std::string_view guidance() const noexcept {
    if (stale())
      return RequestInput::discard_instruction;
    if (kind_ == RequestKind::approval)
      return "Approve? [y/N]";
    return kind_ == RequestKind::question ? question_prompt : std::string_view{};
  }
  bool before_output() noexcept {
    const bool visible = prompt_visible_;
    prompt_visible_ = false;
    prompt_due_ = true;
    return visible;
  }
  bool needs_prompt() const noexcept {
    return prompt_due_ && !prompt_visible_;
  }
  bool needs_explanation() const noexcept {
    return stale() && !discard_explained_;
  }
  void presented(bool success) noexcept {
    prompt_visible_ = success;
    prompt_due_ = false;
    if (success && stale())
      discard_explained_ = true;
  }
};
} // namespace codet::detail
