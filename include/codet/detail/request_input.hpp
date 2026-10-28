#pragma once
#include "codet/detail/approval_identity.hpp"
namespace codet::detail {
struct InputLine {
  std::string text;
  std::optional<ApprovalIdentity> request;
  bool discarded = false;
};
// Intent belongs to the draft, not whichever request happens to exist at Enter.
class RequestInput {
  std::optional<ApprovalIdentity> active_, draft_;
  bool started_ = false, stale_ = false;

public:
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
