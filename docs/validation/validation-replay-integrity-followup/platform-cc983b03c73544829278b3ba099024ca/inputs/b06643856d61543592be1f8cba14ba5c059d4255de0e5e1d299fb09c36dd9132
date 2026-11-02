#pragma once
#include "codet/core.hpp"
#include "codet/detail/warning_bits.hpp"
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
#define CODET_INTERACTION_WARNINGS(X)                                                              \
  X(exec_stop, 1, "Exec cleanup failed while stopping owned process")                              \
  X(exec_original, 2, "Exec cleanup failed; preserving original error")                            \
  X(event_render, 4, "Worker event diagnostic could not be displayed")
#define CODET_WARNING_ENUM(name, bit, message) name = (bit),
  enum class Warning : uint8_t { CODET_INTERACTION_WARNINGS(CODET_WARNING_ENUM) };
#undef CODET_WARNING_ENUM

private:
#define CODET_WARNING_VALUE(name, bit, message) (bit),
  static constexpr unsigned warning_values[] = {CODET_INTERACTION_WARNINGS(CODET_WARNING_VALUE)};
#undef CODET_WARNING_VALUE
  static_assert(detail::valid_warning_values(warning_values),
                "Every warning value must be one unique nonzero bit");
#define CODET_WARNING_MASK(name, bit, message) | (bit)
  using WarningEncoding = detail::WarningBits<0 CODET_INTERACTION_WARNINGS(CODET_WARNING_MASK)>;
#undef CODET_WARNING_MASK
#define CODET_WARNING_MESSAGE(name, bit, message) {Warning::name, message},
  static constexpr std::pair<Warning, std::string_view> warning_messages[] = {
      CODET_INTERACTION_WARNINGS(CODET_WARNING_MESSAGE)};
#undef CODET_WARNING_MESSAGE
  static_assert(sizeof(warning_messages) / sizeof(warning_messages[0]) == WarningEncoding::shift,
                "Every warning must have one unique bit and message");
#undef CODET_INTERACTION_WARNINGS

public:
  // Producers never format, allocate, or invoke a renderer. One pending bit per warning kind.
  void warn(Warning kind) noexcept {
    warnings_.fetch_or(static_cast<unsigned>(kind), std::memory_order_relaxed);
  }
  template <class Consumer> void drain_warnings(Consumer consume, bool retry = true) noexcept {
    // Low bits are new occurrences; high bits are failed attempts awaiting a real UI event.
    constexpr unsigned kinds = WarningEncoding::kinds, shift = WarningEncoding::shift;
    auto snapshot = warnings_.load(std::memory_order_relaxed);
    unsigned pending;
    do {
      pending = (snapshot | (retry ? snapshot >> shift : 0u)) & kinds;
      if (!pending)
        return;
    } while (!warnings_.compare_exchange_weak(snapshot, snapshot & ~(pending | (pending << shift)),
                                              std::memory_order_relaxed));
    for (const auto &entry : warning_messages) {
      const auto bit = static_cast<unsigned>(entry.first);
      if (pending & bit)
        try {
          if (!consume(entry.second))
            warnings_.fetch_or(bit << shift, std::memory_order_relaxed);
        } catch (...) {
          warnings_.fetch_or(bit << shift, std::memory_order_relaxed);
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
