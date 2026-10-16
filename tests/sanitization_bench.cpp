// Host-only measurement of the serialization component of saves and streamed events.
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "temp.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
std::atomic<size_t> allocations{0}, allocated_bytes{0};
void *allocate(size_t size) {
  if (void *memory = std::malloc(size ? size : 1)) {
    ++allocations;
    allocated_bytes += size;
    return memory;
  }
  throw std::bad_alloc();
}
} // namespace
void *operator new(size_t size) {
  return allocate(size);
}
void *operator new[](size_t size) {
  return allocate(size);
}
void operator delete(void *p) noexcept {
  std::free(p);
}
void operator delete[](void *p) noexcept {
  std::free(p);
}
void operator delete(void *p, size_t) noexcept {
  std::free(p);
}
void operator delete[](void *p, size_t) noexcept {
  std::free(p);
}
int main() {
  using namespace codet;
  Json transcript = Json::array();
  for (int i = 0; i < 100; ++i)
    transcript.push_back(
        {{"role", "assistant"}, {"text", std::string(8192, 'x') + "雪"}, {"calls", Json::array()}});
  const Json event = {{"type", "item.updated"},
                      {"item", {{"text_delta", std::string(64, 'x') + "雪"}}}};
  const auto old = [](const Json &value) {
    return Json::parse(value.dump(-1, ' ', false, Json::error_handler_t::replace));
  };
  for (const auto &value : {transcript, event}) {
    if (safe_json(value).dump() != old(value).dump())
      return 1;
    for (bool previous : {true, false}) {
      const int iterations = value.is_array() ? 20 : 20000;
      allocations = 0;
      allocated_bytes = 0;
      const auto began = std::chrono::steady_clock::now();
      size_t bytes = 0;
      for (int i = 0; i < iterations; ++i)
        bytes += (previous ? old(value) : safe_json(value)).dump().size();
      const auto elapsed =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      const auto count = allocations.load(), total = allocated_bytes.load();
      std::cout << Json({{"case",
                          value.is_array() ? "transcript_save_serialization" : "streamed_event"},
                         {"implementation", previous ? "previous_roundtrip" : "utf8_tree"},
                         {"iterations", iterations},
                         {"milliseconds", elapsed},
                         {"allocations", count},
                         {"allocated_bytes", total},
                         {"serialized_bytes", bytes}})
                       .dump()
                << '\n';
    }
  }
  TempDirectory temporary;
  auto store = std::make_shared<CaptureStore>(temporary.root / "output");
  const std::string chunk(8192, 'x');
  allocations = 0;
  allocated_bytes = 0;
  const auto began = std::chrono::steady_clock::now();
  size_t retained_capacity = 0;
  {
    OutputCapture capture(store, 65536);
    for (int i = 0; i < 128; ++i)
      capture.append(chunk);
    capture.finish();
    retained_capacity = capture.memory_bytes();
    if (capture.render(65536).omitted == 0)
      return 1;
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto count = allocations.load(), bytes = allocated_bytes.load();
  std::cout << Json({{"case", "lazy_capture_1MiB_8KiB_chunks"},
                     {"milliseconds", elapsed},
                     {"allocations", count},
                     {"allocated_bytes", bytes},
                     {"retained_capacity", retained_capacity},
                     {"flushes", store->metrics.batches},
                     {"scans", store->metrics.scans},
                     {"syncs", store->metrics.syncs},
                     {"renders", store->metrics.renders}})
                   .dump()
            << '\n';
}
