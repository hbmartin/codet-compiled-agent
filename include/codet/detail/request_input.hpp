#pragma once
#include "codet/detail/approval_identity.hpp"
namespace codet::detail {
enum class InputAction : uint8_t { quit, discard, answer, message };
struct InputLine {
  std::string text;
  std::optional<ApprovalIdentity> request;
  bool discarded = false;
  InputAction action(const Json *pending = nullptr) const {
    if (text == "/quit" || text == "/exit")
      return InputAction::quit;
    if (discarded || (request && (!pending || !request->matches(*pending))))
      return InputAction::discard;
    return request ? InputAction::answer : InputAction::message;
  }
};
// Intent belongs to the draft, not whichever request happens to exist at Enter.
class RequestInput {
  std::optional<ApprovalIdentity> active_, draft_;
  bool started_ = false, stale_ = false;

public:
  static constexpr std::string_view discard_prompt = "discard> ";
  static constexpr std::string_view discard_instruction =
      "Unfinished input must be discarded. Press Enter, then type a fresh answer.";
  bool open(ApprovalIdentity identity) {
    if (started_)
      stale_ = true;
    active_ = std::move(identity);
    return stale_;
  }
  void edit() {
    if (!started_) {
      draft_ = active_;
      started_ = true;
    }
  }
  void edited_empty() noexcept {
    if (!stale_) {
      draft_.reset();
      started_ = false;
    }
  }
  void close() noexcept {
    if (started_ && draft_)
      stale_ = true;
    active_.reset();
  }
  bool stale() const noexcept {
    return stale_;
  }
  bool ordinary_draft() const noexcept {
    return started_ && !draft_ && !stale_;
  }
  InputLine take(std::string text) {
    InputLine result{std::move(text), started_ ? std::move(draft_) : active_, stale_};
    draft_.reset();
    started_ = stale_ = false;
    return result;
  }
};
} // namespace codet::detail
