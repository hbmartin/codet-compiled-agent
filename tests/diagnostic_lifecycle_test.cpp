#include "codet/detail/line_prompt.hpp"
#include "codet/diagnostics.hpp"
#include "native_io_metrics.hpp"
#include "process_helper.hpp"
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

  // Assertions belong outside this scope. Callbacks collect observations only.
  template <class Operation> decltype(auto) capture(Operation operation) {
    struct Restore {
      int out, error;
      ~Restore() {
        (void)::dup2(out, STDOUT_FILENO);
        (void)::dup2(error, STDERR_FILENO);
      }
    } restore{saved_out, saved_error};
    if (::dup2(slave, STDOUT_FILENO) < 0 || ::dup2(slave, STDERR_FILENO) < 0)
      throw Error("Cannot capture fixture descriptors");
    return operation();
  }
  void SetUp() override {
    struct winsize size {};
    size.ws_row = 24;
    size.ws_col = 80;
    ASSERT_EQ(::openpty(&master, &slave, nullptr, nullptr, &size), 0);
    saved_out = ::dup(STDOUT_FILENO);
    saved_error = ::dup(STDERR_FILENO);
    ASSERT_GE(saved_out, 0);
    ASSERT_GE(saved_error, 0);
    ASSERT_EQ(::fstat(slave, &identity), 0);
    ASSERT_EQ(::fcntl(master, F_SETFL, O_NONBLOCK), 0);
    detail::reset_standard_diagnostics();
    capture([&] {
      line.emplace(input, output, io, prompt, identity);
      line->acquire();
    });
  }
  void TearDown() override {
    test::native_io_metrics = {};
    line.reset();
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
    capture([&] {
      for (char byte : std::string("abc"))
        line->append(byte);
    });
  }
};
struct DeliveryGate {
  std::mutex mutex;
  std::condition_variable changed;
  bool start = false, entered = false, released = false, finished = false, timed_out = false;
  unsigned inspections = 0;
  void begin() {
    std::unique_lock<std::mutex> lock(mutex);
    start = true;
    changed.notify_all();
    timed_out |= !changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; });
  }
  static void output(void *context) {
    auto &gate = *static_cast<DeliveryGate *>(context);
    std::unique_lock<std::mutex> lock(gate.mutex);
    gate.entered = true;
    gate.changed.notify_all();
    gate.timed_out |=
        !gate.changed.wait_for(lock, std::chrono::seconds(5), [&] { return gate.released; });
  }
  static void geometry(void *context) {
    static_cast<DeliveryGate *>(context)->begin();
  }
  static void second_stat(void *context) {
    auto &gate = *static_cast<DeliveryGate *>(context);
    if (++gate.inspections == 1)
      test::native_io_metrics.after_stat = second_stat;
    else
      gate.begin();
  }
  bool wait_for_start() {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [&] { return start; });
  }
  void release() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      released = start = true;
    }
    changed.notify_all();
  }
};
struct JoinDelivery {
  DeliveryGate &gate;
  std::thread &worker;
  ~JoinDelivery() {
    gate.release();
    if (worker.joinable())
      worker.join();
  }
};
TEST_F(NativeRecordLifecycle, DeferredEraseRetainsWorkUntilDiagnosticCompletion) {
  for (unsigned columns : {0u, 80u}) {
    struct winsize size {};
    size.ws_row = 24;
    size.ws_col = static_cast<unsigned short>(columns);
    ASSERT_EQ(::ioctl(slave, TIOCSWINSZ, &size), 0);
    capture([&] { line->append('a'); });
    EXPECT_THAT(read(), EndsWith("codet> a"));
    DeliveryGate gate;
    bool delivered = false, deferred = false, cleanup_retained = false;
    unsigned inspected = 0;
    capture([&] {
      std::thread worker([&] {
        if (!gate.wait_for_start())
          return;
        auto &metrics = test::native_io_metrics;
        metrics.interrupt_fd = STDERR_FILENO;
        metrics.before_output = DeliveryGate::output;
        metrics.output_context = &gate;
        delivered = detail::immediate_stderr({"held diagnostic"});
        metrics = {};
      });
      JoinDelivery join{gate, worker};
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDOUT_FILENO;
      metrics.after_stat = DeliveryGate::second_stat;
      metrics.stat_context = &gate;
      line->erase_character();
      deferred = output.presentation_deferred();
      cleanup_retained = line->pending_presentation();
      metrics = {};
      gate.release();
      worker.join();
      metrics.interrupt_fd = STDOUT_FILENO;
      line->flush();
      inspected = metrics.inspections;
      metrics = {};
    });
    EXPECT_TRUE(deferred);
    EXPECT_TRUE(cleanup_retained);
    EXPECT_TRUE(delivered);
    EXPECT_FALSE(gate.timed_out);
    EXPECT_GT(inspected, 0u);
    EXPECT_FALSE(output.failed());
    EXPECT_TRUE(input.input().empty());
    // The completed diagnostic already separated the obsolete draft. No stale-width erase.
    EXPECT_EQ(read(), "\r\nheld diagnostic\r\n");
    capture([&] { line->flush(); });
    EXPECT_TRUE(read().empty());
  }
}
TEST_F(NativeRecordLifecycle, InjectedStdoutSharesSeparatorWithNativeStderr) {
  line.reset();
  struct Forward : std::streambuf {
    std::streamsize xsputn(const char *p, std::streamsize n) override {
      return ::write(STDOUT_FILENO, p, static_cast<size_t>(n));
    }
    int_type overflow(int_type c) override {
      if (traits_type::eq_int_type(c, traits_type::eof()))
        return traits_type::not_eof(c);
      char ch = traits_type::to_char_type(c);
      return ::write(STDOUT_FILENO, &ch, 1) == 1 ? c : traits_type::eof();
    }
  } forward;
  struct Restore {
    std::streambuf *old;
    ~Restore() {
      std::cout.rdbuf(old);
    }
  } restore{std::cout.rdbuf(&forward)};
  output.associate_terminal(STDOUT_FILENO);
  capture([&] { line.emplace(input, output, io, prompt, identity, 80); });
  line->acquire();
  draft();
  EXPECT_EQ(read(), "codet> abc");
  detail::DiagnosticWriter errors(std::cerr, &io);
  EXPECT_TRUE(capture([&] { return errors.write({"warning"}); }));
  EXPECT_EQ(read(), "\r\nwarning\r\n");
}
TEST_F(NativeRecordLifecycle, ObserverAndNativeReservationRequestOnlyOneSeparator) {
  draft();
  (void)read();
  struct Observer : detail::LineOutputObserver {
    bool before_output(int, const struct stat &) override {
      return true;
    }
  } observer;
  io.line_output = &observer;
  detail::DiagnosticWriter errors(std::cerr, &io);
  EXPECT_TRUE(capture([&] { return errors.write({"warning"}); }));
  io.line_output = &*line;
  EXPECT_EQ(read(), "\r\nwarning\r\n");
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
}
TEST_F(NativeRecordLifecycle, ThrowingObserverReleasesReservationAndPreservesException) {
  draft();
  (void)read();
  const auto original = std::make_exception_ptr(Error("observer failed"));
  struct Observer : detail::LineOutputObserver {
    std::exception_ptr error;
    explicit Observer(std::exception_ptr value) : error(value) {}
    bool before_output(int, const struct stat &) override {
      std::rethrow_exception(error);
    }
  } observer{original};
  io.line_output = &observer;
  detail::DiagnosticWriter errors(std::cerr, &io);
  std::exception_ptr caught;
  try {
    capture([&] { (void)errors.write({"must not print"}); });
  } catch (...) {
    caught = std::current_exception();
  }
  io.line_output = &*line;
  EXPECT_EQ(caught, original);
  EXPECT_TRUE(read().empty());
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
  capture([&] { line->flush(); });
  EXPECT_THAT(read(), EndsWith("codet> abc"));
}
TEST_F(NativeRecordLifecycle, FailedPromptDoesNotGenerateIdleRetryEvents) {
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  ::close(descriptors[0]);
  unsigned inspections = 0, writes = 0, edited_inspections = 0;
  uint64_t before = 0, after = 0;
  capture([&] {
    const auto replaced = ::dup2(descriptors[1], STDOUT_FILENO);
    ::close(descriptors[1]);
    if (replaced < 0)
      throw Error("Cannot install failed-prompt fixture");
    line->append('a');
    before = detail::native_line_destinations().snapshot(1, identity).generation;
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDOUT_FILENO;
    for (unsigned i = 0; i != 20; ++i)
      line->flush();
    inspections = metrics.inspections;
    writes = metrics.writes + metrics.vectors;
    after = detail::native_line_destinations().snapshot(1, identity).generation;
    line->append('b');
    edited_inspections = metrics.inspections;
    metrics = {};
  });
  EXPECT_EQ(inspections, 0u);
  EXPECT_EQ(writes, 0u);
  EXPECT_EQ(after, before);
  EXPECT_GT(edited_inspections, 0u);
  EXPECT_EQ(input.input().raw(), "ab");
}
TEST_F(NativeRecordLifecycle, PromptWaitsForCompletionAndRecoversWithoutAnotherKey) {
  draft();
  EXPECT_EQ(read(), "codet> abc");
  struct Gate {
    NativeRecordLifecycle *fixture;
    size_t active = 0;
    bool empty = false, failed = true;
    static void run(void *context) {
      auto &g = *static_cast<Gate *>(context);
      auto &f = *g.fixture;
      g.active = detail::native_line_destinations().snapshot(1, f.identity).active;
      f.line->flush();
      g.empty = f.read().empty();
      g.failed = f.output.failed();
    }
  } gate{this};
  const bool delivered = capture([&] {
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.before_output = Gate::run;
    metrics.output_context = &gate;
    const auto result = detail::immediate_stderr({"warning"});
    metrics = {};
    return result;
  });
  EXPECT_TRUE(delivered);
  EXPECT_EQ(gate.active, 1u);
  EXPECT_TRUE(gate.empty);
  EXPECT_FALSE(gate.failed);
  EXPECT_EQ(read(), "\r\nwarning\r\n");
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
  capture([&] { line->flush(); });
  EXPECT_EQ(read(), "codet> abc");
  capture([&] { line->flush(); });
  EXPECT_TRUE(read().empty());
  EXPECT_EQ(input.input().raw(), "abc");
}
TEST_F(NativeRecordLifecycle, ConcurrentDeliveryDefersBothSnapshotAndAdmissionRaces) {
  for (bool during_geometry : {false, true}) {
    draft();
    (void)read();
    DeliveryGate gate;
    bool delivered = false, empty = false;
    capture([&] {
      std::thread worker([&] {
        if (!gate.wait_for_start())
          return;
        auto &metrics = test::native_io_metrics;
        metrics.interrupt_fd = STDERR_FILENO;
        metrics.before_output = DeliveryGate::output;
        metrics.output_context = &gate;
        delivered = detail::immediate_stderr({"worker warning"});
        metrics = {};
      });
      JoinDelivery join{gate, worker};
      if (during_geometry) {
        auto &metrics = test::native_io_metrics;
        metrics.interrupt_fd = STDOUT_FILENO;
        metrics.after_stat = DeliveryGate::geometry;
        metrics.stat_context = &gate;
        line->append('x');
        metrics = {};
      } else {
        gate.begin();
        line->flush();
      }
      empty = read().empty();
      gate.release();
      worker.join();
      line->flush();
    });
    EXPECT_TRUE(delivered);
    EXPECT_TRUE(empty);
    EXPECT_FALSE(gate.timed_out);
    EXPECT_FALSE(output.failed());
    EXPECT_THAT(read(), EndsWith(during_geometry ? "codet> abcabcx" : "codet> abc"));
  }
}
TEST_F(NativeRecordLifecycle, ObserverCannotExposeAnUnreservedSeparatorGap) {
  draft();
  struct Observer : detail::LineOutputObserver {
    detail::LinePrompt &line;
    bool delivered = false;
    explicit Observer(detail::LinePrompt &value) : line(value) {}
    bool before_output(int fd, const struct stat &value) override {
      (void)line.before_output(fd, value);
      delivered = detail::immediate_stderr({"intervening"});
      return false;
    }
  } observer{*line};
  io.line_output = &observer;
  detail::DiagnosticWriter errors(std::cerr, &io);
  EXPECT_TRUE(capture([&] { return errors.write({"outer"}); }));
  io.line_output = &*line;
  capture([&] { line->flush(); });
  EXPECT_TRUE(observer.delivered);
  const auto bytes = read();
  EXPECT_THAT(bytes, Not(HasSubstr("abcintervening")));
  EXPECT_THAT(bytes, HasSubstr("intervening\r\n"));
  EXPECT_THAT(bytes, EndsWith("codet> abc"));
}
TEST_F(NativeRecordLifecycle, EarlyEmergencyFailureReleasesItsReservation) {
  draft();
  capture([&] {
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.writes_interrupted = 9;
    FileNotice notice(std::string(2048, 'x'), FileNoticeKind::tool_result);
    emergency_file_diagnostic(notice);
    metrics = {};
  });
  EXPECT_EQ(detail::native_line_destinations().snapshot(1, identity).active, 0u);
  capture([&] { line->flush(); });
  EXPECT_THAT(read(), EndsWith("codet> abc"));
}
TEST_F(NativeRecordLifecycle, NullStreamCannotMutateNativeBoundaryOwnership) {
  const bool before = detail::native_line_destinations().row(identity);
  std::ostream missing(nullptr);
  detail::CliOutput injected(missing);
  detail::RequestInputController other;
  detail::LinePrompt null_line(other, injected, io, prompt, identity);
  null_line.acquire();
  capture([&] { null_line.append('x'); });
  EXPECT_EQ(detail::native_line_destinations().row(identity), before);
  EXPECT_TRUE(io.line_destinations.row(identity));
}
TEST_F(NativeRecordLifecycle, ReturningFromInjectedBufferAbandonsTheOldLongerRow) {
  capture([&] {
    for (char byte : std::string("abcdef"))
      line->append(byte);
  });
  std::ostringstream buffer;
  struct Restore {
    std::streambuf *saved;
    ~Restore() {
      std::cout.rdbuf(saved);
    }
  } restore{std::cout.rdbuf(buffer.rdbuf())};
  capture([&] {
    for (unsigned i = 0; i != 3; ++i)
      line->erase_character();
    std::cout.rdbuf(restore.saved);
    line->flush();
  });
  EXPECT_EQ(read(), "codet> abcdef\r\ncodet> abc");
  EXPECT_EQ(input.input().raw(), "abc");
}
TEST_F(NativeRecordLifecycle, OlderInspectionRefreshesWithoutDroppingTheCurrentRecord) {
  struct Gate {
    NativeRecordLifecycle *fixture;
    bool passed = false;
    static void run(void *context) {
      auto &g = *static_cast<Gate *>(context);
      auto &f = *g.fixture;
      int descriptors[2];
      if (::pipe(descriptors) != 0)
        return;
      struct Close {
        int a, b;
        ~Close() {
          ::close(a);
          ::close(b);
        }
      } close{descriptors[0], descriptors[1]};
      if (::dup2(descriptors[1], STDERR_FILENO) < 0)
        return;
      const bool delivered = detail::immediate_stderr({"newer identity"});
      pollfd ready{descriptors[0], POLLIN, 0};
      char bytes[64];
      g.passed = delivered && ::poll(&ready, 1, 1000) > 0 &&
                 ::read(descriptors[0], bytes, sizeof(bytes)) == 15;
      g.passed &= ::dup2(f.slave, STDERR_FILENO) == STDERR_FILENO;
    }
  } gate{this};
  bool delivered = false;
  unsigned inspections = 0;
  capture([&] {
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.after_stat = Gate::run;
    metrics.stat_context = &gate;
    delivered = detail::immediate_stderr({"current record"});
    inspections = metrics.inspections;
    metrics = {};
  });
  EXPECT_TRUE(delivered);
  EXPECT_TRUE(gate.passed);
  EXPECT_GE(inspections, 3u);
  EXPECT_EQ(read(), "current record\r\n");
}
TEST_F(NativeRecordLifecycle, StaleInspectionCannotSuppressAnEmergencyOnTheCurrentDescriptor) {
  DeliveryGate old_gate, current_gate;
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  struct Close {
    int a, b;
    ~Close() {
      ::close(a);
      ::close(b);
    }
  } close{descriptors[0], descriptors[1]};
  bool ready = false, replaced = false, delivered = false;
  const FileNotice notice("/current/preserved/result", FileNoticeKind::tool_result);
  capture([&] {
    std::thread older([&] {
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDERR_FILENO;
      metrics.after_stat = DeliveryGate::output;
      metrics.stat_context = &old_gate;
      delivered = detail::immediate_stderr({"older"});
      metrics = {};
    });
    JoinDelivery join_old{old_gate, older};
    {
      std::unique_lock<std::mutex> lock(old_gate.mutex);
      ready = old_gate.changed.wait_for(lock, std::chrono::seconds(5),
                                        [&] { return old_gate.entered; });
    }
    replaced = ::dup2(descriptors[1], STDERR_FILENO) == STDERR_FILENO;
    std::thread current([&] {
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDERR_FILENO;
      metrics.before_bound = DeliveryGate::output;
      metrics.bound_context = &current_gate;
      emergency_file_diagnostic(notice);
      metrics = {};
    });
    JoinDelivery join_current{current_gate, current};
    {
      std::unique_lock<std::mutex> lock(current_gate.mutex);
      ready &= current_gate.changed.wait_for(lock, std::chrono::seconds(5),
                                             [&] { return current_gate.entered; });
    }
    old_gate.release();
    older.join();
    current_gate.release();
    current.join();
  });
  EXPECT_TRUE(ready);
  EXPECT_TRUE(replaced);
  EXPECT_TRUE(delivered);
  EXPECT_FALSE(old_gate.timed_out || current_gate.timed_out);
  ASSERT_EQ(::fcntl(descriptors[0], F_SETFL, O_NONBLOCK), 0);
  std::string bytes;
  char buffer[256];
  ssize_t count;
  while ((count = ::read(descriptors[0], buffer, sizeof(buffer))) > 0)
    bytes.append(buffer, static_cast<size_t>(count));
  EXPECT_THAT(bytes, HasSubstr("older\n"));
  EXPECT_THAT(bytes, HasSubstr(notice.text() + "\n"));
}
TEST_F(NativeRecordLifecycle, CaptureRestoresDescriptorsOnExceptions) {
  EXPECT_THROW(capture([&] { throw Error("original capture exception"); }), Error);
  struct stat restored {};
  ASSERT_EQ(::fstat(STDOUT_FILENO, &restored), 0);
  struct stat expected {};
  ASSERT_EQ(::fstat(saved_out, &expected), 0);
  EXPECT_TRUE(detail::same_destination(restored, expected));
}
TEST_F(NativeRecordLifecycle, DISABLED_FailurePrinterProbe) {
  capture([&] { line->append('x'); });
  EXPECT_EQ(1, 2) << "LIFECYCLE_FAILURE_SENTINEL";
  EXPECT_EQ(read(), "codet> x");
}
TEST(DiagnosticFixture, FailureDetailsReachParentLogAndDoNotContaminatePty) {
  struct Capture {
    bool active = true;
    Capture() {
      internal::CaptureStdout();
    }
    std::string finish() {
      active = false;
      return internal::GetCapturedStdout();
    }
    ~Capture() {
      if (active)
        (void)internal::GetCapturedStdout();
    }
  } capture;
  const auto child =
      test::start_helper({"--gtest_filter=NativeRecordLifecycle.DISABLED_FailurePrinterProbe",
                          "--gtest_also_run_disabled_tests"});
  int status = 0;
  const bool waited = child > 0 && test::wait_helper(child, status);
  const auto text = capture.finish();
  EXPECT_TRUE(waited);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 1);
  EXPECT_THAT(text, HasSubstr("LIFECYCLE_FAILURE_SENTINEL"));
  EXPECT_THAT(text, HasSubstr("Expected equality of these values"));
  EXPECT_THAT(text, Not(HasSubstr("Which is: \"codet> x")));
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
TEST_F(NativeRecordLifecycle, DeferredSubmissionRetainsRawBytesAndOriginalRequest) {
  struct winsize size {};
  size.ws_row = 24;
  ASSERT_EQ(::ioctl(slave, TIOCSWINSZ, &size), 0);
  input.open({"session", "first", 1}, detail::RequestKind::approval);
  capture([&] {
    line->append('a');
    line->append(static_cast<char>(0xe9));
  });
  (void)read();
  DeliveryGate gate;
  bool deferred = false, delivered = false;
  std::optional<detail::InputLine> submitted;
  capture([&] {
    std::thread worker([&] {
      if (!gate.wait_for_start())
        return;
      test::native_io_metrics.interrupt_fd = STDERR_FILENO;
      test::native_io_metrics.before_output = DeliveryGate::output;
      test::native_io_metrics.output_context = &gate;
      delivered = detail::immediate_stderr({"held diagnostic"});
      test::native_io_metrics = {};
    });
    JoinDelivery join{gate, worker};
    gate.begin();
    submitted = line->try_take();
    deferred = !submitted && line->submission_pending();
    input.close();
    input.open({"session", "replacement", 1}, detail::RequestKind::approval);
    line->append('y'); // Pending submission owns editing until its display attempt completes.
    gate.release();
    worker.join();
    submitted = line->try_take();
    line->flush();
  });
  ASSERT_TRUE(submitted);
  ASSERT_TRUE(submitted->request);
  EXPECT_TRUE(deferred);
  EXPECT_TRUE(delivered);
  EXPECT_FALSE(gate.timed_out);
  EXPECT_EQ(submitted->text, std::string("a\xe9"));
  EXPECT_EQ(submitted->request->request, "first");
  EXPECT_EQ(submitted->action(), detail::InputAction::discard);
  EXPECT_THAT(read(), HasSubstr("\\xE9"));
  EXPECT_FALSE(output.failed());
  EXPECT_TRUE(input.input().empty());
}
TEST_F(NativeRecordLifecycle, MemoryOutputDoesNotClaimAnUnfinishedNativeRow) {
  line.reset();
  std::ostringstream memory;
  detail::CliOutput injected(memory);
  line.emplace(input, injected, io, prompt, identity, 80);
  line->acquire();
  line->append('a');
  EXPECT_EQ(memory.str(), "codet> a");
  EXPECT_TRUE(capture([&] { return detail::immediate_stderr({"warning"}); }));
  EXPECT_EQ(read(), "warning\r\n");
}

#if defined(CODET_NATIVE_TERMINAL_METRICS)
namespace {
void pending_echo(NativeRecordLifecycle &fixture, unsigned columns, bool prior_line) {
  struct winsize size {};
  size.ws_row = 24;
  size.ws_col = static_cast<unsigned short>(columns);
  ASSERT_EQ(::ioctl(fixture.slave, TIOCSWINSZ, &size), 0);
  if (prior_line) {
    fixture.capture([&] {
      for (char byte : std::string("hello world"))
        fixture.line->append(byte);
      (void)fixture.line->try_take();
    });
    (void)fixture.read();
  } else if (columns) {
    fixture.capture([&] {
      fixture.line->append('a');
      fixture.line->append('b');
    });
    (void)fixture.read();
  }
  DeliveryGate gate;
  bool held = false;
  std::optional<detail::InputLine> submitted;
  fixture.capture([&] {
    std::thread worker([&] {
      if (!gate.wait_for_start())
        return;
      test::native_io_metrics.interrupt_fd = STDERR_FILENO;
      test::native_io_metrics.before_output = DeliveryGate::output;
      test::native_io_metrics.output_context = &gate;
      (void)detail::immediate_stderr({"held diagnostic"});
      test::native_io_metrics = {};
    });
    JoinDelivery join{gate, worker};
    gate.begin();
    if (columns)
      fixture.line->append('c');
    else
      for (char byte : std::string("abc"))
        fixture.line->append(byte);
    submitted = fixture.line->try_take();
    held = !submitted && fixture.line->submission_pending();
    gate.release();
    worker.join();
    submitted = fixture.line->try_take();
  });
  ASSERT_TRUE(held);
  ASSERT_TRUE(submitted);
  EXPECT_EQ(submitted->text, "abc");
  EXPECT_FALSE(gate.timed_out);
  EXPECT_THAT(fixture.read(), HasSubstr("codet> abc")) << "Frozen submitted line was not displayed";
}
TEST_F(NativeRecordLifecycle, KnownWidthPendingLineIsVisible) {
  pending_echo(*this, 80, false);
}
TEST_F(NativeRecordLifecycle, BasicSecondPendingLineIsVisible) {
  pending_echo(*this, 0, true);
}
TEST_F(NativeRecordLifecycle, BasicFreshPendingLinePositiveControl) {
  pending_echo(*this, 0, false);
}
} // namespace
#endif

TEST_F(NativeRecordLifecycle, FrozenSubmissionOwnsOriginAndDiscardCompletesExactlyOnce) {
  line->origin("origin-session", 7);
  draft();
  (void)read();
  DeliveryGate gate;
  bool held = false;
  std::optional<detail::InputLine> discarded;
  capture([&] {
    std::thread worker([&] {
      if (!gate.wait_for_start())
        return;
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDERR_FILENO;
      metrics.before_output = DeliveryGate::output;
      metrics.output_context = &gate;
      (void)detail::immediate_stderr({"held diagnostic"});
      metrics = {};
    });
    JoinDelivery join{gate, worker};
    gate.begin();
    held = !line->try_take();
    line->origin("replacement-session", 8);
    input.open({"replacement-session", "replacement", 8}, detail::RequestKind::approval);
    line->append('y');
    discarded = line->discard_pending();
    gate.release();
  });
  ASSERT_TRUE(held);
  ASSERT_TRUE(discarded);
  EXPECT_EQ(discarded->text, "abc");
  ASSERT_TRUE(discarded->origin);
  EXPECT_EQ(discarded->origin->session, "origin-session");
  EXPECT_EQ(discarded->origin->turn, 7u);
  EXPECT_FALSE(line->discard_pending());
  EXPECT_TRUE(input.input().empty());
}

TEST_F(NativeRecordLifecycle, InterruptedSubmissionRepaintsAfterItsSeparatorWasDeferred) {
  capture([&] {
    line->append('a');
    line->append('b');
  });
  (void)read();
  input.append('c'); // The new byte has not been displayed when Enter freezes the line.
  DeliveryGate gate;
  std::optional<detail::InputLine> submitted;
  bool held = false;
  capture([&] {
    std::thread worker([&] {
      if (!gate.wait_for_start())
        return;
      auto &metrics = test::native_io_metrics;
      metrics.interrupt_fd = STDERR_FILENO;
      metrics.before_output = DeliveryGate::output;
      metrics.output_context = &gate;
      (void)detail::immediate_stderr({"intervening diagnostic"});
      metrics = {};
    });
    JoinDelivery join{gate, worker};
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDOUT_FILENO;
    metrics.before_output = DeliveryGate::geometry;
    metrics.output_context = &gate;
    submitted = line->try_take();
    held = !submitted && line->submission_pending();
    metrics = {};
    gate.release();
    worker.join();
    submitted = line->try_take();
  });
  ASSERT_TRUE(held);
  ASSERT_TRUE(submitted);
  EXPECT_EQ(submitted->text, "abc");
  EXPECT_THAT(read(), EndsWith("intervening diagnostic\r\ncodet> abc\r\n"));
  EXPECT_FALSE(gate.timed_out);
}
