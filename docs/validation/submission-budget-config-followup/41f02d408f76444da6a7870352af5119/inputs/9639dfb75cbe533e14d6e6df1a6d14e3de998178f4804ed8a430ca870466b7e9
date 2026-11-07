#pragma once
#include "codet/core.hpp"
#include <cstdint>
namespace codet::detail {
// Local preparation occurs before owning a provider turn. Keep original hook input distinct.
struct PreparedPrompt {
  std::string original, expanded;
};
std::optional<PreparedPrompt>
prepare_interactive_prompt(const Config &, FileSystem &, const std::string &,
                           const std::function<void(size_t)> &reserve = {});
enum class PreparationContext : uint8_t { fresh, pending };
enum class PreparationState : uint8_t { unprepared, ready, failed, cancelled };
struct PreparationOutcome {
  PreparationState state = PreparationState::unprepared;
  std::shared_ptr<const PreparedPrompt> prompt;
  std::exception_ptr failure;
  std::shared_ptr<const std::string> normalized = {};
};
// Expected cancellation belongs to the input phase, not the UI exception boundary.
template <class Prepare>
PreparationOutcome prepare_submission(const Cancellation &cancel, Prepare prepare) {
  try {
    cancel.check();
    auto value = prepare();
    cancel.check();
    auto prompt = value ? std::make_shared<const PreparedPrompt>(std::move(*value)) : nullptr;
    return {PreparationState::ready, std::move(prompt), {}};
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const Error &error) {
    return {cancel.requested || error.code == 130 ? PreparationState::cancelled
                                                  : PreparationState::failed,
            {},
            std::current_exception()};
  } catch (const std::exception &) {
    return {cancel.requested ? PreparationState::cancelled : PreparationState::failed,
            {},
            std::current_exception()};
  }
}
struct AgentAccess {
  static RunResult run(Agent &agent, Session &session, const PreparedPrompt &prompt,
                       const Cancellation &cancel, const Json &schema,
                       const std::vector<ImageArtifact> &images) {
    return agent.run_impl(session, prompt.original, cancel, schema, false, images,
                          &prompt.expanded);
  }
};
} // namespace codet::detail
