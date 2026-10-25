#pragma once
#include <cstddef>
namespace codet::test {
struct NativeIoMetrics {
  bool observe = false;
  unsigned interrupted = 0, races = 0, sends = 0, waits = 0;
  size_t chunk = 2;
};
extern thread_local NativeIoMetrics native_io_metrics;
} // namespace codet::test
