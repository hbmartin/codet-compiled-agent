#pragma once
#include "codet/detail/request_input.hpp"

namespace codet::detail {
enum class TurnInputPhase : uint8_t { active, completed };
enum class TurnInputAction : uint8_t {
  ignore,
  quit,
  discard,
  reject_origin,
  restricted,
  new_session,
  answer,
  hold,
  steer
};
struct TurnInputDisposition {
  TurnInputAction action = TurnInputAction::ignore;
  std::optional<InputDiscard> rejection;
  std::string_view guidance;
};
// Commands keep their submission-time policy even when display finishes after the worker.
inline TurnInputDisposition classify_turn_input(const InputLine &line, const Json *pending,
                                                const InputOrigin &origin, TurnInputPhase phase,
                                                bool held) {
  const auto submission = line.classify(pending);
  if (submission.action == InputAction::quit)
    return {TurnInputAction::quit, {}, {}};
  if (submission.rejection)
    return {TurnInputAction::discard, submission.rejection, {}};
  if (!line.origin || line.origin->session != origin.session || line.origin->turn != origin.turn)
    return {TurnInputAction::reject_origin, {}, {}};
  const auto &text = line.text;
  if (text == "/model" || text.rfind("/model ", 0) == 0 || text == "/status" || text == "/compact")
    return {TurnInputAction::restricted,
            {},
            "Use /model, /status and /compact after the active turn completes."};
  if (submission.action == InputAction::answer)
    return {TurnInputAction::answer, {}, {}};
  if (text.rfind("/permissions", 0) == 0)
    return {TurnInputAction::restricted,
            {},
            "Change permission modes after the active turn completes."};
  if (text == "/new")
    return {TurnInputAction::new_session, {}, {}};
  if (text.empty())
    return {TurnInputAction::ignore, {}, {}};
  return {phase == TurnInputPhase::completed || line.presentation_held || held
              ? TurnInputAction::hold
              : TurnInputAction::steer,
          {},
          {}};
}
} // namespace codet::detail
