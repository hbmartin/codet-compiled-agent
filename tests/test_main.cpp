#include "codet/detail/cli.hpp"
#include "codet/detail/cli_output.hpp"
#include "codet/detail/curses_render.hpp"
#include "codet/detail/native_terminal.hpp"
#include "codet/diagnostics.hpp"
#ifdef CODET_NATIVE_TERMINAL_METRICS
#include "native_symbol.hpp"
#endif
#include "codet/output.hpp"
#include "line_prompt_helper.hpp"
#include "native_io_metrics.hpp"
#include "native_terminal_metrics.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <csignal>
#include <cstdlib>
#include <curses.h>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/resource.h>

std::string codet::test::executable;
namespace {
class MutatingFailureOps final : public codet::detail::TerminalResourceOps {
  codet::detail::NativeTerminalResourceOps native_;

public:
  bool capture(termios &settings) noexcept override {
    return native_.capture(settings);
  }
  bool apply(const termios &settings) noexcept override {
    return native_.apply(settings);
  }
  void restore(const termios &settings) noexcept override {
    native_.restore(settings);
  }
  bool start_screen() override {
    termios settings{};
    if (!capture(settings))
      throw codet::Error("Cannot capture fixture terminal");
    settings.c_lflag ^= ECHO;
    codet::detail::TerminalCleanupSignals signals;
    if (!signals.ready() || !apply(settings))
      throw codet::Error("Cannot mutate fixture terminal");
    return false;
  }
  void stop_screen() noexcept override {
    native_.stop_screen();
  }
  EditLine *start_editor() override {
    return native_.start_editor();
  }
  void stop_editor(EditLine *editor) noexcept override {
    native_.stop_editor(editor);
  }
  History *start_history() override {
    return native_.start_history();
  }
  void stop_history(History *entries) noexcept override {
    native_.stop_history(entries);
  }
};
} // namespace
int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  codet::test::executable = codet::fs::absolute(argv[0]).string();
  try {
    if (argc > 1 && std::string(argv[1]) == "--codet-redraw-helper") {
      if (argc < 6)
        return 2;
      struct Failure {
        int arm, notify;
        bool fired = false;
        bool fail() {
          if (fired)
            return false;
          pollfd descriptor{arm, POLLIN, 0};
          if (::poll(&descriptor, 1, 0) != 1)
            return false;
          char byte = 0;
          if (::read(arm, &byte, 1) != 1)
            return false;
          fired = true;
          (void)::write(notify, "f", 1);
          return true;
        }
      } failure{std::stoi(argv[3]), std::stoi(argv[4])};
      codet::detail::CursesDrawOps ops;
      ops.context = &failure;
      const std::string stage = argv[2];
      if (stage == "clear")
        ops.clear_row =
            +[](void *p) { return static_cast<Failure *>(p)->fail() ? ERR : ::clrtoeol(); };
      else if (stage == "put")
        ops.put_text = +[](void *p, std::string_view text) {
          return static_cast<Failure *>(p)->fail()
                     ? ERR
                     : ::addnstr(text.data(), static_cast<int>(text.size()));
        };
      else if (stage == "refresh")
        ops.refresh_screen =
            +[](void *p) { return static_cast<Failure *>(p)->fail() ? ERR : ::refresh(); };
      else
        return 2;
      codet::Cancellation cancel;
      codet::detail::IoContext io(cancel);
      return codet::detail::cli_main(std::vector<std::string>(argv + 5, argv + argc), cancel, io,
                                     ops);
    }

#ifdef CODET_NATIVE_TERMINAL_METRICS
    if (argc > 1 && std::string(argv[1]) == "--codet-quarantine-warning-helper") {
      if (argc != 3 || ::dup2(std::stoi(argv[2]), STDERR_FILENO) < 0)
        return 2;
      ::setenv("TERM", "codet-invalid-quarantine-fixture", 1);
      codet::detail::NativeTerminalResourceOps ops;
      if (ops.start_screen())
        return 3;
      codet::detail::reset_standard_diagnostics();
      if (::write(STDERR_FILENO, "fi", 2) != 2)
        return 4;
      auto &io = codet::test::native_io_metrics;
      io.interrupt_fd = STDERR_FILENO;
      io.vectors_interrupted = 9;
      const bool failed = !codet::detail::write_diagnostic(std::cerr, {"rst"});
      io = {};
      if (!failed)
        return 5;
      auto &terminal = codet::test::native_terminal_metrics;
      terminal.observe = terminal.detach_failure = terminal.query_failure = true;
      ops.begin_cleanup();
      ops.end_cleanup();
      terminal.observe = false;
      return codet::detail::write_diagnostic(std::cerr, {"later"}) ? 0 : 6;
    }
#endif
    if (argc > 1 && std::string(argv[1]) == "--codet-closed-stderr-terminal-helper") {
      if (argc != 4)
        return 2;
      const std::string mode(argv[2]);
      if (mode != "line" && mode != "curses")
        return 2;
      const int terminal = std::stoi(argv[3]);
      if (::dup2(terminal, STDIN_FILENO) < 0 || ::dup2(terminal, STDOUT_FILENO) < 0)
        return 3;
      ::close(STDERR_FILENO);
      ::setenv("TERM", "xterm", 1);
      {
        codet::detail::NativeTerminalResourceOps ops;
        codet::detail::TerminalResources resources(ops);
        if (mode == "line")
          resources.start_editor();
        else
          resources.start_screen();
        if (::fcntl(STDERR_FILENO, F_GETFD) >= 0)
          return 4;
        resources.close();
        resources.close();
      }
      return ::fcntl(STDERR_FILENO, F_GETFD) < 0 ? 0 : 5;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-private-stream-failure-helper") {
      if (argc != 4)
        return 2;
      const int allowance = std::stoi(argv[2]), terminal = std::stoi(argv[3]);
      if (allowance < 0 || allowance > 3)
        return 2;
      for (int target = 0; target != 3; ++target)
        if (::dup2(terminal, target) < 0)
          return 3;
      if (terminal > 2)
        ::close(terminal);
      int highest = 2;
      for (int fd = 0; fd != 512; ++fd)
        if (::fcntl(fd, F_GETFD) >= 0) {
          highest = fd;
        }
      // Inherited descriptors can leave holes below the highest fd on Linux.
      // Reserve them so each allowance controls the actual number of free slots.
      struct Reservations {
        int descriptors[512]{};
        size_t size = 0;
        ~Reservations() {
          for (size_t i = 0; i < size; ++i)
            ::close(descriptors[i]);
        }
      } reservations;
      for (int fd = 3; fd <= highest; ++fd)
        if (::fcntl(fd, F_GETFD) < 0) {
          const int reserved = ::fcntl(STDIN_FILENO, F_DUPFD, fd);
          if (reserved < 0)
            return 4;
          reservations.descriptors[reservations.size++] = reserved;
          if (reserved != fd)
            return 4;
        }
      rlimit original{};
      if (::getrlimit(RLIMIT_NOFILE, &original))
        return 4;
      auto limited = original;
      limited.rlim_cur = static_cast<rlim_t>(highest + 1 + allowance);
      if (::setrlimit(RLIMIT_NOFILE, &limited))
        return 4;
      struct RestoreLimit {
        rlimit original;
        ~RestoreLimit() {
          (void)::setrlimit(RLIMIT_NOFILE, &original);
        }
      } restore{original};
      int before = 0;
      const int bound = highest + 1 + allowance;
      for (int fd = 0; fd < bound; ++fd)
        before += ::fcntl(fd, F_GETFD) >= 0;
      bool failed = false;
      {
        codet::detail::NativeTerminalResourceOps ops;
        codet::detail::TerminalResources resources(ops);
        try {
          resources.start_editor();
        } catch (const codet::Error &) {
          failed = true;
        }
      }
      if (!failed)
        return 5;
      int after = 0;
      for (int fd = 0; fd < bound; ++fd)
        after += ::fcntl(fd, F_GETFD) >= 0;
      return before == after ? 0 : 6;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-background-close-helper") {
      if (argc != 6)
        return 2;
      const std::string mode(argv[2]);
      if (mode != "line" && mode != "curses" && mode != "partial" && mode != "detach-failure" &&
          mode != "query-failure" && mode != "detach-failure-curses")
        return 2;
      const int gate = std::stoi(argv[3]), notify = std::stoi(argv[4]);
      const int terminal = std::stoi(argv[5]);
      for (int target = 0; target != 3; ++target)
        if (::dup2(terminal, target) < 0)
          return 3;
      char token;
      if (!codet::test::read_token(gate, token))
        return 4;
      const auto descriptors = [] {
        int count = 0;
        for (int fd = 0; fd != 512; ++fd)
          count += ::fcntl(fd, F_GETFD) >= 0;
        return count;
      };
      const auto before = descriptors();
      ::setenv("TERM", "xterm", 1);
      sigset_t prior{}, after{};
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &prior);
      auto &metrics = codet::test::native_terminal_metrics;
      const auto failure = std::make_exception_ptr(std::bad_alloc());
      metrics.observe_streams = true;
      try {
        codet::detail::NativeTerminalResourceOps ops;
        codet::detail::TerminalResources resources(ops);
        if (mode == "curses" || mode == "detach-failure-curses") {
          resources.start_screen();
          (void)::cbreak();
          (void)::noecho();
          (void)::refresh();
        } else {
          resources.start_editor();
          resources.start_history();
          (void)::el_set(resources.editor(), EL_PREP_TERM, 1);
        }
        metrics.observe_streams = false;
        (void)::write(notify, "R", 1);
        if (!codet::test::read_token(gate, token))
          return 5;
        if (mode.rfind("detach-failure", 0) == 0) {
          if (metrics.stream_count != (mode == "detach-failure-curses" ? 2u : 3u))
            return 11;
          // Exercise the production bundle's pre-acquisition 4 KiB buffer through process exit.
          (void)std::fputs("QUARANTINED OUTPUT MUST NEVER REACH THE FOREGROUND",
                           metrics.streams[1]);
#ifdef __linux__
          const auto pending = ::__fpending(metrics.streams[1]);
#else
          const auto pending =
              static_cast<size_t>(metrics.streams[1]->_p - metrics.streams[1]->_bf._base);
#endif
          if (pending < std::strlen("QUARANTINED OUTPUT MUST NEVER REACH THE FOREGROUND"))
            return 12;
        }
        metrics.observe = true;
        metrics.detach_failure = mode.rfind("detach-failure", 0) == 0;
        metrics.query_failure = mode == "query-failure";
        if (mode == "partial" || mode.rfind("detach-failure", 0) == 0)
          std::rethrow_exception(failure);
        resources.close();
        resources.close();
      } catch (...) {
        if ((mode != "partial" && mode.rfind("detach-failure", 0) != 0) ||
            std::current_exception() != failure)
          throw;
      }
      metrics.observe = false;
      if (metrics.live_settings)
        return 6;
      if (mode.rfind("detach-failure", 0) == 0) {
        // A failed detach quarantines FILE-owned descriptors: none can be reused or double-closed.
        const int unrelated = ::open("/dev/null", O_RDWR);
        if (unrelated < 0 || descriptors() <= before)
          return 9;
        bool rejected = false;
        try {
          codet::detail::NativeTerminalResourceOps next;
          (void)next.start_editor();
        } catch (const codet::Error &) {
          rejected = true;
        }
        if (!rejected || ::fcntl(unrelated, F_GETFD) < 0)
          return 10;
        ::close(unrelated);
      } else if (descriptors() != before)
        return 6;
      if (mode == "detach-failure-curses" && (metrics.ends || metrics.deletes))
        return 13;
      if (mode == "curses" &&
          (metrics.ends != 1 || metrics.deletes != 1 || !metrics.ended_before_delete))
        return 7;
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &after);
      for (int signal = 1; signal < NSIG; ++signal)
        if (sigismember(&prior, signal) != sigismember(&after, signal))
          return 8;
      (void)::write(notify, "E", 1);
      return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-native-symbol-helper") {
#ifdef CODET_NATIVE_TERMINAL_METRICS
      if (argc != 3 || ::dup2(std::stoi(argv[2]), STDERR_FILENO) < 0)
        return 2;
      (void)codet::test::native_symbol("codet_missing_native_symbol_fixture");
#endif
      return 3;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-active-job-control-helper") {
      if (argc != 5)
        return 2;
      const int gate = std::stoi(argv[2]), notify = std::stoi(argv[3]);
      const int terminal = std::stoi(argv[4]);
      for (int target = 0; target != 3; ++target)
        if (::dup2(terminal, target) < 0)
          return 3;
      char token;
      if (!codet::test::read_token(gate, token))
        return 4;
      codet::detail::NativeTerminalResourceOps ops;
      codet::detail::TerminalResources resources(ops);
      resources.begin_active();
      (void)::write(notify, "R", 1);
      if (!codet::test::read_token(gate, token))
        return 5;
      sigset_t before{}, after{};
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &before);
      resources.end_active();
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &after);
      if (sigismember(&before, SIGTTOU) != sigismember(&after, SIGTTOU))
        return 6;
      (void)::write(notify, "E", 1);
      return codet::test::read_token(gate, token) ? 0 : 7;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-required-stdout-helper") {
      if (argc != 4)
        return 2;
      static char buffer[8192];
      if (::setvbuf(stdout, buffer, _IOFBF, sizeof(buffer)) != 0)
        return 3;
      codet::detail::CliOutput output;
      try {
        output.required({"failed-required-record"});
        return 4;
      } catch (const codet::Error &) {
      }
      (void)::write(std::stoi(argv[2]), "F", 1);
      char token;
      if (!codet::test::read_token(std::stoi(argv[3]), token))
        return 5;
      output.secondary({"must-not-publish"});
      codet::report_diagnostic({}, "final output failure");
      (void)::fflush(stdout);
      return output.status(0);
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-line-prompt-helper") {
      if (argc != 5) {
        std::fputs("line-prompt helper expects MODE RELEASE_FD NOTIFY_FD\n", stderr);
        return 2;
      }
      return codet::test::line_prompt_helper(argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-line-output-helper") {
      if (argc < 4) {
        std::fputs("line-output helper expects FD CLI_ARGS\n", stderr);
        return 2;
      }
      struct ReplyFailure : std::streambuf {
        std::streambuf *original;
        int notify;
        bool failed = false;
        ReplyFailure(std::streambuf *buffer, int fd) : original(buffer), notify(fd) {}
        std::streamsize xsputn(const char *text, std::streamsize size) override {
          if (!failed && std::string_view(text, static_cast<size_t>(size)) == "fixture done") {
            failed = true;
            (void)::write(notify, "f", 1);
            return 0;
          }
          return original->sputn(text, size);
        }
        int overflow(int character) override {
          return original->sputc(static_cast<char>(character));
        }
        int sync() override {
          return original->pubsync();
        }
      } buffer(std::cout.rdbuf(), std::stoi(argv[2]));
      struct RestoreOutput {
        std::streambuf *original;
        ~RestoreOutput() {
          std::cout.rdbuf(original);
          codet::detail::recover_diagnostic_stream(std::cout);
        }
      } restore{std::cout.rdbuf(&buffer)};
      std::vector<std::string> arguments(argv + 3, argv + argc);
      codet::Cancellation cancel;
      return codet::cli_main(arguments, cancel);
    }
    if (argc > 1 && (std::string(argv[1]) == "--codet-stderr-recovery-helper" ||
                     std::string(argv[1]) == "--codet-stdout-recovery-helper")) {
      const bool use_stdout = std::string(argv[1]) == "--codet-stdout-recovery-helper";
      auto &stream = use_stdout ? std::cout : std::cerr;
      const int target = use_stdout ? STDOUT_FILENO : STDERR_FILENO;
      if (argc != 2) {
        std::fputs("stream-recovery helper accepts no arguments\n", stderr);
        return 2;
      }
      int descriptors[2];
      if (::pipe(descriptors) != 0)
        return 3;
      const int saved = ::dup(target);
      struct Restore {
        int original, read, write, target;
        std::ostream &stream;
        ~Restore() {
          if (original >= 0) {
            (void)::dup2(original, target);
            ::close(original);
          }
          ::close(read);
          ::close(write);
          codet::detail::recover_diagnostic_stream(stream);
        }
      } restore{saved, descriptors[0], descriptors[1], target, stream};
      if (saved < 0 || ::fcntl(descriptors[0], F_SETFL, O_NONBLOCK) < 0 ||
          ::fcntl(descriptors[1], F_SETFL, O_NONBLOCK) < 0)
        return 4;
      char bytes[4096]{};
      while (::write(descriptors[1], bytes, sizeof(bytes)) > 0) {
      }
      if (errno != EAGAIN || ::dup2(descriptors[1], target) < 0)
        return 5;
      bool failed = false;
      try {
        codet::write_file_diagnostic(stream, "blocked notice");
      } catch (const codet::Error &) {
        failed = true;
      }
      const bool recovered = stream.good() && std::ferror(use_stdout ? stdout : stderr) == 0;
      while (::read(descriptors[0], bytes, sizeof(bytes)) > 0) {
      }
      if (use_stdout)
        (void)codet::detail::write_diagnostic(stream, {"later warning"}, true, true);
      else
        codet::report_diagnostic({}, "later warning");
      const auto count = ::read(descriptors[0], bytes, sizeof(bytes));
      const std::string observed = count > 0 ? std::string(bytes, static_cast<size_t>(count)) : "";
      // Buffered stdout may retain bytes from the failed flush; later output must still work.
      const bool delivered =
          use_stdout ? observed.size() >= 14 &&
                           observed.compare(observed.size() - 14, 14, "later warning\n") == 0
                     : observed == "\nlater warning\n";
      return failed && recovered && delivered ? 0 : 6;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-terminal-startup-helper") {
      const std::string phase = argc > 2 ? argv[2] : "";
      if (argc != 4 ||
          (phase != "screen" && phase != "render" && phase != "editor" && phase != "history" &&
           phase != "active" && phase != "unchanged" && phase != "failed")) {
        std::cerr << "terminal-startup helper expects PHASE PTY_FD\n";
        return 2;
      }
      const int descriptor = std::stoi(argv[3]);
      for (int target = 0; target != 3; ++target)
        if (::dup2(descriptor, target) < 0)
          return 3;
      if (descriptor > 2)
        ::close(descriptor);
      ::setenv("TERM", "xterm", 1);
      codet::detail::NativeTerminalResourceOps ops;
      MutatingFailureOps failure_ops;
      sigset_t mask_before{}, mask_after{};
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &mask_before);
      auto &metrics = codet::test::native_terminal_metrics;
      if (phase == "unchanged") {
        termios settings{};
        if (!ops.capture(settings))
          return 5;
        metrics.observe = true;
        ops.restore(settings);
        return metrics.settings == 0 ? 0 : 6;
      }
      const auto original = std::make_exception_ptr(std::bad_alloc());
      try {
        codet::detail::TerminalResources resources(
            phase == "failed" ? static_cast<codet::detail::TerminalResourceOps &>(failure_ops)
                              : ops);
        if (phase == "failed") {
          try {
            resources.start_screen();
            return 9;
          } catch (const codet::Error &error) {
            if (std::string(error.what()) != "Cannot initialize terminal")
              throw;
          }
        } else if (phase == "screen" || phase == "render") {
          resources.start_screen();
          (void)::cbreak();
          (void)::noecho();
          if (phase == "render")
            (void)::refresh();
        } else {
          resources.start_editor();
          if (phase == "history")
            resources.start_history();
          ::el_set(resources.editor(), EL_PREP_TERM, phase == "active" ? 0 : 1);
          if (phase == "active")
            resources.begin_active();
        }
        metrics.observe = true;
        std::rethrow_exception(original);
      } catch (...) {
        if (std::current_exception() != original)
          return 4;
      }
      metrics.observe = false;
      (void)::pthread_sigmask(SIG_SETMASK, nullptr, &mask_after);
      for (int signal = 1; signal < NSIG; ++signal)
        if (sigismember(&mask_before, signal) != sigismember(&mask_after, signal))
          return 7;
#ifdef CODET_NATIVE_TERMINAL_METRICS
      if ((phase == "screen" || phase == "render") &&
          (metrics.ends != 1 || metrics.deletes != 1 || !metrics.ended_before_delete))
        return 8;
#endif
      std::cerr << "startup exception preserved after restoration\n";
      return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-file-notice-terminal-helper") {
      if ((argc != 3 && argc != 4) || (argc == 4 && std::string(argv[3]) != "preview")) {
        std::cerr << "file-notice helper expects PATH [preview]\n";
        return 2;
      }
      char terminal_type[] = "xterm";
      SCREEN *screen = ::newterm(terminal_type, stderr, stdin);
      if (!screen)
        return 2;
      (void)::refresh();
      codet::FileDiagnosticQueue queue;
      auto notice =
          std::make_unique<codet::FileNotice>(argv[2], codet::FileNoticeKind::tool_result);
      notice->confirmed = true;
      queue.submit(std::move(notice));
      codet::DiagnosticBuffer live(256);
      live.add(std::string(256, 'x'));
      if (argc == 4)
        queue.preview([&](const auto &text) {
          (void)::addnstr(text.c_str(), 10);
          (void)::refresh();
          live.add(text);
        });
      else
        queue.preview([](const auto &) { throw std::bad_alloc(); });
      (void)::endwin();
      queue.finish([](const auto &text) { std::cerr << text << '\n'; });
      ::delscreen(screen);
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--codet-startup-wait-helper") {
      struct rlimit limit {};
      if (::getrlimit(RLIMIT_NOFILE, &limit))
        return 2;
      limit.rlim_cur = std::min(limit.rlim_cur, rlim_t{512});
      if (::setrlimit(RLIMIT_NOFILE, &limit))
        return 3;
      codet::NativeFileSystem files;
      { codet::StateStore warm(files, argv[3]); }
      const auto descriptors = [&] {
        size_t count = 0;
        for (int fd = 0; fd < static_cast<int>(limit.rlim_cur); ++fd)
          if (::fcntl(fd, F_GETFD) >= 0)
            ++count;
        return count;
      };
      const bool from_notification = std::string(argv[4]) == "notification";
      codet::Cancellation cancel;
      cancel.requested = !from_notification;
      size_t notifications = 0;
      const auto before = descriptors();
      for (int attempt = 0; attempt < 20; ++attempt) {
        try {
          codet::StateStore store(files, argv[2], {}, "durable",
                                  {&cancel, [&](const auto &message) {
                                     if (message != "Waiting for database migration")
                                       throw codet::Error("unexpected startup diagnostic");
                                     ++notifications;
                                     cancel.requested = true;
                                     throw std::bad_alloc();
                                   }});
          std::cerr << "migration wait was not cancelled\n";
          return 4;
        } catch (const codet::Error &error) {
          if (error.code != 130 ||
              std::string(error.what()) != "Interrupted waiting for database migration") {
            std::cerr << error.what() << '\n';
            return 5;
          }
        }
        if (descriptors() != before) {
          std::cerr << "cancelled constructor leaked descriptors\n";
          return 6;
        }
      }
      if (notifications != (from_notification ? 1u : 0u)) {
        std::cerr << "unexpected startup notification count\n";
        return 7;
      }
      return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--codet-catalog-lock-probe") {
      const int fd = ::open(argv[2], O_RDWR);
      if (fd < 0)
        return 2;
      struct flock lock {};
      lock.l_type = F_WRLCK;
      lock.l_whence = SEEK_SET;
      lock.l_len = 1;
      const auto result = ::fcntl(fd, F_SETLK, &lock);
      const auto error = errno;
      ::close(fd);
      return result == 0 ? 0 : (error == EACCES || error == EAGAIN ? 1 : 2);
    }
    if (argc == 5 && std::string(argv[1]) == "--codet-capture-helper") {
      codet::OutputSpool active(argv[2], 8, 8, 64);
      active.append("123456");
      active.flush();
      char token = active.bytes() == 6 ? 'y' : 'n';
      if (::write(std::stoi(argv[3]), &token, 1) != 1)
        return 1;
      if (!codet::test::read_token(std::stoi(argv[4]), token))
        return 1;
      active.close();
      return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--codet-crash-helper") {
      codet::StateStore store(argv[2], {}, argv[4]);
      auto session = store.load_id(argv[3], false);
      codet::Message assistant;
      assistant.role = "assistant";
      assistant.calls = {{"effect", "read_file", {{"path", "x"}}}};
      session.messages.push_back(assistant);
      store.append_messages(session, 0);
      store.tool_started(session.id, assistant.calls[0]);
      if (std::string(argv[5]) == "complete") {
        codet::Message result;
        result.role = "tool";
        result.tool_call_id = "effect";
        result.text = "completed once";
        store.complete_tool(session, assistant.calls[0], result);
      }
      ::_exit(0);
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  TempDirectory private_home;
  ::setenv("HOME", private_home.root.c_str(), 1);
  ::setenv("SHELL", "/bin/sh", 1);
  for (const auto *name :
       {"BASH_ENV", "ENV", "ZDOTDIR", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "GOOGLE_API_KEY",
        "GOOGLE_GENERATIVE_AI_API_KEY", "OLLAMA_API_KEY", "SEARXNG_URL", "HTTP_PROXY",
        "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"})
    ::unsetenv(name);
  ::setenv("NO_PROXY", "127.0.0.1,localhost", 1);
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
