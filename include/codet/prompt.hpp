#pragma once
#include "codet/core.hpp"
namespace codet {
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
