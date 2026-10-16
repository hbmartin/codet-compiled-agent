#pragma once
#include "codet/core.hpp"
#include <string_view>
namespace codet {
struct PromptEntry {
  const char *model, *instructions, *default_mode, *plan_mode, *approval_on_request,
      *approval_never;
};
const PromptEntry *model_prompt(std::string_view);
// Null means absent; an explicitly empty catalog string remains an override.
std::string_view resolve_prompt(const char *, std::string_view fallback);
std::string assemble_prompt(const Config &, const Json &schemas, const Json &capabilities,
                            const std::string &project_context, bool structured_output = false);
std::string_view summarization_prompt();
std::string_view summary_prefix();
struct PromptBudget {
  size_t total, fixed, output;
  size_t available() const {
    return fixed >= total || output >= total - fixed ? 0 : total - fixed - output;
  }
};
PromptBudget prompt_budget(const Config &, const std::string &system, const Json &schemas);

// Append prepared messages without copying history. Roll back before turn recovery on failure.
class PromptAppend {
  Session &session_;
  size_t original_;
  bool committed_ = false;
  void rollback() noexcept {
    while (session_.messages.size() > original_)
      session_.messages.pop_back();
  }

public:
  PromptAppend(Session &session, std::vector<Message> messages,
               const std::function<void(size_t)> &checkpoint = {})
      : session_(session), original_(session.messages.size()) {
    try {
      session_.messages.reserve(original_ + messages.size());
      for (size_t i = 0; i < messages.size(); ++i) {
        if (checkpoint)
          checkpoint(i);
        session_.messages.push_back(std::move(messages[i]));
      }
    } catch (...) {
      rollback();
      throw;
    }
  }
  ~PromptAppend() {
    if (!committed_)
      rollback();
  }
  PromptAppend(const PromptAppend &) = delete;
  PromptAppend &operator=(const PromptAppend &) = delete;
  void commit() noexcept {
    committed_ = true;
  }
};
} // namespace codet
