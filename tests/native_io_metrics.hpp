#pragma once
#include <cstddef>
namespace codet::test {
struct NativeIoMetrics {
  bool observe = false;
  unsigned interrupted = 0, races = 0, sends = 0, waits = 0;
  size_t chunk = 2;
  int interrupt_fd = -1;
  unsigned writes_interrupted = 0, vectors_interrupted = 0, polls_interrupted = 0;
  unsigned input_polls_failed = 0;
  int input_error = 0;
  unsigned stats_interrupted = 0, bounds_interrupted = 0;
  unsigned writes = 0, vectors = 0, inspections = 0;
  unsigned bound_queries = 0;
  bool bound_indeterminate = false;
  bool vector_partial = false, vector_failure = false;
  void (*before_output)(void *) = nullptr;
  void *output_context = nullptr;
};
extern thread_local NativeIoMetrics native_io_metrics;
} // namespace codet::test
