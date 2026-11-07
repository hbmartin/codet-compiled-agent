#pragma once
#include "codet/detail/input_limits.hpp"
#include "codet/detail/prepared_prompt.hpp"
#include <array>
#include <limits>

namespace codet::detail {
// Validated immutable content is the only form admitted into the queue.
class PreparedSubmission {
  std::shared_ptr<const std::string> normalized_;
  std::shared_ptr<const PreparedPrompt> execution_;
  bool command_;
  size_t retained_;
  friend class SubmissionPreparation;
  PreparedSubmission(std::shared_ptr<const std::string> normalized,
                     std::shared_ptr<const PreparedPrompt> execution, bool command)
      : normalized_(std::move(normalized)), execution_(std::move(execution)), command_(command),
        retained_(0) {
    const std::array<const std::string *, 3> strings{&execution_->original, normalized_.get(),
                                                     &execution_->expanded};
    for (size_t i = 0; i < strings.size(); ++i) {
      bool shared = false;
      for (size_t j = 0; j < i; ++j)
        shared = shared || strings[i] == strings[j];
      if (!shared)
        retained_ = add(retained_, strings[i]->size());
    }
  }

public:
  static size_t add(size_t left, size_t right) {
    if (right > std::numeric_limits<size_t>::max() - left)
      throw Error("Pending retained text size overflow");
    return left + right;
  }
  const auto &normalized_storage() const noexcept {
    return normalized_;
  }
  const auto &execution() const noexcept {
    return execution_;
  }
  bool command() const noexcept {
    return command_;
  }
  size_t effective_size() const noexcept {
    return execution_->expanded.size();
  }
  size_t retained_size() const noexcept {
    return retained_;
  }
};
// Inspect once, then use the same validated sizes for allocation and admission.
class SubmissionPreparation {
  std::shared_ptr<const std::string> original_;
  size_t normalized_bytes_;
  bool repaired_ = false;

public:
  explicit SubmissionPreparation(std::shared_ptr<const std::string> original)
      : original_(std::move(original)), normalized_bytes_(0) {
    const auto &text = PreparedPrompt::checked(original_);
    if (text.empty() || text.size() > raw_input_limit)
      throw Error("Pending raw input exceeds 65536-byte limit");
    normalized_bytes_ = normalized_input_size(text, expanded_input_limit, &repaired_);
  }
  size_t retained_size(size_t expansion = 0) const {
    return PreparedSubmission::add(
        PreparedSubmission::add(original_->size(), repaired_ ? normalized_bytes_ : 0), expansion);
  }
  template <class Reserve>
  PreparedSubmission finish(std::shared_ptr<const PreparedPrompt> command, Reserve reserve) const {
    auto original = original_;
    if (command) {
      if (command->original_storage().get() != original_.get() && command->original != *original_)
        throw Error("Prepared command belongs to another input");
      original = command->original_storage();
      if (!command->validated_expansion()) {
        bool repaired = false;
        const auto size = normalized_input_size(command->expanded, expanded_input_limit, &repaired);
        reserve(size, retained_size(size));
        if (repaired)
          command = std::make_shared<const PreparedPrompt>(
              original, std::make_shared<const std::string>(
                            normalized_input_known(command->expanded, size, true)));
      }
    }
    const auto effective = command ? command->expanded.size() : normalized_bytes_;
    reserve(effective,
            retained_size(command && command->expanded_storage().get() != original.get() ? effective
                                                                                         : 0));
    auto normalized = repaired_ ? std::make_shared<const std::string>(
                                      normalized_input_known(*original, normalized_bytes_, true))
                                : original;
    const bool custom = static_cast<bool>(command);
    if (!command)
      command = std::make_shared<const PreparedPrompt>(original, normalized);
    return PreparedSubmission{std::move(normalized), std::move(command), custom};
  }
};
} // namespace codet::detail
