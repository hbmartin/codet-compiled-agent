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

public:
  static constexpr std::string_view approval_prompt = "Approve? [y/N]";
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
  bool append(char byte) {
    if (input_.size() == CursesInput::limit)
      return false;
    scope_.edit();
    input_ += byte;
    return true;
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
  }
  void close() noexcept {
    const bool was_stale = stale();
    scope_.close();
    kind_ = RequestKind::message;
    if (!ordinary_draft() && !was_stale)
      input_.clear();
  }
  InputLine take(std::string text) {
    auto line = scope_.take(std::move(text));
    input_.clear();
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
      return approval_prompt;
    return kind_ == RequestKind::question ? question_prompt : std::string_view{};
  }
};
} // namespace codet::detail
