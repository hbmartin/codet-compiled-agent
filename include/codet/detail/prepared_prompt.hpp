#pragma once
#include "codet/core.hpp"
namespace codet::detail {
// Local preparation occurs before owning a provider turn. Keep original hook input distinct.
struct PreparedPrompt {
  std::string original, expanded;
};
std::optional<PreparedPrompt> prepare_interactive_prompt(const Config &, FileSystem &,
                                                         const std::string &);
struct AgentAccess {
  static RunResult run(Agent &agent, Session &session, const PreparedPrompt &prompt,
                       const Cancellation &cancel, const Json &schema,
                       const std::vector<ImageArtifact> &images) {
    return agent.run_impl(session, prompt.original, cancel, schema, false, images,
                          &prompt.expanded);
  }
};
} // namespace codet::detail
