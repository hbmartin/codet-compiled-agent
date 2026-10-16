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
  std::deque<std::string> input_;
  std::map<std::string, std::string> answers_;

public:
  void post(Json);
  std::vector<Json> events();
  std::string request(const std::string &, Json, const Cancellation &);
  void answer(const std::string &, const std::string &);
  void submit(const std::string &);
  std::vector<std::string> take_input();
  bool sleep(int64_t milliseconds, const Cancellation &);
};
} // namespace codet
