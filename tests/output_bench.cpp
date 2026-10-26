// Host-only measurements; compile this identical harness against both compared source trees.
#include "codet/core.hpp"
#include "codet/detail/cli_output.hpp"
#include "codet/detail/native_terminal.hpp"
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include "codet/diagnostics.hpp"
#include "descriptor_bench_metrics.hpp"
#include "temp.hpp"
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <new>
#include <poll.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
namespace {
bool measuring = false;
std::atomic<size_t> allocations{0}, allocated{0}, writes{0}, bytes{0}, waits{0};
void *allocate(size_t size) {
  if (auto *memory = std::malloc(size ? size : 1)) {
    if (measuring) {
      ++allocations;
      allocated += size;
    }
    return memory;
  }
  throw std::bad_alloc();
}
// Resolving write itself needs a nonrecursive failure path; native_symbol reports via write.
template <class Function> Function resolve(const char *name) {
  auto function = reinterpret_cast<Function>(::dlsym(RTLD_NEXT, name));
  if (!function)
    ::_exit(86);
  return function;
}
template <class Output> Output production_output(const codet::Cancellation &cancel) {
  if constexpr (std::is_constructible_v<Output, const codet::Cancellation &>)
    return Output(cancel);
  else
    return Output();
}
template <class Output>
auto answer(Output &output, const std::string &text, bool terminal,
            int) -> decltype(output.answer(text, terminal), void()) {
  output.answer(text, terminal);
}
template <class Output> void answer(Output &output, const std::string &text, bool terminal, long) {
  // Exact pre-follow-up production boundary; the new explicit boundary can borrow the string.
  output.required({terminal ? codet::detail::terminal_text(text) : text});
}
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
extern "C" ssize_t write(int fd, const void *data, size_t size) {
  static const auto native = resolve<ssize_t (*)(int, const void *, size_t)>("write");
  const auto result = native(fd, data, size);
  if (measuring && (fd == STDOUT_FILENO || fd == STDERR_FILENO)) {
    ++writes;
    if (result > 0)
      bytes += static_cast<size_t>(result);
  }
  return result;
}
extern "C" ssize_t writev(int fd, const iovec *data, int count) {
  static const auto native = resolve<ssize_t (*)(int, const iovec *, int)>("writev");
  const auto result = native(fd, data, count);
  if (measuring && (fd == STDOUT_FILENO || fd == STDERR_FILENO)) {
    ++writes;
    if (result > 0)
      bytes += static_cast<size_t>(result);
  }
  return result;
}
extern "C" int poll(pollfd *fds, nfds_t count, int timeout) {
  static const auto native = resolve<int (*)(pollfd *, nfds_t, int)>("poll");
  if (measuring && count == 1 && (fds[0].fd == STDOUT_FILENO || fds[0].fd == STDERR_FILENO) && (fds[0].events & POLLOUT))
    ++waits;
  return native(fds, count, timeout);
}
int main(int argc, char **argv) {
  using namespace codet;
  if (argc < 3)
    return 2;
  const std::string mode(argv[1]);
  const bool editor = mode == "editor";
  const bool diagnostic = mode.rfind("diagnostics", 0) == 0;
  const bool piped = mode.rfind("pipe-", 0) == 0 || editor ||
                     (diagnostic && mode.find("pipe") != std::string::npos);
  pid_t drainer = -1;
  if (piped) {
    int pipe[2];
    if (editor ? ::openpty(&pipe[0], &pipe[1], nullptr, nullptr, nullptr) : ::pipe(pipe))
      return 3;
    drainer = ::fork();
    if (drainer < 0)
      return 3;
    if (drainer == 0) {
      ::close(pipe[1]);
      char buffer[65536];
      while (::read(pipe[0], buffer, sizeof(buffer)) > 0) {
      }
      ::_exit(0);
    }
    ::close(pipe[0]);
    if (editor && ::dup2(pipe[1], STDIN_FILENO) < 0)
      return 3;
    if (::dup2(pipe[1], STDOUT_FILENO) < 0)
      return 3;
    ::close(pipe[1]);
  } else {
    const auto sink = ::open("/dev/null", O_WRONLY);
    if (sink < 0 || ::dup2(sink, STDOUT_FILENO) < 0)
      return 3;
    ::close(sink);
  }
  const bool large = mode.find("large") != std::string::npos;
  const bool amplified = mode.find("amplified") != std::string::npos;
  const bool output = mode.rfind("jsonl-", 0) == 0 || (piped && !editor && !diagnostic);
  const size_t repetitions = amplified                  ? 200000
                             : output                   ? (large ? 2000 : 20000)
                             : editor                   ? 20000
                             : mode == "budget-partial" ? 32
                                                        : 128;
  const std::string record = "{\"text\":\"" + std::string(large ? 65536 : 100, 'x') + "\"}";
  std::string input(1024 * 1024, mode == "budget-partial" ? '\x1b' : 'x');
  if (mode == "render-utf8") {
    input.clear();
    while (input.size() < 1024 * 1024)
      input += "Jos\xc3\xa9 \xe9\x9b\xaa \xf0\x9f\x90\xb1";
  }
  DiagnosticBuffer full(DiagnosticBuffer::notice().size() + 3);
  full.add("a");
  detail::NativeTerminalResourceOps native;
  detail::TerminalResources resources(native);
  if (editor) {
    ::setenv("TERM", "xterm", 1);
    resources.start_editor();
    (void)::el_set(resources.editor(), EL_SIGNAL, 0);
    (void)::el_set(resources.editor(), EL_EDITOR, "emacs");
    (void)::el_set(
        resources.editor(), EL_PROMPT, +[](EditLine *) -> const char * {
          return "a representative private libedit prompt with Unicode 雪> ";
        });
  }
  Cancellation cancel;
  auto stdout_output = production_output<detail::CliOutput>(cancel);
  detail::CliOutput uncancelled_output;
  if (diagnostic && ::dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
    return 3;
  detail::IoContext io(cancel);
  detail::DiagnosticWriter diagnostics(std::cerr,
                                      mode.find("standalone") == std::string::npos ? &io : nullptr);
  TempDirectory temporary;
  std::vector<std::string> command;
  if (mode == "answer") {
    if (argc != 4)
      return 2;
    fs::create_directories(temporary.root / "home");
    fs::create_directories(temporary.root / "work");
    ::setenv("CODET_HOME", (temporary.root / "home").c_str(), 1);
    ::setenv("HOME", temporary.root.c_str(), 1);
    std::ofstream config(temporary.root / "home/config.toml");
    config
        << "provider='openai'\nmodel='fixture'\n[providers.openai]\napi_key='fixture'\nbase_url='"
        << argv[3] << "'\n";
    config.close();
    command = {"-C", (temporary.root / "work").string(), "exec", "benchmark answer"};
  }
  allocations = allocated = writes = bytes = waits = 0;
  test::descriptor_bench::stats = test::descriptor_bench::flags = test::descriptor_bench::bounds =
      0;
  size_t observed = 0;
  volatile bool terminal = false;
  measuring = test::descriptor_bench::observing = true;
  const auto started = std::chrono::steady_clock::now();
  if (mode == "answer") {
    if (cli_main(command, cancel))
      return 4;
  } else
    for (size_t i = 0; i != repetitions; ++i) {
      if (editor)
        observed += ::el_set(resources.editor(), EL_REFRESH) == 0;
      else if (mode == "write-positive")
        observed += ::write(STDOUT_FILENO, "x", 1) == 1;
      else if (mode.find("nocancel") != std::string::npos)
        uncancelled_output.required({record});
      else if (output)
        stdout_output.required({record});
      else if (diagnostic)
        observed += diagnostics.write({"ordinary diagnostic"});
      else if (mode == "stat-positive") {
        struct stat status {};
        observed += ::fstat(STDOUT_FILENO, &status) == 0;
      } else if (mode == "flags-positive")
        observed += ::fcntl(STDOUT_FILENO, F_GETFL) >= 0;
      else if (mode == "bounds-positive")
        observed += ::fpathconf(STDOUT_FILENO, _PC_PIPE_BUF) >= 0;
      else if (mode == "answer-boundary")
        answer(stdout_output, input, terminal, 0);
      else if (mode == "budget-full")
        observed += full.add(input).has_value();
      else if (mode == "budget-partial") {
        DiagnosticBuffer buffer(256);
        observed += buffer.add(input)->size();
      } else
        observed += detail::terminal_text(input).size();
    }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  measuring = test::descriptor_bench::observing = false;
  resources.close();
  if (piped) {
    if (editor)
      ::close(STDIN_FILENO);
    ::close(STDOUT_FILENO);
    if (diagnostic)
      ::close(STDERR_FILENO);
    int status;
    if (::waitpid(drainer, &status, 0) < 0 || status)
      return 5;
  }
  const auto measured_allocations = allocations.load(), measured_bytes = allocated.load();
  const auto calls = writes.load(), delivered = bytes.load(), readiness = waits.load();
  if (output && delivered != repetitions * (record.size() + 1))
    return 6;
  std::ofstream report(argv[2]);
  report << Json({{"case", mode},
                  {"milliseconds", elapsed},
                  {"allocations", measured_allocations},
                  {"allocated_bytes", measured_bytes},
                  {"writes", calls},
                  {"direct_native_write_counter", !editor && !diagnostic},
                  {"bytes", delivered},
                  {"readiness", readiness},
                  {"fstat", test::descriptor_bench::stats.load()},
                  {"fcntl", test::descriptor_bench::flags.load()},
                  {"fpathconf", test::descriptor_bench::bounds.load()},
                  {"observed", observed}})
                .dump()
         << '\n';
  return report ? 0 : 7;
}
