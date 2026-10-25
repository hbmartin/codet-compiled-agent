#pragma once
namespace codet::test {
struct NativeTerminalMetrics {
  bool observe = false, ended_before_delete = false;
  bool detach_failure = false, query_failure = false;
  unsigned ends = 0, deletes = 0, settings = 0, live_settings = 0;
};
#ifdef CODET_NATIVE_TERMINAL_METRICS
extern NativeTerminalMetrics native_terminal_metrics;
#else
inline NativeTerminalMetrics native_terminal_metrics;
#endif
} // namespace codet::test
