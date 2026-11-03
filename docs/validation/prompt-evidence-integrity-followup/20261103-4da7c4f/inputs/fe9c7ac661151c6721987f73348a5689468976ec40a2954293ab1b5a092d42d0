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
  uint64_t revision_ = 0, request_revision_ = 0;
  bool appended_ = false;

public:
  static constexpr std::string_view approval_prompt = "Approve? [y/N]";
  static constexpr std::string_view question_prompt = "Enter a choice number or a text answer.";
  uint64_t revision() const noexcept {
    return revision_;
  }
  uint64_t request_revision() const noexcept {
    return request_revision_;
  }
  bool appended_since(uint64_t painted) const noexcept {
    return appended_ && revision_ - painted == 1;
  }
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
    appended_ = false;
    input_.unicode(enabled);
    ++revision_;
  }
  bool append(char byte) {
    if (input_.size() == CursesInput::limit)
      return false;
    scope_.edit();
    input_ += byte;
    appended_ = true;
    ++revision_;
    return true;
  }
  void erase_character() {
    if (input_.empty())
      return;
    input_.erase_character();
    appended_ = false;
    ++revision_;
    if (input_.empty())
      scope_.edited_empty();
  }
  void presentation_clear() noexcept {
    appended_ = false;
    if (!scope_.stale())
      input_.clear();
    ++revision_;
  }
  void open(ApprovalIdentity identity, RequestKind kind) {
    appended_ = false;
    const bool was_stale = stale();
    if (scope_.open(std::move(identity)) && !was_stale)
      input_.clear();
    kind_ = kind;
    ++revision_;
    ++request_revision_;
  }
  void close() noexcept {
    appended_ = false;
    const bool was_stale = stale();
    scope_.close();
    kind_ = RequestKind::message;
    if (!ordinary_draft() && !was_stale)
      input_.clear();
    ++revision_;
    ++request_revision_;
  }
  InputLine take(std::string text) {
    appended_ = false;
    auto line = scope_.take(std::move(text));
    input_.clear();
    ++revision_;
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
