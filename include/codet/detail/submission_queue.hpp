#pragma once
#include "codet/detail/prepared_prompt.hpp"
#include "codet/detail/submission_lease.hpp"
#include "codet/interaction.hpp"
#include <algorithm>

namespace codet::detail {
struct QueuedSubmission {
  std::string text;
  bool idle_command = false;
  std::shared_ptr<const PreparedPrompt> prepared;
};
// Internal access to the existing interaction mutex and its single input owner.
// Reserved entries account for frozen lines before presentation hands them over.
struct SubmissionAccess {
  static uint64_t reserve(Interaction &io) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    if (io.input_.size() >= 100)
      throw Error("Pending user input limit exceeded");
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
    if (entry != io.input_.end() && entry->kind == Interaction::InputKind::reserved)
      io.input_.erase(entry);
  }
  static void commit(Interaction &io, uint64_t sequence, const std::string &text, bool held,
                     bool command = false, std::shared_ptr<const PreparedPrompt> expansion = {}) {
    if (text.empty() || text.size() > 65536)
      throw Error("Pending user input limit exceeded");
    auto prepared = safe_text(text);
    std::lock_guard<std::mutex> lock(io.mutex_);
    const auto entry = std::find_if(io.input_.begin(), io.input_.end(),
                                    [&](const auto &line) { return line.sequence == sequence; });
    if (entry == io.input_.end() || entry->kind != Interaction::InputKind::reserved)
      throw Error("Pending submission lost its admission");
    const bool barrier = io.input_barrier(entry);
    entry->text = std::move(prepared);
    entry->prepared = std::move(expansion);
    entry->kind = command                          ? Interaction::InputKind::idle_command
                  : held || barrier || !io.active_ ? Interaction::InputKind::followup
                                                   : Interaction::InputKind::steering;
    io.changed_.notify_all();
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
    QueuedSubmission result{std::move(entry.text),
                            entry.kind == Interaction::InputKind::idle_command,
                            std::move(entry.prepared)};
    io.input_.pop_front();
    return result;
  }
};
} // namespace codet::detail
