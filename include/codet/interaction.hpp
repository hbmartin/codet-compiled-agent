#pragma once
#include "codet/core.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>

namespace codet {
// Terminal APIs stay on the main thread; the agent owns persistence and tool execution.
class Interaction {
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
