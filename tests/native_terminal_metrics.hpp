#pragma once
namespace codet::test {
struct NativeTerminalMetrics {
  bool observe = false, ended_before_delete = false;
  unsigned ends = 0, deletes = 0, settings = 0;
};
extern NativeTerminalMetrics native_terminal_metrics;
} // namespace codet::test
