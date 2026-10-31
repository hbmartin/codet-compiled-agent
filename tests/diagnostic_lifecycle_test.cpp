#include "codet/detail/line_prompt.hpp"
#include "codet/diagnostics.hpp"
#include "native_io_metrics.hpp"
#include <condition_variable>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

using namespace codet;
using namespace testing;
namespace {
class NativeRecordLifecycle : public Test {
public:
  int master = -1, slave = -1, saved_out = -1, saved_error = -1;
  Cancellation cancel;
  detail::IoContext io{cancel};
  detail::CliOutput output{io};
  detail::RequestInputController input;
  std::string prompt = "codet> ";
  struct stat identity {};
  std::optional<detail::LinePrompt> line;
  void SetUp() override {
    struct winsize size {};
    size.ws_row = 24;
    size.ws_col = 80;
    ASSERT_EQ(::openpty(&master, &slave, nullptr, nullptr, &size), 0);
    saved_out = ::dup(STDOUT_FILENO);
    saved_error = ::dup(STDERR_FILENO);
    ASSERT_GE(saved_out, 0);
    ASSERT_GE(saved_error, 0);
    ASSERT_EQ(::dup2(slave, STDOUT_FILENO), STDOUT_FILENO);
    ASSERT_EQ(::dup2(slave, STDERR_FILENO), STDERR_FILENO);
    ASSERT_EQ(::fstat(STDOUT_FILENO, &identity), 0);
    ASSERT_EQ(::fcntl(master, F_SETFL, O_NONBLOCK), 0);
    detail::reset_standard_diagnostics();
    line.emplace(input, output, io, prompt, identity);
    line->acquire();
  }
  void TearDown() override {
    test::native_io_metrics = {};
    line.reset();
    if (saved_out >= 0)
      (void)::dup2(saved_out, STDOUT_FILENO);
    if (saved_error >= 0)
      (void)::dup2(saved_error, STDERR_FILENO);
    for (int fd : {master, slave, saved_out, saved_error})
      if (fd >= 0)
        ::close(fd);
    detail::reset_standard_diagnostics();
  }
  std::string read() {
    std::string result;
    char bytes[4096];
    ssize_t count;
    while ((count = ::read(master, bytes, sizeof(bytes))) > 0)
      result.append(bytes, static_cast<size_t>(count));
    return result;
  }
  void draft() {
    for (char byte : std::string("abc"))
      line->append(byte);
  }
};
TEST_F(NativeRecordLifecycle, FailedPromptDoesNotGenerateIdleRetryEvents) {
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  ::close(descriptors[0]);
  const auto replaced = ::dup2(descriptors[1], STDOUT_FILENO);
  ::close(descriptors[1]);
  ASSERT_EQ(replaced, STDOUT_FILENO);
  line->append('a');
  const auto generation = detail::native_line_destinations().snapshot(1, identity).generation;
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDOUT_FILENO;
  for (unsigned i = 0; i != 20; ++i)
    line->flush();
  EXPECT_EQ(metrics.inspections, 0u);
  EXPECT_EQ(metrics.writes + metrics.vectors, 0u);
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).generation, generation);
  line->append('b');
  EXPECT_GT(metrics.inspections, 0u);
  EXPECT_EQ(input.input().raw(), "ab");
}
TEST_F(NativeRecordLifecycle, PromptWaitsForCompletionAndRecoversWithoutAnotherKey) {
  draft();
  EXPECT_EQ(read(), "codet> abc");
  struct Gate {
    NativeRecordLifecycle *fixture;
    static void run(void *context) {
      auto &f = *static_cast<Gate *>(context)->fixture;
      EXPECT_EQ(detail::native_line_destinations().snapshot(1, f.identity).active, 1u);
      f.line->flush();
      EXPECT_TRUE(f.read().empty());
      EXPECT_FALSE(f.output.failed());
    }
  } gate{this};
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.before_output = Gate::run;
  metrics.output_context = &gate;
  EXPECT_TRUE(detail::immediate_stderr({"warning"}));
  metrics = {};
  EXPECT_EQ(read(), "\r\nwarning\r\n");
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
  line->flush();
  EXPECT_EQ(read(), "codet> abc");
  line->flush();
  EXPECT_TRUE(read().empty());
  EXPECT_EQ(input.input().raw(), "abc");
}
TEST_F(NativeRecordLifecycle, ConcurrentDeliveryDefersBothSnapshotAndAdmissionRaces) {
  for (bool during_geometry : {false, true}) {
    draft();
    (void)read();
    struct Gate {
      std::mutex mutex;
      std::condition_variable changed;
      bool start = false, entered = false, released = false, finished = false;
      void begin() {
        std::unique_lock<std::mutex> lock(mutex);
        start = true;
        changed.notify_all();
        EXPECT_TRUE(changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
      }
      static void geometry(void *context) {
        static_cast<Gate *>(context)->begin();
      }
      static void output(void *context) {
        auto &gate = *static_cast<Gate *>(context);
        std::unique_lock<std::mutex> lock(gate.mutex);
        gate.entered = true;
        gate.changed.notify_all();
        EXPECT_TRUE(
            gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.released; }));
      }
    } gate;
    std::thread worker([&] {
      {
        std::unique_lock<std::mutex> lock(gate.mutex);
        if (!gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.start; }))
          return;
      }
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDERR_FILENO;
      metrics.before_output = Gate::output;
      metrics.output_context = &gate;
      EXPECT_TRUE(detail::immediate_stderr({"worker warning"}));
      metrics = {};
      {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.finished = true;
      }
      gate.changed.notify_all();
    });
    struct Join {
      Gate &gate;
      std::thread &worker;
      ~Join() {
        {
          std::lock_guard<std::mutex> lock(gate.mutex);
          gate.released = gate.start = true;
        }
        gate.changed.notify_all();
        worker.join();
      }
    } join{gate, worker};
    if (during_geometry) {
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDOUT_FILENO;
      metrics.after_stat = Gate::geometry;
      metrics.stat_context = &gate;
      line->append('x');
      metrics = {};
    } else {
      gate.begin();
      line->flush();
    }
    EXPECT_FALSE(output.failed());
    EXPECT_TRUE(read().empty());
    {
      std::lock_guard<std::mutex> lock(gate.mutex);
      gate.released = true;
    }
    gate.changed.notify_all();
    // The scope-bound join also protects assertions and exceptional exits.
    {
      std::unique_lock<std::mutex> lock(gate.mutex);
      EXPECT_TRUE(
          gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.finished; }));
    }
    line->flush();
    EXPECT_THAT(read(), EndsWith(during_geometry ? "codet> abcabcx" : "codet> abc"));
  }
}
TEST_F(NativeRecordLifecycle, ObserverCannotExposeAnUnreservedSeparatorGap) {
  draft();
  struct Observer : detail::LineOutputObserver {
    detail::LinePrompt &line;
    explicit Observer(detail::LinePrompt &value) : line(value) {}
    bool before_output(int fd, const struct stat &value) override {
      (void)line.before_output(fd, value);
      EXPECT_TRUE(detail::immediate_stderr({"intervening"}));
      return false;
    }
  } observer{*line};
  io.line_output = &observer;
  detail::DiagnosticWriter errors(std::cerr, &io);
  EXPECT_TRUE(errors.write({"outer"}));
  io.line_output = &*line;
  line->flush();
  const auto bytes = read();
  EXPECT_THAT(bytes, Not(HasSubstr("abcintervening")));
  EXPECT_THAT(bytes, HasSubstr("intervening\r\n"));
  EXPECT_THAT(bytes, EndsWith("codet> abc"));
}
TEST_F(NativeRecordLifecycle, EarlyEmergencyFailureReleasesItsReservation) {
  draft();
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.writes_interrupted = 9;
  FileNotice notice(std::string(2048, 'x'), FileNoticeKind::tool_result);
  emergency_file_diagnostic(notice);
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
  metrics = {};
  line->flush();
  EXPECT_THAT(read(), EndsWith("codet> abc"));
}
TEST_F(NativeRecordLifecycle, NullStreamCannotMutateNativeBoundaryOwnership) {
  const bool before = detail::native_line_destinations().row(identity);
  std::ostream missing(nullptr);
  detail::CliOutput injected(missing);
  detail::RequestInputController other;
  detail::LinePrompt null_line(other, injected, io, prompt, identity);
  null_line.acquire();
  null_line.append('x');
  EXPECT_EQ(detail::native_line_destinations().row(identity), before);
  EXPECT_TRUE(io.line_destinations.row(identity));
}
TEST_F(NativeRecordLifecycle, ReturningFromInjectedBufferAbandonsTheOldLongerRow) {
  for (char byte : std::string("abcdef"))
    line->append(byte);
  std::ostringstream buffer;
  struct Restore {
    std::streambuf *saved;
    ~Restore() {
      std::cout.rdbuf(saved);
    }
  } restore{std::cout.rdbuf(buffer.rdbuf())};
  for (unsigned i = 0; i != 3; ++i)
    line->erase_character();
  std::cout.rdbuf(restore.saved);
  line->flush();
  EXPECT_EQ(read(), "codet> abcdef\r\ncodet> abc");
  EXPECT_EQ(input.input().raw(), "abc");
}
TEST_F(NativeRecordLifecycle, OlderInspectionRefreshesWithoutDroppingTheCurrentRecord) {
  struct Gate {
    NativeRecordLifecycle *fixture;
    static void run(void *context) {
      auto &f = *static_cast<Gate *>(context)->fixture;
      // The interrupted fstat has captured the PTY. Publish a newer identity before it returns.
      int descriptors[2];
      ASSERT_EQ(::pipe(descriptors), 0);
      struct Close {
        int a, b;
        ~Close() {
          ::close(a);
          ::close(b);
        }
      } close{descriptors[0], descriptors[1]};
      ASSERT_EQ(::dup2(descriptors[1], STDERR_FILENO), STDERR_FILENO);
      EXPECT_TRUE(detail::immediate_stderr({"newer identity"}));
      pollfd ready{descriptors[0], POLLIN, 0};
      ASSERT_GT(::poll(&ready, 1, 1000), 0);
      char bytes[64];
      ASSERT_EQ(::read(descriptors[0], bytes, sizeof(bytes)), 15);
      // Leave a different confirmed observation in the cache while restoring the writable PTY.
      ASSERT_EQ(::dup2(f.slave, STDERR_FILENO), STDERR_FILENO);
    }
  } gate{this};
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.after_stat = Gate::run;
  metrics.stat_context = &gate;
  EXPECT_TRUE(detail::immediate_stderr({"current record"}));
  EXPECT_GE(metrics.inspections, 3u); // Old snapshot, intervening identity, fresh validation.
  metrics = {};
  EXPECT_EQ(read(), "current record\r\n");
}
TEST_F(NativeRecordLifecycle, StaleInspectionCannotSuppressAnEmergencyOnTheCurrentDescriptor) {
  struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false;
    static void pause(void *context) {
      auto &gate = *static_cast<Gate *>(context);
      std::unique_lock<std::mutex> lock(gate.mutex);
      gate.entered = true;
      gate.changed.notify_all();
      EXPECT_TRUE(
          gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.released; }));
    }
    bool wait() {
      std::unique_lock<std::mutex> lock(mutex);
      return changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; });
    }
    void release() {
      {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
      }
      changed.notify_all();
    }
  } old_gate, current_gate;
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  struct Close {
    int a, b;
    ~Close() {
      ::close(a);
      ::close(b);
    }
  } close{descriptors[0], descriptors[1]};
  std::thread older([&] {
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.after_stat = Gate::pause;
    metrics.stat_context = &old_gate;
    EXPECT_TRUE(detail::immediate_stderr({"older"}));
    metrics = {};
  });
  std::thread current;
  struct Join {
    Gate &a, &b;
    std::thread &older, &current;
    ~Join() {
      a.release();
      b.release();
      if (older.joinable())
        older.join();
      if (current.joinable())
        current.join();
    }
  } join{old_gate, current_gate, older, current};
  ASSERT_TRUE(old_gate.wait());
  ASSERT_EQ(::dup2(descriptors[1], STDERR_FILENO), STDERR_FILENO);
  const FileNotice notice("/current/preserved/result", FileNoticeKind::tool_result);
  current = std::thread([&] {
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.before_bound = Gate::pause;
    metrics.bound_context = &current_gate;
    emergency_file_diagnostic(notice);
    metrics = {};
  });
  ASSERT_TRUE(current_gate.wait());
  old_gate.release();
  older.join();
  current_gate.release();
  current.join();
  ASSERT_EQ(::fcntl(descriptors[0], F_SETFL, O_NONBLOCK), 0);
  std::string bytes;
  char buffer[256];
  ssize_t count;
  while ((count = ::read(descriptors[0], buffer, sizeof(buffer))) > 0)
    bytes.append(buffer, static_cast<size_t>(count));
  EXPECT_THAT(bytes, HasSubstr("older\n"));
  EXPECT_THAT(bytes, HasSubstr(notice.text() + "\n"));
}
TEST(DiagnosticFallbackMetadata, ResetMayOverlapAStableReplacedBuffer) {
  struct Sink : std::streambuf {
    std::streamsize xsputn(const char *, std::streamsize count) override {
      return count;
    }
    int_type overflow(int_type ch) override {
      return traits_type::not_eof(ch);
    }
  } sink;
  struct Restore {
    std::streambuf *saved;
    ~Restore() {
      std::cerr.rdbuf(saved);
      detail::reset_standard_diagnostics();
    }
  } restore{std::cerr.rdbuf(&sink)};
  std::atomic<bool> start{false};
  std::thread writer([&] {
    while (!start.load())
      std::this_thread::yield();
    for (unsigned i = 0; i != 1000; ++i)
      (void)detail::write_diagnostic(std::cerr, {"stable injected buffer"});
  });
  start = true;
  for (unsigned i = 0; i != 1000; ++i)
    detail::reset_standard_diagnostics();
  writer.join();
}
TEST(DiagnosticReservations, CompletionPublishesAnEventButPresentationFailureDoesNot) {
  detail::LineDestinations destinations;
  struct stat tty {};
  tty.st_mode = S_IFCHR;
  destinations.bind(1, tty);
  const auto diagnostic = destinations.admit(1, false);
  const auto admitted = destinations.generation(1);
  EXPECT_EQ(destinations.active(1), 1u);
  destinations.finish(diagnostic, true, true);
  EXPECT_GT(destinations.generation(1), admitted);
  EXPECT_EQ(destinations.active(1), 0u);
  const auto presentation = destinations.admit(1, true);
  const auto before_failure = destinations.generation(1);
  destinations.finish(presentation, false, false);
  EXPECT_EQ(destinations.generation(1), before_failure);
  EXPECT_TRUE(destinations.pending(1));
}
TEST(DiagnosticReservations, AliasesResetAndOldFailuresRetainTheirOwnership) {
  detail::LineDestinations destinations;
  struct stat tty {};
  tty.st_mode = S_IFCHR;
  tty.st_ino = 1;
  destinations.bind(1, tty);
  destinations.bind(2, tty);
  destinations.open(tty);
  const auto first = destinations.admit(2, false);
  EXPECT_TRUE(first.separator);
  EXPECT_EQ(destinations.active(1), 1u);
  EXPECT_FALSE(destinations.admit(1, true).admitted);
  const auto second = destinations.admit(1, false);
  destinations.finish(second, false, true);
  destinations.finish(first, true, true);
  EXPECT_TRUE(destinations.pending(1));
  const auto old = destinations.admit(2, false);
  destinations.reset();
  destinations.bind(1, tty);
  const auto current = destinations.admit(1, false);
  destinations.finish(old, true, true);
  EXPECT_EQ(destinations.active(1), 1u);
  destinations.finish(current, true, true);
  EXPECT_EQ(destinations.active(1), 0u);
}
} // namespace
