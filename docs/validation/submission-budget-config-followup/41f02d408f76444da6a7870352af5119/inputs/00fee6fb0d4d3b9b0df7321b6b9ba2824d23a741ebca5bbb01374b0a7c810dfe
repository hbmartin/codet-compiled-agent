#pragma once
#include <cstdint>
#include <utility>
namespace codet {
class Interaction;
namespace detail {
void abandon_submission(Interaction *, uint64_t) noexcept;
// A main-thread input line owns admission until the queue accepts or rejects it.
class SubmissionLease {
  Interaction *owner_ = nullptr;
  uint64_t sequence_ = 0;

public:
  SubmissionLease() = default;
  SubmissionLease(Interaction *owner, uint64_t sequence) noexcept
      : owner_(owner), sequence_(sequence) {}
  SubmissionLease(const SubmissionLease &) = delete;
  SubmissionLease &operator=(const SubmissionLease &) = delete;
  SubmissionLease(SubmissionLease &&other) noexcept
      : owner_(std::exchange(other.owner_, nullptr)), sequence_(std::exchange(other.sequence_, 0)) {
  }
  SubmissionLease &operator=(SubmissionLease &&other) noexcept {
    if (this != &other) {
      abandon_submission(owner_, sequence_);
      owner_ = std::exchange(other.owner_, nullptr);
      sequence_ = std::exchange(other.sequence_, 0);
    }
    return *this;
  }
  ~SubmissionLease() {
    abandon_submission(owner_, sequence_);
  }
  void release() noexcept {
    owner_ = nullptr;
    sequence_ = 0;
  }
};
} // namespace detail
} // namespace codet
