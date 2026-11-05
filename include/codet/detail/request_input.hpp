#pragma once
#include "codet/detail/approval_identity.hpp"
#include "codet/detail/local_command.hpp"
namespace codet::detail {
enum class InputDiscard : uint8_t { unfinished, expired };
inline std::string_view discard_message(InputDiscard reason) noexcept {
  return reason == InputDiscard::unfinished ? "Unfinished input discarded."
                                            : "Request expired; answer discarded.";
}
enum class InputAction : uint8_t { quit, discard, answer, message };
struct InputSubmission {
  InputAction action = InputAction::message;
  std::optional<InputDiscard> rejection;
};
struct InputOrigin {
  std::string session;
  uint64_t turn = 0;
};
struct InputLine {
  std::string text;
  std::optional<ApprovalIdentity> request;
  bool discarded = false;
  std::optional<InputOrigin> origin;
  bool presentation_held = false;
  uint64_t admission = 0;
  InputLine(std::string bytes, std::optional<ApprovalIdentity> identity = {}, bool stale = false,
            std::optional<InputOrigin> source = {})
      : text(std::move(bytes)), request(std::move(identity)), discarded(stale),
        origin(std::move(source)) {}
  InputSubmission classify(const Json *pending = nullptr) const {
    if (is_quit_command(text))
      return {InputAction::quit, {}};
    if (discarded)
      return {InputAction::discard, InputDiscard::unfinished};
    if (request && (!pending || !request->matches(*pending)))
      return {InputAction::discard, InputDiscard::expired};
    return {request ? InputAction::answer : InputAction::message, {}};
  }
  InputAction action(const Json *pending = nullptr) const {
    return classify(pending).action;
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
    InputLine result{std::move(text), started_ ? std::move(draft_) : active_, stale_, {}};
    draft_.reset();
    started_ = stale_ = false;
    return result;
  }
};
} // namespace codet::detail
