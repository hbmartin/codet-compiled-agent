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
  idle_command,
  steer
};
struct TurnInputDisposition {
  TurnInputAction action = TurnInputAction::ignore;
  std::optional<InputDiscard> rejection;
  std::string_view guidance;
};
// Commands keep their submission-time policy even when display finishes after the worker.
inline TurnInputDisposition classify_turn_input(const InputLine &line, const Json *pending,
                                                const InputOrigin &origin, TurnInputPhase phase) {
  const auto submission = line.classify(pending);
  if (submission.action == InputAction::quit)
    return {TurnInputAction::quit, {}, {}};
  if (submission.rejection)
    return {TurnInputAction::discard, submission.rejection, {}};
  if (!line.origin || line.origin->session != origin.session || line.origin->turn != origin.turn)
    return {TurnInputAction::reject_origin, {}, {}};
  const auto &text = line.text();
  // A bound answer keeps its bytes and identity; only quit is global.
  if (submission.action == InputAction::answer)
    return {TurnInputAction::answer, {}, {}};
  const auto command = local_command(text);
  if (command == LocalCommand::new_session)
    return {TurnInputAction::new_session, {}, {}};
  if (command != LocalCommand::none)
    return {TurnInputAction::restricted, {}, "Use local commands after the active turn completes."};
  if (text.empty())
    return {TurnInputAction::ignore, {}, {}};
  return {phase == TurnInputPhase::completed || line.presentation_held ? TurnInputAction::hold
                                                                       : TurnInputAction::steer,
          {},
          {}};
}
} // namespace codet::detail
