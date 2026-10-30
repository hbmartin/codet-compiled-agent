// Identical host-only harness for fetched main and this follow-up. No production counters.
#include "codet/detail/line_prompt.hpp"
#include "codet/interaction.hpp"
#include "descriptor_bench_metrics.hpp"
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include <atomic>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <new>
#include <sys/ioctl.h>
#include <sys/wait.h>
namespace {
bool observing = false;
size_t allocations = 0, allocated = 0, operations = 0, bytes = 0, geometry = 0;
void *allocate(size_t size) {
  if (observing) {
    ++allocations;
    allocated += size;
  }
  if (auto *p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
struct Discard : std::streambuf {
  std::streamsize xsputn(const char *, std::streamsize n) override {
    if (observing) {
      ++operations;
      bytes += static_cast<size_t>(n);
    }
    return n;
  }
  int overflow(int value) override {
    if (observing) {
      ++operations;
      ++bytes;
    }
    return value;
  }
};
template <class Prompt, class... Args>
auto prompt(int, size_t columns, Args &&...args)
    -> std::enable_if_t<std::is_constructible_v<Prompt, Args..., size_t>, std::unique_ptr<Prompt>> {
  return std::make_unique<Prompt>(std::forward<Args>(args)..., columns);
}
template <class Prompt, class... Args> auto prompt(long, size_t, Args &&...args) {
  return std::make_unique<Prompt>(std::forward<Args>(args)...);
}
template <class Prompt>
auto before(Prompt &value, const struct stat &tty, codet::detail::CliOutput &output,
            int) -> decltype(value.before_output(STDOUT_FILENO, tty), void()) {
  if (value.before_output(STDOUT_FILENO, tty))
    output.echo({"\n"});
}
template <class Prompt>
void before(Prompt &value, const struct stat &tty, codet::detail::CliOutput &, long) {
  value.before_output(tty);
}
template <class Interaction, class Consume>
auto drain_new(Interaction &value, Consume consume,
               int) -> decltype(value.drain_warnings(consume, false), void()) {
  value.drain_warnings(consume, false);
}
template <class Interaction, class Consume>
void drain_new(Interaction &value, Consume consume, long) {
  value.drain_warnings(consume);
}
} // namespace
void *operator new(size_t n) {
  return allocate(n);
}
void *operator new[](size_t n) {
  return allocate(n);
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
extern "C" ssize_t write(int fd, const void *p, size_t n) {
  static auto native =
      reinterpret_cast<ssize_t (*)(int, const void *, size_t)>(codet::test::native_symbol("write"));
  auto result = native(fd, p, n);
  if (observing && fd == STDOUT_FILENO) {
    ++operations;
    if (result > 0)
      bytes += static_cast<size_t>(result);
  }
  return result;
}
extern "C" ssize_t writev(int fd, const iovec *p, int n) {
  static auto native =
      reinterpret_cast<ssize_t (*)(int, const iovec *, int)>(codet::test::native_symbol("writev"));
  auto result = native(fd, p, n);
  if (observing && fd == STDOUT_FILENO) {
    ++operations;
    if (result > 0)
      bytes += static_cast<size_t>(result);
  }
  return result;
}
extern "C" int ioctl(int fd, unsigned long request, ...)
#ifdef __linux__
    noexcept
#endif
{
  // This harness only acquires a PTY and queries its width. No native editor is initialized.
  if (request != TIOCGWINSZ && request != TIOCSWINSZ)
    ::_exit(86);
  static auto native =
      reinterpret_cast<int (*)(int, unsigned long, ...)>(codet::test::native_symbol("ioctl"));
  va_list args;
  va_start(args, request);
  auto *value = va_arg(args, void *);
  va_end(args);
  if (observing && request == TIOCGWINSZ)
    ++geometry;
  return native(fd, request, value);
}
int main(int argc, char **argv) {
  using namespace codet;
  if (argc != 3)
    return 2;
  (void)std::setlocale(LC_ALL, "C");
  const std::string mode = argv[1];
  const bool native =
      mode == "native-busy" || mode == "native-editing" || mode == "native-boundary";
  const bool warnings =
      mode == "warning-empty" || mode == "warning-new" || mode == "warning-failed-idle";
  const bool deferred = mode == "deferred-tail";
  const bool editing = mode == "editing" || mode == "editing-basic" ||
                       mode == "editing-patterned" || mode == "native-editing";
  int child = -1;
  struct stat tty {};
  tty.st_mode = S_IFCHR;
  if (native) {
    int master, slave;
    if (::openpty(&master, &slave, nullptr, nullptr, nullptr) < 0)
      return 3;
    struct winsize size {};
    size.ws_row = 24;
    size.ws_col = 80;
    if (::ioctl(slave, TIOCSWINSZ, &size) < 0)
      return 3;
    child = ::fork();
    if (child < 0)
      return 3;
    if (!child) {
      ::close(slave);
      char data[65536];
      while (::read(master, data, sizeof(data)) > 0) {
      };
      ::_exit(0);
    }
    ::close(master);
    if (::dup2(slave, STDOUT_FILENO) < 0 || ::fstat(STDOUT_FILENO, &tty) < 0)
      return 3;
    ::close(slave);
    if (mode == "native-boundary" && ::dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
      return 3;
  }
  Cancellation cancel;
  detail::IoContext io(cancel);
  Discard discard;
  std::ostream stream(&discard);
  detail::CliOutput injected(stream), terminal(io);
  auto &output = native ? terminal : injected;
  detail::RequestInputController input;
  if (!editing && !deferred && !warnings && mode != "allocation-positive" &&
      mode != "geometry-positive")
    for (size_t i = 0; i < (mode == "unchanged" ? 65536u : 20480u); ++i)
      input.append('x');
  const std::string label = "codet> ";
  auto line = prompt<detail::LinePrompt>(0, mode == "editing-basic" || deferred ? 0u : 80u, input,
                                         output, io, label, tty);
  line->acquire();
  line->flush();
  const auto initial = input.input().rebuilt_bytes();
  Interaction interaction;
  size_t warning_attempts = 0;
  const auto consume = [&](std::string_view) {
    ++warning_attempts;
    return mode != "warning-failed-idle";
  };
  if (mode == "warning-failed-idle")
    interaction.warn(Interaction::Warning::exec_stop);
  const size_t count = warnings              ? 100000
                       : editing             ? 20480
                       : mode == "unchanged" ? 10000
                       : native              ? 100
                                             : 1000;
  allocations = allocated = operations = bytes = geometry = 0;
  test::descriptor_bench::stats = test::descriptor_bench::flags = test::descriptor_bench::bounds =
      0;
  observing = test::descriptor_bench::observing = true;
  const auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < count; ++i) {
    if (warnings) {
      if (mode == "warning-new")
        interaction.warn(Interaction::Warning::exec_stop);
      drain_new(interaction, consume, 0);
    } else if (deferred) {
      line->append('a');
      line->append(static_cast<char>(0xe9));
      if (line->take().text != std::string("a\xe9"))
        return 6;
    } else if (mode == "native-boundary") {
      (void)detail::write_diagnostic(std::cerr, {"progress"});
      line->append('x');
    } else if (mode == "allocation-positive") {
      auto *p = ::operator new(32);
      ::operator delete(p);
    } else if (mode == "geometry-positive") {
      struct winsize value {};
      (void)::ioctl(STDOUT_FILENO, TIOCGWINSZ, &value);
    } else if (editing)
      line->append(mode == "editing-patterned" ? static_cast<char>('a' + i % 26) : 'x');
    else if (mode == "deletion")
      line->erase_character();
    else if (mode == "unchanged")
      line->flush();
    else {
      if (!native)
        before(*line, tty, output, 0);
      (void)output.display({"progress"});
      line->flush();
    }
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  observing = test::descriptor_bench::observing = false;
  if (native) {
    if (mode == "native-boundary")
      ::close(STDERR_FILENO);
    ::close(STDOUT_FILENO);
    int status;
    if (::waitpid(child, &status, 0) < 0 || status)
      return 4;
  }
  std::ofstream report(argv[2]);
  report << Json({{"case", mode},
                  {"milliseconds", elapsed},
                  {"allocations", allocations},
                  {"allocated_bytes", allocated},
                  {"output_operations", operations},
                  {"bytes", bytes},
                  {"geometry_queries", geometry},
                  {"fstat", test::descriptor_bench::stats.load()},
                  {"fcntl", test::descriptor_bench::flags.load()},
                  {"fpathconf", test::descriptor_bench::bounds.load()},
                  {"prepared_input_bytes", input.input().rebuilt_bytes() - initial},
                  {"warning_attempts", warning_attempts},
                  {"iterations", count}})
                .dump()
         << '\n';
  return report ? 0 : 5;
}
