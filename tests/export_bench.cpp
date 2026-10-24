// Host-only end-to-end export measurement; use this identical harness in both builds.
#include "codet/core.hpp"
#include "temp.hpp"
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <new>
#include <unistd.h>
namespace {
bool measuring = false;
std::atomic<size_t> allocations{0}, allocated_bytes{0}, descriptor_writes{0}, descriptor_bytes{0};
void *allocate(size_t size) {
  if (auto *memory = std::malloc(size ? size : 1)) {
    if (measuring) {
      ++allocations;
      allocated_bytes += size;
    }
    return memory;
  }
  throw std::bad_alloc();
}
struct Output : std::streambuf {
  std::streambuf *original;
  size_t writes = 0, bytes = 0, flushes = 0;
  explicit Output(std::streambuf *buffer) : original(buffer) {}
  std::streamsize xsputn(const char *text, std::streamsize count) override {
    if (measuring) {
      ++writes;
      bytes += static_cast<size_t>(count);
    }
    return original->sputn(text, count);
  }
  int overflow(int character) override {
    if (traits_type::eq_int_type(character, traits_type::eof()))
      return traits_type::not_eof(character);
    if (measuring)
      ++bytes;
    return original->sputc(static_cast<char>(character));
  }
  int sync() override {
    if (measuring)
      ++flushes;
    return original->pubsync();
  }
};
} // namespace
void *operator new(size_t size) {
  return allocate(size);
}
void *operator new[](size_t size) {
  return allocate(size);
}
void operator delete(void *memory) noexcept {
  std::free(memory);
}
void operator delete[](void *memory) noexcept {
  std::free(memory);
}
void operator delete(void *memory, size_t) noexcept {
  std::free(memory);
}
void operator delete[](void *memory, size_t) noexcept {
  std::free(memory);
}
extern "C" ssize_t write(int fd, const void *bytes, size_t count) {
  using Write = ssize_t (*)(int, const void *, size_t);
  static const auto native = reinterpret_cast<Write>(::dlsym(RTLD_NEXT, "write"));
  if (!native)
    ::_exit(86);
  const auto written = native(fd, bytes, count);
  if (measuring && fd == STDOUT_FILENO) {
    ++descriptor_writes;
    if (written > 0)
      descriptor_bytes += static_cast<size_t>(written);
  }
  return written;
}
int main(int argc, char **argv) {
  using namespace codet;
  if (argc != 3 || (std::string(argv[1]) != "small" && std::string(argv[1]) != "large"))
    return 2;
  const bool large = std::string(argv[1]) == "large";
  const size_t messages = large ? 10000 : 100, bytes = large ? 1024 : 256;
  TempDirectory temporary;
  const auto home = temporary.root / "home", work = temporary.root / "work";
  fs::create_directories(home);
  fs::create_directories(work);
  ::setenv("CODET_HOME", home.c_str(), 1);
  std::string id;
  {
    StateStore store(home);
    auto session = store.create();
    session.messages.reserve(messages);
    for (size_t i = 0; i != messages; ++i) {
      Message message;
      message.role = "user";
      message.text.assign(bytes, 'x');
      session.messages.push_back(std::move(message));
    }
    store.save(session);
    id = session.id;
  }
  const int sink = ::open("/dev/null", O_WRONLY);
  if (sink < 0 || ::dup2(sink, STDOUT_FILENO) < 0)
    return 3;
  ::close(sink);
  Output output(std::cout.rdbuf());
  auto *original = std::cout.rdbuf(&output);
  Cancellation cancel;
  const std::vector<std::string> command{"-C", work.string(), "export", id};
  allocations = allocated_bytes = descriptor_writes = descriptor_bytes = 0;
  measuring = true;
  const auto start = std::chrono::steady_clock::now();
  const int status = cli_main(command, cancel);
  const auto milliseconds =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  measuring = false;
  const auto count = allocations.load(), allocated = allocated_bytes.load();
  const auto calls = descriptor_writes.load(), delivered = descriptor_bytes.load();
  std::cout.rdbuf(original);
  const auto expected = messages * (bytes + 8);
  if (status || (output.bytes != expected && delivered != expected))
    return 4;
  std::ofstream report(argv[2]);
  report << Json({{"case", large ? "large" : "small"},
                  {"messages", messages},
                  {"milliseconds", milliseconds},
                  {"allocations", count},
                  {"allocated_bytes", allocated},
                  {"stream_writes", output.writes},
                  {"stream_flushes", output.flushes},
                  {"stream_bytes", output.bytes},
                  {"descriptor_writes", calls},
                  {"descriptor_bytes", delivered},
                  {"expected_bytes", expected}})
                .dump()
         << '\n';
  return report ? 0 : 5;
}
