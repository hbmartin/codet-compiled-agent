#include "codet/interaction.hpp"

namespace codet {
void Interaction::post(Json event) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (events_.size() >= 2048 && event.value("type", "") == "item.updated")
    return;
  events_.push_back(std::move(event));
  changed_.notify_all();
}
std::vector<Json> Interaction::events() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Json> out(events_.begin(), events_.end());
  events_.clear();
  return out;
}
std::string Interaction::request(const std::string &kind, Json payload,
                                 const Cancellation &cancel) {
  const auto id = unique_id();
  payload["type"] = kind;
  payload["request_id"] = id;
  post(std::move(payload));
  std::unique_lock<std::mutex> lock(mutex_);
  while (!answers_.count(id)) {
    cancel.check();
    changed_.wait_for(lock, std::chrono::milliseconds(50));
  }
  auto result = answers_.at(id);
  answers_.erase(id);
  return result;
}
void Interaction::answer(const std::string &id, const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  answers_[id] = value;
  changed_.notify_all();
}
void Interaction::submit(const std::string &text) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (input_.size() >= 100 || text.size() > 65536)
    throw Error("Pending user input limit exceeded");
  input_.push_back(safe_text(text));
  changed_.notify_all();
}
std::vector<std::string> Interaction::take_input() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> out(input_.begin(), input_.end());
  input_.clear();
  return out;
}
bool Interaction::sleep(int64_t milliseconds, const Cancellation &cancel) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  std::unique_lock<std::mutex> lock(mutex_);
  while (input_.empty() && std::chrono::steady_clock::now() < until) {
    cancel.check();
    changed_.wait_until(
        lock, std::min(until, std::chrono::steady_clock::now() + std::chrono::milliseconds(50)));
  }
  cancel.check();
  return !input_.empty();
}
} // namespace codet
