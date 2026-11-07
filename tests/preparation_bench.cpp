// Shared host harness; cli_prepare_adapter.hpp contains each verified variant's CLI lambda.
#include "cli_prepare_adapter.hpp"
#include "temp.hpp"
#include <cstdlib>
#include <fstream>
#include <new>

namespace {
bool observing = false;
size_t allocations = 0, allocated = 0, large_allocations = 0;
void *allocate(size_t size) {
  if (observing) {
    ++allocations;
    allocated += size;
    large_allocations += size >= 32768;
  }
  if (void *value = std::malloc(size ? size : 1))
    return value;
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
int main(int argc, char **argv) {
  using namespace codet;
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  TempDirectory temporary;
  NativeFileSystem files;
  Config config;
  config.cwd = temporary.root;
  config.home = temporary.root / "home";
  fs::create_directories(config.home / "commands");
  files.atomic_write(config.home / "commands/review.md", "expanded $ARGUMENTS");
  Interaction queue;
  queue.begin_turn("s");
  if (mode == "saturated-count") {
    for (unsigned n = 0; n != 100; ++n)
      (void)detail::SubmissionAccess::reserve(queue);
  } else if (mode == "saturated-effective" || mode == "saturated-retained") {
    const unsigned entries = mode == "saturated-effective" ? 32 : 1;
    for (unsigned n = 0; n != entries; ++n) {
      const auto ticket = detail::SubmissionAccess::reserve(queue);
      detail::SubmissionAccess::charge(queue, ticket, mode == "saturated-effective" ? 262144 : 0,
                                       mode == "saturated-retained" ? 8388608 : 0);
    }
  }
  const std::string raw(65536, static_cast<char>(0x80));
  size_t rejections = 0, prepared = 0, wrappers = 0, content_bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  observing = true;
  for (unsigned n = 0; n != 128; ++n) {
    if (mode == "allocation-positive") {
      auto *value = ::operator new(65536);
      ::operator delete(value);
    } else if (mode.compare(0, 10, "saturated-") == 0) {
      try {
        queue.submit(raw);
      } catch (const Error &) {
        ++rejections;
      }
    } else if (mode == "cli-preparation") {
      detail::InputLine line{"/review once"};
      line.preparation_context = detail::PreparationContext::pending;
      line.admission = detail::SubmissionAccess::reserve(queue);
      line.reservation = detail::SubmissionLease(&queue, line.admission);
      const auto result = codet::test::cli_prepare(config, files, line, queue, Cancellation{});
      if (result.state != detail::PreparationState::ready || !result.submission || !result.prompt)
        return 3;
      ++prepared;
      wrappers += 1 + (result.submission->execution().get() != result.prompt.get());
      content_bytes += result.submission->effective_size();
    } else if (mode == "compatibility-preparation") {
      queue.submit(raw);
      const auto input = queue.take_input();
      if (input.size() != 1 || input.front().size() != 196608)
        return 4;
      ++prepared;
      content_bytes += input.front().size();
    } else if (mode == "normalization") {
      auto normalized = detail::normalized_input(raw, 262144);
      if (normalized.size() != 196608)
        return 5;
      content_bytes += normalized.size();
    } else {
      return 6;
    }
  }
  observing = false;
  const auto duration =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::ofstream(argv[2]) << Json({{"case", mode},
                                  {"preparation_metric_version", 1},
                                  {"iterations", 128},
                                  {"allocations", allocations},
                                  {"allocated_bytes", allocated},
                                  {"payload_sized_allocations", large_allocations},
                                  {"rejections", rejections},
                                  {"prepared_submissions", prepared},
                                  {"distinct_command_wrappers", wrappers},
                                  {"effective_content_bytes", content_bytes},
                                  {"milliseconds", duration}})
                                .dump(2);
}
