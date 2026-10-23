#include "codet/detail/native_terminal.hpp"
#include "codet/diagnostics.hpp"
#include "codet/output.hpp"
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
int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  codet::test::executable = codet::fs::absolute(argv[0]).string();
  try {
    if (argc > 1 && std::string(argv[1]) == "--codet-stderr-recovery-helper") {
      if (argc != 2) {
        std::fputs("stderr-recovery helper accepts no arguments\n", stderr);
        return 2;
      }
      int descriptors[2];
      if (::pipe(descriptors) != 0)
        return 3;
      const int saved = ::dup(STDERR_FILENO);
      struct Restore {
        int original, read, write;
        ~Restore() {
          if (original >= 0) {
            (void)::dup2(original, STDERR_FILENO);
            ::close(original);
          }
          ::close(read);
          ::close(write);
          codet::detail::recover_diagnostic_stream(std::cerr);
        }
      } restore{saved, descriptors[0], descriptors[1]};
      if (saved < 0 || ::fcntl(descriptors[0], F_SETFL, O_NONBLOCK) < 0 ||
          ::fcntl(descriptors[1], F_SETFL, O_NONBLOCK) < 0)
        return 4;
      char bytes[4096]{};
      while (::write(descriptors[1], bytes, sizeof(bytes)) > 0) {
      }
      if (errno != EAGAIN || ::dup2(descriptors[1], STDERR_FILENO) < 0)
        return 5;
      bool failed = false;
      try {
        codet::write_file_diagnostic(std::cerr, "blocked notice");
      } catch (const codet::Error &) {
        failed = true;
      }
      const bool recovered = std::cerr.good() && std::ferror(stderr) == 0;
      while (::read(descriptors[0], bytes, sizeof(bytes)) > 0) {
      }
      codet::report_diagnostic({}, "later warning");
      const auto count = ::read(descriptors[0], bytes, sizeof(bytes));
      const bool delivered =
          count == 14 && std::string(bytes, static_cast<size_t>(count)) == "later warning\n";
      return failed && recovered && delivered ? 0 : 6;
    }
    if (argc > 1 && std::string(argv[1]) == "--codet-terminal-startup-helper") {
      const std::string phase = argc > 2 ? argv[2] : "";
      if (argc != 4 || (phase != "screen" && phase != "render" && phase != "editor" &&
                        phase != "history" && phase != "active")) {
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
      const auto original = std::make_exception_ptr(std::bad_alloc());
      try {
        codet::detail::TerminalResources resources(ops);
        if (phase == "screen" || phase == "render") {
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
        std::rethrow_exception(original);
      } catch (...) {
        if (std::current_exception() != original)
          return 4;
      }
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
