#include "codet/interaction.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <algorithm>
#include <cstdio>
#include <iterator>

namespace codet {
void Interaction::post(Json event) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (events_.size() >= 2048 && event.value("type", "") == "item.updated")
    return;
  event["codet_session_id"] = session_;
  event["codet_turn"] = turn_;
  events_.push_back(std::move(event));
  changed_.notify_all();
}
std::vector<Json> Interaction::events() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Json> out(events_.begin(), events_.end());
  events_.clear();
  return out;
}
void Interaction::deliver(const EventSink &sink, const DiagnosticSink &diagnostics,
                          bool recovering) {
  auto batch = events();
  const auto diagnose = [&](const std::string &message) {
    try {
      report_diagnostic(diagnostics, message);
    } catch (...) {
      warn(Warning::event_render);
    }
  };
  for (size_t i = 0; i < batch.size(); ++i) {
    try {
      const auto &event = batch[i];
      if (!current_event(event))
        continue;
      const auto type = event.value("type", "");
      if ((type == "codet.approval" || type == "codet.question") &&
          (recovering || !request_active(event.at("request_id"))))
        continue;
      sink(event);
    } catch (const Json::exception &error) {
      diagnose("Invalid worker event: " + safe_text(error.what()));
    } catch (...) {
      if (!recovering) {
        // Preserve the failed event and every undelivered event ahead of newer worker output.
        std::lock_guard<std::mutex> lock(mutex_);
        events_.insert(events_.begin(),
                       std::make_move_iterator(batch.begin() + static_cast<std::ptrdiff_t>(i)),
                       std::make_move_iterator(batch.end()));
        throw;
      }
      diagnose("Worker event could not be displayed during cleanup");
    }
  }
}
std::string Interaction::request(const std::string &kind, Json payload,
                                 const Cancellation &cancel) {
  const auto id = unique_id();
  payload["type"] = kind;
  payload["request_id"] = id;
  std::unique_lock<std::mutex> lock(mutex_);
  cancel.check();
  if (!active_)
    throw Error("Input turn has ended", 130);
  requests_[id] = turn_;
  payload["codet_session_id"] = session_;
  payload["codet_turn"] = turn_;
  events_.push_back(std::move(payload));
  changed_.notify_all();
  try {
    while (!answers_.count(id)) {
      if (!requests_.count(id))
        throw Error("Input request expired", 130);
      cancel.check();
      changed_.wait_for(lock, std::chrono::milliseconds(50));
    }
    cancel.check();
    auto result = answers_.at(id);
    answers_.erase(id);
    requests_.erase(id);
    return result;
  } catch (...) {
    requests_.erase(id);
    answers_.erase(id);
    throw;
  }
}
bool Interaction::answer(const std::string &id, const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto request = requests_.find(id);
  if (!active_ || request == requests_.end() || request->second != turn_ || answers_.count(id))
    return false;
  answers_[id] = value;
  changed_.notify_all();
  return true;
}
bool Interaction::request_active(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto request = requests_.find(id);
  return active_ && request != requests_.end() && request->second == turn_ && !answers_.count(id);
}
bool Interaction::current_event(const Json &event) {
  std::lock_guard<std::mutex> lock(mutex_);
  return event.value("codet_turn", uint64_t{0}) == 0 ||
         (event.value("codet_turn", uint64_t{0}) == turn_ &&
          event.value("codet_session_id", "") == session_);
}
bool Interaction::input_barrier(std::deque<Input>::const_iterator end) const noexcept {
  return std::any_of(input_.cbegin(), end,
                     [](const auto &line) { return line.kind != InputKind::steering; });
}
void Interaction::submit(const std::string &text) {
  if (text.empty())
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (input_.size() >= 100 || text.size() > 65536)
    throw Error("Pending user input limit exceeded");
  const bool held = !active_ || input_barrier(input_.cend());
  input_.push_back({session_, safe_text(text), turn_, ++next_input_,
                    held ? InputKind::followup : InputKind::steering});
  changed_.notify_all();
}
std::vector<std::string> Interaction::take_input() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_)
    return {};
  std::vector<std::string> out;
  out.reserve(input_.size());
  auto end = input_.begin();
  for (; end != input_.end() && end->kind == InputKind::steering; ++end) {
    if (end->session != session_ || end->turn != turn_)
      throw Error("Queued input belongs to another session or turn");
    out.push_back(end->text);
  }
  input_.erase(input_.begin(), end);
  return out;
}
void Interaction::begin_turn(const std::string &session) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!input_.empty() && session_ != session)
    throw Error("Handle pending messages before switching sessions");
  if (active_ && session_ == session)
    return;
  session_ = session;
  ++turn_;
  active_ = true;
}
void Interaction::end_turn() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Unread input belongs to a separate follow-up, never the next worker's steering window.
  for (auto &line : input_)
    if (line.kind == InputKind::steering)
      line.kind = InputKind::followup;
  active_ = false;
  answers_.clear();
  requests_.clear();
  changed_.notify_all();
}
std::vector<std::string> Interaction::take_pending(const std::string &session) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_ || session != session_)
    throw Error("Pending messages belong to another active session");
  std::vector<std::string> out;
  out.reserve(input_.size());
  for (const auto &input : input_)
    out.push_back(input.text);
  input_.clear();
  return out;
}
bool Interaction::sleep(int64_t milliseconds, const Cancellation &cancel) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  std::unique_lock<std::mutex> lock(mutex_);
  const auto ready = [&] { return !input_.empty() && input_.front().kind == InputKind::steering; };
  while (!ready() && std::chrono::steady_clock::now() < until) {
    cancel.check();
    changed_.wait_until(
        lock, std::min(until, std::chrono::steady_clock::now() + std::chrono::milliseconds(50)));
  }
  cancel.check();
  return ready();
}
} // namespace codet
