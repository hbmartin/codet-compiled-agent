#pragma once
#include "codet/detail/prepared_prompt.hpp"
#include "codet/detail/submission_lease.hpp"
#include "codet/interaction.hpp"
#include <algorithm>

namespace codet::detail {
struct QueuedSubmission {
private:
  std::shared_ptr<const std::string> backing_;

public:
  const std::string &text;
  std::shared_ptr<const PreparedPrompt> prepared;
  QueuedSubmission(std::shared_ptr<const std::string> backing,
                   std::shared_ptr<const PreparedPrompt> command)
      : backing_(std::move(backing)), text(*backing_), prepared(std::move(command)) {}
};
// Queue admission and accounting share the existing interaction mutex. I/O and
// normalization happen outside it; immutable normalized payloads transfer once.
struct SubmissionAccess {
  static uint64_t reserve(Interaction &io) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    if (io.input_.size() >= queued_input_count)
      throw Error("Pending input exceeds 100-entry limit");
    const auto sequence = ++io.next_input_;
    io.input_.push_back({io.session_, {}, io.turn_, sequence, Interaction::InputKind::reserved});
    return sequence;
  }
  static void abandon(Interaction &io, uint64_t sequence) noexcept {
    if (!sequence)
      return;
    std::lock_guard<std::mutex> lock(io.mutex_);
    const auto entry = std::find_if(io.input_.begin(), io.input_.end(),
                                    [&](const auto &line) { return line.sequence == sequence; });
    if (entry != io.input_.end() && entry->kind == Interaction::InputKind::reserved) {
      io.input_bytes_ -= entry->charged_bytes;
      io.retained_bytes_ -= entry->retained_bytes;
      io.input_.erase(entry);
    }
  }
  static void charge(Interaction &io, uint64_t sequence, size_t bytes, size_t retained) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    const auto entry = std::find_if(io.input_.begin(), io.input_.end(),
                                    [&](const auto &line) { return line.sequence == sequence; });
    if (entry == io.input_.end() || entry->kind != Interaction::InputKind::reserved)
      throw Error("Pending submission lost its admission");
    io.check_input_capacity(bytes, retained, entry->charged_bytes, entry->retained_bytes);
    io.input_bytes_ = io.input_bytes_ - entry->charged_bytes + bytes;
    io.retained_bytes_ = io.retained_bytes_ - entry->retained_bytes + retained;
    entry->charged_bytes = bytes;
    entry->retained_bytes = retained;
  }
  static void charge(Interaction &io, uint64_t sequence, size_t bytes) {
    charge(io, sequence, bytes, bytes);
  }
  static size_t retained_size(const std::shared_ptr<const std::string> &text,
                              const std::shared_ptr<const PreparedPrompt> &expansion) {
    size_t bytes = text->size();
    if (expansion) {
      if (text.get() != &expansion->original)
        bytes += expansion->original.size();
      bytes += expansion->expanded.size();
    }
    return bytes;
  }
  static void commit_prepared(Interaction &io, uint64_t sequence,
                              std::shared_ptr<const std::string> text, bool held, bool command,
                              std::shared_ptr<const PreparedPrompt> expansion = {}) {
    const auto bytes = expansion ? expansion->expanded.size() : text->size();
    const auto retained = retained_size(text, expansion);
    std::lock_guard<std::mutex> lock(io.mutex_);
    const auto entry = std::find_if(io.input_.begin(), io.input_.end(),
                                    [&](const auto &line) { return line.sequence == sequence; });
    if (entry == io.input_.end() || entry->kind != Interaction::InputKind::reserved)
      throw Error("Pending submission lost its admission");
    io.check_input_capacity(bytes, retained, entry->charged_bytes, entry->retained_bytes);
    io.input_bytes_ = io.input_bytes_ - entry->charged_bytes + bytes;
    io.retained_bytes_ = io.retained_bytes_ - entry->retained_bytes + retained;
    entry->charged_bytes = bytes;
    entry->retained_bytes = retained;
    const bool followup =
        command || held || entry->turn != io.turn_ || io.input_barrier(entry) || !io.active_;
    entry->text = std::move(text);
    entry->prepared = std::move(expansion);
    entry->kind = followup ? Interaction::InputKind::followup : Interaction::InputKind::steering;
    io.changed_.notify_all();
  }
  // Compatibility entry point for host harnesses and internal callers with raw text.
  static void commit(Interaction &io, uint64_t sequence, const std::string &text, bool held,
                     bool command = false, std::shared_ptr<const PreparedPrompt> expansion = {}) {
    if (text.empty() || text.size() > raw_input_limit)
      throw Error("Pending raw input exceeds 65536-byte limit");
    bool repaired = false;
    const auto length = normalized_input_size(text, expanded_input_limit, &repaired);
    const auto effective =
        expansion ? normalized_input_size(expansion->expanded, expanded_input_limit) : length;
    const auto retained =
        length + (expansion ? expansion->original.size() + expansion->expanded.size() : 0);
    charge(io, sequence, effective, retained);
    auto normalized =
        std::make_shared<const std::string>(normalized_input_known(text, length, repaired));
    commit_prepared(io, sequence, std::move(normalized), held, command, std::move(expansion));
  }
  static std::optional<QueuedSubmission> next(Interaction &io, const std::string &session) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    if (io.input_.empty())
      return {};
    if (io.active_ || io.session_ != session)
      throw Error("Pending messages belong to another active session");
    if (io.input_.front().kind == Interaction::InputKind::reserved)
      return {};
    auto &entry = io.input_.front();
    QueuedSubmission result{std::move(entry.text), std::move(entry.prepared)};
    io.input_bytes_ -= entry.charged_bytes;
    io.retained_bytes_ -= entry.retained_bytes;
    io.input_.pop_front();
    return result;
  }
};
} // namespace codet::detail
