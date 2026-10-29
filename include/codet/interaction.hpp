#pragma once
#include "codet/core.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>

namespace codet {
// Terminal APIs stay on the main thread; the agent owns persistence and tool execution.
class Interaction {
  std::atomic<unsigned> warnings_{0};
  std::mutex mutex_;
  std::condition_variable changed_;
  std::deque<Json> events_;
  struct Input {
    std::string session, text;
    uint64_t turn = 0;
  };
  std::deque<Input> input_;
  std::string session_;
  uint64_t turn_ = 0;
  bool active_ = true;
  std::map<std::string, std::string> answers_;
  std::map<std::string, uint64_t> requests_;

public:
  enum class Warning : unsigned { exec_stop = 1, exec_original = 2, event_render = 4 };
  // Producers never format, allocate, or invoke a renderer. One pending bit per warning kind.
  void warn(Warning kind) noexcept {
    warnings_.fetch_or(static_cast<unsigned>(kind), std::memory_order_relaxed);
  }
  template <class Consumer> void drain_warnings(Consumer consume) noexcept {
    const auto pending = warnings_.exchange(0, std::memory_order_relaxed);
    constexpr std::pair<Warning, std::string_view> messages[] = {
        {Warning::exec_stop, "Exec cleanup failed while stopping owned process"},
        {Warning::exec_original, "Exec cleanup failed; preserving original error"},
        {Warning::event_render, "Worker event diagnostic could not be displayed"}};
    for (const auto &entry : messages) {
      const auto bit = static_cast<unsigned>(entry.first);
      if (pending & bit)
        try {
          if (!consume(entry.second))
            warn(entry.first);
        } catch (...) {
          warn(entry.first); // Retry only when the main thread next drains warnings.
        }
    }
  }
  void post(Json);
  std::vector<Json> events();
  void deliver(const EventSink &, const DiagnosticSink & = {}, bool recovering = false);
  std::string request(const std::string &, Json, const Cancellation &);
  bool answer(const std::string &, const std::string &);
  bool request_active(const std::string &);
  bool current_event(const Json &);
  void submit(const std::string &);
  void begin_turn(const std::string &);
  void end_turn();
  std::vector<std::string> take_input();
  std::vector<std::string> take_pending(const std::string &);
  bool sleep(int64_t milliseconds, const Cancellation &);
};
} // namespace codet
