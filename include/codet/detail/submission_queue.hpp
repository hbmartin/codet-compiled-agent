#pragma once
#include "codet/detail/submission_lease.hpp"
#include "codet/detail/submission_payload.hpp"
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
      : backing_(std::move(backing)), text(PreparedPrompt::checked(backing_)),
        prepared(std::move(command)) {}
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

private:
  static auto reserved(Interaction &io, uint64_t sequence) {
    const auto entry = std::find_if(io.input_.begin(), io.input_.end(),
                                    [&](const auto &line) { return line.sequence == sequence; });
    if (entry == io.input_.end() || entry->kind != Interaction::InputKind::reserved)
      throw Error("Pending submission lost its admission");
    return entry;
  }
  static void account_locked(Interaction &io, Interaction::Input &entry, size_t bytes,
                             size_t retained) {
    io.input_bytes_ = io.input_bytes_ - entry.charged_bytes + bytes;
    io.retained_bytes_ = io.retained_bytes_ - entry.retained_bytes + retained;
    entry.charged_bytes = bytes;
    entry.retained_bytes = retained;
  }
  static void charge_locked(Interaction &io, Interaction::Input &entry, size_t bytes,
                            size_t retained) {
    io.check_input_capacity(bytes, retained, entry.charged_bytes, entry.retained_bytes);
    account_locked(io, entry, bytes, retained);
  }
  static void assign(Interaction::Input &entry, const PreparedSubmission &payload, bool followup) {
    entry.text = payload.normalized_storage();
    entry.prepared = payload.execution();
    entry.kind = followup ? Interaction::InputKind::followup : Interaction::InputKind::steering;
  }

public:
  static void charge(Interaction &io, uint64_t sequence, size_t bytes, size_t retained) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    charge_locked(io, *reserved(io, sequence), bytes, retained);
  }
  static void charge(Interaction &io, uint64_t sequence, size_t bytes) {
    charge(io, sequence, bytes, bytes);
  }
  static void commit_prepared(Interaction &io, uint64_t sequence, const PreparedSubmission &payload,
                              bool held) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    const auto entry = reserved(io, sequence);
    charge_locked(io, *entry, payload.effective_size(), payload.retained_size());
    const bool followup = payload.command() || held || entry->turn != io.turn_ ||
                          io.input_barrier(entry) || !io.active_;
    assign(*entry, payload, followup);
    io.changed_.notify_all();
  }
  static void preflight(Interaction &io, size_t bytes = 0, size_t retained = 0) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    if (io.input_.size() >= queued_input_count)
      throw Error("Pending input exceeds 100-entry limit");
    io.check_input_capacity(bytes, retained);
  }
  static void insert(Interaction &io, const PreparedSubmission &payload) {
    std::lock_guard<std::mutex> lock(io.mutex_);
    if (io.input_.size() >= queued_input_count)
      throw Error("Pending input exceeds 100-entry limit");
    io.check_input_capacity(payload.effective_size(), payload.retained_size());
    const bool followup = payload.command() || !io.active_ || io.input_barrier(io.input_.cend());
    io.input_.push_back(
        {io.session_, {}, io.turn_, ++io.next_input_, Interaction::InputKind::reserved});
    auto &entry = io.input_.back();
    account_locked(io, entry, payload.effective_size(), payload.retained_size());
    assign(entry, payload, followup);
    io.changed_.notify_all();
  }
  template <class Prepare>
  static void submit(Interaction &io, const std::string &text, Prepare prepare) {
    SubmissionPreparation::validate_raw(text);
    preflight(io);
    const auto payload = prepare(text); // No queue state is visible during preparation.
    insert(io, payload);
  }
  // Raw compatibility callers use exactly the same payload preparation as the UI.
  static void commit(Interaction &io, uint64_t sequence, const std::string &text, bool held,
                     bool command = false, std::shared_ptr<const PreparedPrompt> expansion = {}) {
    const auto payload = SubmissionPreparation::from_raw(
        text, command, std::move(expansion),
        [&](size_t bytes, size_t retained) { charge(io, sequence, bytes, retained); });
    commit_prepared(io, sequence, payload, held);
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
