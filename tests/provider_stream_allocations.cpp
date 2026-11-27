#include "codet/core.hpp"
#include "temp.hpp"
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool measuring = false;
thread_local size_t allocated = 0;

void *allocate(size_t size) {
  if (measuring)
    allocated += size;
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

namespace {
using namespace codet;
std::string frame(const Json &value) {
  return "data: " + value.dump() + "\n\n";
}

struct FixtureStream final : HttpTransport {
  size_t fragments;
  std::vector<std::string> initial, deltas;
  const std::string stop = frame({{"type", "message_stop"}});

  FixtureStream(size_t count, size_t bytes) : fragments(count) {
    initial.push_back(frame({{"type", "content_block_start"},
                             {"index", 0},
                             {"content_block", {{"type", "thinking"}, {"thinking", "initial "}}}}));
    initial.push_back(frame({{"type", "content_block_start"},
                             {"index", 1},
                             {"content_block", {{"type", "text"}, {"text", ""}}}}));
    for (const std::string field : {"thinking", "signature", "text"})
      deltas.push_back(frame(
          {{"type", "content_block_delta"},
           {"index", field == "text" ? 1 : 0},
           {"delta", {{"type", field + "_delta"}, {field, std::string(bytes, field.front())}}}}));
  }
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &sink) override {
    for (const auto &event : initial)
      sink(event);
    for (size_t i = 0; i < fragments; ++i)
      for (const auto &event : deltas)
        sink(event);
    sink(stop);
    return {200, ""};
  }
};
struct Sample {
  size_t bytes;
  Message message;
};
Sample run(const Config &config, CredentialStore &credentials, Clock &clock, size_t count,
           size_t bytes) {
  FixtureStream stream(count, bytes);
  ApiProvider provider(stream, credentials, clock);
  std::string streamed;
  allocated = 0;
  measuring = true;
  auto result = provider.complete(config, "", {}, Json::array(), {},
                                  [&](const std::string &s) { streamed += s; });
  measuring = false;
  if (streamed != result.message.text || result.message.text != std::string(count * bytes, 't') ||
      result.message.replay.at(0).at("thinking") != "initial " + std::string(count * bytes, 't') ||
      result.message.replay.at(0).at("signature") != std::string(count * bytes, 's') ||
      result.message.replay.at(1).at("text") != result.message.text)
    throw Error("Anthropic initial content, visible text, or signed replay changed");
  return {allocated, std::move(result.message)};
}
} // namespace

int main() {
  TempDirectory temp;
  codet::NativeFileSystem files;
  codet::SystemClock clock;
  codet::Config config;
  config.home = temp.root;
  config.model = "fixture-model";
  config.provider = "anthropic";
  config.providers["anthropic"].api_key = "fixture";
  codet::CredentialStore credentials(config.home, files, clock);
  const auto small = run(config, credentials, clock, 256, 96);
  const auto large = run(config, credentials, clock, 1024, 96);
  const auto single = run(config, credentials, clock, 1, 1024 * 96);
  if (large.message.replay != single.message.replay || large.message.text != single.message.text)
    return 1;
  // Four times as many fragments allow fixed costs and allocator growth. Recopying each
  // accumulated block on every delta instead grows approximately sixteenfold.
  std::cout << "Allocated " << small.bytes << " and " << large.bytes << " bytes\n";
  return small.bytes > 0 && large.bytes < 7 * small.bytes ? 0 : 1;
}
