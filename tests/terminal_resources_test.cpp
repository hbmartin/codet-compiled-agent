#include "codet/detail/terminal_resources.hpp"
#include "process_helper.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <poll.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
using namespace codet;
namespace {
struct Resources : detail::TerminalResourceOps {
  char calls[32]{};
  size_t used = 0;
  bool captured = true, screen = true, editor = true, entries = true;
  void record(char call) noexcept {
    calls[used++] = call;
  }
  bool capture(termios &) noexcept override {
    record('c');
    return captured;
  }
  void restore(const termios &) noexcept override {
    record('t');
  }
  bool start_screen() override {
    record('s');
    return screen;
  }
  void stop_screen() noexcept override {
    record('S');
  }
  EditLine *start_editor() override {
    record('e');
    return editor ? reinterpret_cast<EditLine *>(this) : nullptr;
  }
  void stop_editor(EditLine *) noexcept override {
    record('E');
  }
  History *start_history() override {
    record('h');
    return entries ? reinterpret_cast<History *>(this) : nullptr;
  }
  void stop_history(History *) noexcept override {
    record('H');
  }
};
TEST(TerminalResources, FailedEnclosingConstructionClosesEachAcquiredResourceInOrder) {
  const auto original = std::make_exception_ptr(std::bad_alloc());
  struct Startup {
    detail::TerminalResources resources;
    Startup(Resources &ops, int phase, const std::exception_ptr &failure) : resources(ops) {
      if (phase == 0)
        resources.start_screen();
      else {
        resources.start_editor();
        if (phase == 2)
          resources.start_history();
      }
      std::rethrow_exception(failure);
    }
  };
  for (int phase = 0; phase != 3; ++phase) {
    Resources ops;
    try {
      Startup startup(ops, phase, original);
      FAIL() << "startup must throw";
    } catch (...) {
      EXPECT_EQ(std::current_exception(), original);
    }
    EXPECT_STREQ(ops.calls, phase == 0 ? "csSt" : phase == 1 ? "ceEt" : "cehEHt");
  }
}
TEST(TerminalResources, FailedAcquisitionsAndRepeatedCloseReleaseOnlyOwnedResources) {
  for (bool screen : {false, true}) {
    Resources ops;
    ops.screen = ops.editor = false;
    {
      detail::TerminalResources resources(ops);
      if (screen)
        EXPECT_THROW(resources.start_screen(), Error);
      else
        EXPECT_THROW(resources.start_editor(), Error);
      resources.close();
      resources.close();
    }
    EXPECT_STREQ(ops.calls, screen ? "cst" : "cet");
  }
  Resources ops;
  ops.captured = ops.entries = false;
  {
    detail::TerminalResources resources(ops);
    resources.start_editor();
    resources.start_history();
    resources.close();
  }
  EXPECT_STREQ(ops.calls, "cehE");
}
TEST(TerminalResources, StartupExceptionsRestoreRealPtyModesBeforeReporting) {
  for (const auto *phase : {"screen", "render", "editor", "history"}) {
    int master = -1, slave = -1;
    winsize size{24, 80, 0, 0};
    ASSERT_EQ(::openpty(&master, &slave, nullptr, nullptr, &size), 0);
    struct Close {
      int master, slave;
      ~Close() {
        ::close(master);
        ::close(slave);
      }
    } close{master, slave};
    termios before{}, after{};
    ASSERT_EQ(::tcgetattr(slave, &before), 0);
    const auto child = test::start_helper(
        {"--codet-terminal-startup-helper", phase, std::to_string(slave)}, {master});
    ASSERT_GT(child, 0);
    int status = 0;
    bool waited = false;
    std::atomic<bool> done{false};
    std::thread waiter([&] {
      waited = test::wait_helper(child, status);
      done = true;
    });
    // endwin/libedit can drain the PTY while restoring it. Drain concurrently with the
    // existing bounded helper wait rather than waiting for exit before reading.
    std::string output;
    for (;;) {
      pollfd ready{master, POLLIN, 0};
      if (::poll(&ready, 1, done ? 0 : 20) > 0) {
        char bytes[4096];
        const auto count = ::read(master, bytes, sizeof(bytes));
        if (count > 0)
          output.append(bytes, static_cast<size_t>(count));
      } else if (done)
        break;
    }
    waiter.join();
    ASSERT_TRUE(waited) << phase;
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0) << phase;
    ASSERT_EQ(::tcgetattr(slave, &after), 0);
#ifdef PENDIN
    before.c_lflag &= ~static_cast<tcflag_t>(PENDIN);
    after.c_lflag &= ~static_cast<tcflag_t>(PENDIN);
#endif
    EXPECT_EQ(after.c_iflag, before.c_iflag) << phase;
    EXPECT_EQ(after.c_oflag, before.c_oflag) << phase;
    EXPECT_EQ(after.c_cflag, before.c_cflag) << phase;
    EXPECT_EQ(after.c_lflag, before.c_lflag) << phase;
    EXPECT_EQ(std::memcmp(after.c_cc, before.c_cc, sizeof(before.c_cc)), 0) << phase;
    EXPECT_NE(output.find("startup exception preserved after restoration"), std::string::npos)
        << phase;
  }
}
} // namespace
