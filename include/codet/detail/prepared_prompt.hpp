#pragma once
#include "codet/core.hpp"
#include <cstdint>
namespace codet::detail {
// Local preparation occurs before owning a provider turn. Keep original hook input distinct.
struct PreparedPrompt {
private:
  std::shared_ptr<const std::string> original_, expanded_;
  bool validated_ = false;
  friend std::optional<PreparedPrompt>
  prepare_interactive_prompt(const Config &, FileSystem &, std::shared_ptr<const std::string>,
                             const std::function<void(size_t)> &);

public:
  const std::string &original, &expanded;
  PreparedPrompt(std::shared_ptr<const std::string> input,
                 std::shared_ptr<const std::string> content)
      : original_(std::move(input)), expanded_(std::move(content)), original(checked(original_)),
        expanded(checked(expanded_)) {}
  PreparedPrompt(std::string input, std::string content)
      : PreparedPrompt(std::make_shared<const std::string>(std::move(input)),
                       std::make_shared<const std::string>(std::move(content))) {}
  static const std::string &checked(const std::shared_ptr<const std::string> &value) {
    if (!value)
      throw Error("Prepared input has no text backing");
    return *value;
  }
  const auto &original_storage() const noexcept {
    return original_;
  }
  const auto &expanded_storage() const noexcept {
    return expanded_;
  }
  bool validated_expansion() const noexcept {
    return validated_;
  }
};
std::optional<PreparedPrompt>
prepare_interactive_prompt(const Config &, FileSystem &, const std::string &,
                           const std::function<void(size_t)> &reserve = {});
std::optional<PreparedPrompt>
prepare_interactive_prompt(const Config &, FileSystem &, std::shared_ptr<const std::string>,
                           const std::function<void(size_t)> &reserve = {});
class PreparedSubmission;
enum class PreparationContext : uint8_t { fresh, pending };
enum class PreparationState : uint8_t { unprepared, ready, failed, cancelled };
struct PreparationOutcome {
  PreparationState state = PreparationState::unprepared;
  std::shared_ptr<const PreparedPrompt> prompt;
  std::exception_ptr failure;
  std::shared_ptr<const PreparedSubmission> submission = {};
};
inline std::shared_ptr<const PreparedPrompt> share_prepared(std::optional<PreparedPrompt> value) {
  return value ? std::make_shared<const PreparedPrompt>(std::move(*value)) : nullptr;
}
inline std::shared_ptr<const PreparedPrompt>
share_prepared(std::shared_ptr<const PreparedPrompt> value) {
  return value;
}
// Expected cancellation belongs to the input phase, not the UI exception boundary.
template <class Prepare>
PreparationOutcome prepare_submission(const Cancellation &cancel, Prepare prepare) {
  try {
    cancel.check();
    auto value = prepare();
    cancel.check();
    auto prompt = share_prepared(std::move(value));
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
