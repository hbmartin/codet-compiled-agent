#include "codet/detail/cli_diagnostics.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace codet;
using namespace testing;
namespace {
struct PartialDiagnosticStream : std::streambuf {
  std::string bytes;
  bool partial = true, fail_newline = false;
  size_t records = 0;
  std::streamsize xsputn(const char *text, std::streamsize count) override {
    ++records;
    const auto written = partial ? std::min(count, std::streamsize{2}) : count;
    bytes.append(text, static_cast<size_t>(written));
    return written;
  }
  int overflow(int character) override {
    if (fail_newline)
      return traits_type::eof();
    bytes += static_cast<char>(character);
    return character;
  }
};
TEST(DiagnosticWriter, PartialRecordsRetainASeparatorAcrossFailedAttempts) {
  for (bool mask : {false, true}) {
    PartialDiagnosticStream buffer;
    std::ostream stream(&buffer);
    if (mask)
      stream.exceptions(std::ios::badbit);
    detail::DiagnosticWriter writer(stream);
    if (mask)
      EXPECT_THROW(writer.write({"first"}), std::ios_base::failure);
    else
      EXPECT_FALSE(writer.write({"first"}));
    buffer.partial = false;
    buffer.fail_newline = true;
    if (mask)
      EXPECT_THROW(writer.write({"must not start"}), std::ios_base::failure);
    else
      EXPECT_FALSE(writer.write({"must not start"}));
    EXPECT_EQ(buffer.records, 1u);
    buffer.fail_newline = false;
    EXPECT_TRUE(writer.write({"second"}));
    EXPECT_EQ(buffer.bytes, "fi\nsecond\n");
  }
}
TEST(DiagnosticWriter, FailedTrailingNewlineIsRepairedBeforeTheNextRecord) {
  PartialDiagnosticStream buffer;
  buffer.partial = false;
  buffer.fail_newline = true;
  std::ostream stream(&buffer);
  detail::DiagnosticWriter writer(stream);
  EXPECT_FALSE(writer.write({"first"}));
  buffer.fail_newline = false;
  EXPECT_TRUE(writer.write({"second"}));
  EXPECT_EQ(buffer.bytes, "first\nsecond\n");
}
TEST(DiagnosticWriter, ReplacedDestinationsAndNewInvocationsDiscardStaleSeparators) {
  struct Refusing : std::stringbuf {
    bool refuse = true;
    std::streamsize xsputn(const char *text, std::streamsize count) override {
      return refuse ? 0 : std::stringbuf::xsputn(text, count);
    }
  } failed;
  std::ostringstream next;
  struct Restore {
    std::streambuf *original;
    ~Restore() {
      std::cerr.rdbuf(original);
      detail::recover_diagnostic_stream(std::cerr);
      detail::reset_standard_diagnostics();
    }
  } restore{std::cerr.rdbuf(&failed)};
  EXPECT_FALSE(detail::write_diagnostic(std::cerr, {"lost"}));
  std::cerr.rdbuf(next.rdbuf());
  EXPECT_TRUE(detail::write_diagnostic(std::cerr, {"fresh"}));
  EXPECT_EQ(next.str(), "fresh\n");
  std::cerr.rdbuf(&failed);
  EXPECT_FALSE(detail::write_diagnostic(std::cerr, {"lost"}));
  failed.refuse = false;
  detail::reset_standard_diagnostics();
  EXPECT_TRUE(detail::write_diagnostic(std::cerr, {"new invocation"}));
  EXPECT_EQ(failed.str(), "new invocation\n");
}
TEST(DiagnosticWriter, EmergencyDescriptorStateCannotLeakIntoACapturedStreamOrReboundFd) {
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  const int saved = ::dup(STDERR_FILENO);
  ASSERT_GE(saved, 0);
  std::ostringstream captured;
  struct Restore {
    int saved, read, write;
    std::streambuf *buffer;
    ~Restore() {
      (void)::dup2(saved, STDERR_FILENO);
      ::close(saved);
      ::close(read);
      ::close(write);
      std::cerr.rdbuf(buffer);
      detail::recover_diagnostic_stream(std::cerr);
      detail::reset_standard_diagnostics();
    }
  } restore{saved, descriptors[0], descriptors[1], std::cerr.rdbuf(captured.rdbuf())};
  ASSERT_EQ(::fcntl(descriptors[1], F_SETFL, O_NONBLOCK), 0);
  char bytes[4096]{};
  while (::write(descriptors[1], bytes, sizeof(bytes)) > 0) {
  }
  ASSERT_EQ(errno, EAGAIN);
  ASSERT_EQ(::dup2(descriptors[1], STDERR_FILENO), STDERR_FILENO);
  FileNotice notice("/complete/path", FileNoticeKind::tool_result);
  notice.confirmed = true;
  emergency_file_diagnostic(notice);
  EXPECT_TRUE(detail::write_diagnostic(std::cerr, {"captured"}));
  EXPECT_EQ(captured.str(), "captured\n");
  ASSERT_EQ(::dup2(saved, STDERR_FILENO), STDERR_FILENO);
  testing::internal::CaptureStderr();
  emergency_file_diagnostic(notice);
  const auto delivered = testing::internal::GetCapturedStderr();
  EXPECT_EQ(delivered, "Uncommitted tool result preserved at /complete/path\n");
}
TEST(DiagnosticWriter, HumanTextEscapesControlsWithoutChangingRawJsonSanitization) {
  const std::string text =
      std::string("raw\0\r\x1b]0;title\a\x7f", 16) + "\xc2\x9b\xe2\x80\xae\xe2\x80\xa8\xff";
  const auto shown = detail::terminal_text(text);
  EXPECT_EQ(shown,
            "raw\\x00\\x0D\\x1B]0;title\\x07\\x7F\\xC2\\x9B\\xE2\\x80\\xAE\\xE2\\x80\\xA8\\xFF");
  const std::string printable = "Jose\xc3\xa9 \xe9\x9b\xaa \xf0\x9f\x90\xb1\n\tpath\\name";
  EXPECT_EQ(detail::terminal_text(printable), printable);
  EXPECT_EQ(safe_text("\x1b\r\a"), "\x1b\r\a");
  EXPECT_EQ(safe_json(Json("\x1b")), "\x1b");
}
TEST(DiagnosticWriter, DiagnosticBudgetsAccountForEscapedHumanText) {
  DiagnosticBuffer buffer(128);
  const std::string controls(100, '\x1b');
  buffer.add(controls);
  std::ostringstream output;
  buffer.flush(output);
  EXPECT_LE(output.str().size(), 128u);
  EXPECT_EQ(output.str().find('\x1b'), std::string::npos);
  EXPECT_NE(output.str().find("\\x1B"), std::string::npos);
  std::string received;
  report_diagnostic([&](const auto &text) { received = text; }, "hook\x1b\r\xff");
  EXPECT_EQ(received, "hook\\x1B\\x0D\\xFF");
}
TEST(DiagnosticWriter, BoundedRenderingKeepsWholeEscapesAndNormalizesOnlyHumanCrLf) {
  const std::string raw = "A\x1b\xe9\x9b\xaa\r\nB\rC\\x1B\xff";
  const std::vector<std::string> tokens{"A",     "\\x1B", "\xe9\x9b\xaa", "\n",   "B",
                                        "\\x0D", "C",     "\\x1B",        "\\xFF"};
  std::string full;
  for (const auto &token : tokens)
    full += token;
  EXPECT_EQ(detail::terminal_text(raw), full);
  EXPECT_EQ(safe_text("a\r\nb\r"), "a\r\nb\r");
  for (size_t limit = 0; limit <= full.size() + 1; ++limit) {
    std::string expected;
    for (const auto &token : tokens) {
      if (token.size() > limit - expected.size())
        break;
      expected += token;
    }
    const auto rendered = detail::render_terminal_text(raw, limit);
    EXPECT_EQ(rendered.text, expected) << limit;
    EXPECT_EQ(rendered.truncated, expected != full) << limit;
    EXPECT_EQ(full.substr(0, detail::rendered_prefix(full, limit)), expected) << limit;
  }
}
TEST(DiagnosticWriter, HumanTextPreservesPrintableAsciiAndEscapesMalformedPrefixesByteForByte) {
  const auto escaped = [](const std::string &text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const auto character : text) {
      const auto byte = static_cast<unsigned char>(character);
      result += "\\x";
      result += hex[byte >> 4u];
      result += hex[byte & 15u];
    }
    return result;
  };
  for (unsigned byte = 0; byte < 128; ++byte) {
    const std::string text(1, static_cast<char>(byte));
    EXPECT_EQ(detail::terminal_text(text),
              byte == '\n' || byte == '\t' || (byte >= 0x20 && byte != 0x7f) ? text
                                                                             : escaped(text));
  }
  for (const std::string bad :
       {"\xc0\xaf", "\xe0\x80", "\xed\xa0\x80", "\xf0\x80\x80", "\xf4\x90\x80\x80", "\xe2\x82"})
    EXPECT_EQ(detail::terminal_text(bad + "X"), escaped(bad) + "X");
}
struct NoticeStream : std::stringbuf {
  size_t writes = 0, flushes = 0;
  bool fail_write = false, fail_flush = false;
  std::streamsize xsputn(const char *bytes, std::streamsize count) override {
    ++writes;
    if (fail_write)
      return 0;
    return std::stringbuf::xsputn(bytes, count);
  }
  int sync() override {
    ++flushes;
    return fail_flush ? -1 : 0;
  }
};
struct RestoreReceiver {
  StateStore &state;
  detail::CliDiagnostics &diagnostics;
  ~RestoreReceiver() {
    diagnostics.install(state);
  }
};
struct Surface final : detail::CliDiagnosticSurface {
  bool full;
  bool &restored;
  size_t &calls;
  DiagnosticSink render;
  Surface(bool fullscreen, bool &closed, size_t &count)
      : full(fullscreen), restored(closed), calls(count) {}
  ~Surface() override {
    restored = true;
  }
  bool fullscreen() const override {
    return full;
  }
  void diagnostic(const std::string &text) override {
    ++calls;
    if (render)
      render(text);
  }
};
struct CliDiagnosticsTest : Test {
  std::ostringstream output;
  TempDirectory temp;
  StateStore state{temp.root / "state"};
  std::shared_ptr<detail::CliDiagnostics> diagnostics =
      std::make_shared<detail::CliDiagnostics>(output);
  std::shared_ptr<Surface> terminal;
  bool restored = false;
  size_t calls = 0;
  void SetUp() override {
    diagnostics->install(state);
  }
  void notice(const std::string &path) {
    auto held = std::make_unique<FileNotice>(path, FileNoticeKind::tool_result);
    held->confirmed = true;
    diagnostics->files->submit(std::move(held));
  }
};
TEST_F(CliDiagnosticsTest, NoticesShareThePartialOrdinaryDiagnosticBoundary) {
  PartialDiagnosticStream buffer;
  std::ostream stream(&buffer);
  detail::CliDiagnostics sink(stream);
  sink.emit("warning");
  buffer.partial = false;
  sink.files->submit(std::make_unique<FileNotice>("held-path", FileNoticeKind::tool_result));
  sink.deliver_files();
  EXPECT_EQ(buffer.bytes,
            "wa\nTool result receipt candidate (publication unconfirmed): held-path\n");
}
TEST_F(CliDiagnosticsTest, FailedLineNoticeIsRetainedForOneEmergencyAttemptAfterRestoration) {
  NoticeStream buffer;
  buffer.fail_write = true;
  std::ostream stream(&buffer);
  auto sink = std::make_shared<detail::CliDiagnostics>(stream);
  RestoreReceiver restore{state, *diagnostics};
  testing::internal::CaptureStderr();
  {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, sink, state);
    scope.activate(std::make_shared<Surface>(false, restored, calls));
    sink->files->submit(std::make_unique<FileNotice>("line-path", FileNoticeKind::tool_result));
    sink->deliver_files();
    sink->deliver_files();
    EXPECT_FALSE(restored);
    EXPECT_EQ(calls, 0u);
    EXPECT_TRUE(buffer.str().empty());
    EXPECT_EQ(buffer.writes, 1u);
  }
  sink->restored(state);
  EXPECT_TRUE(restored);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Tool result receipt candidate (publication unconfirmed): line-path\n");
}
TEST_F(CliDiagnosticsTest, LaterLineNoticesRecoverFromInsertionAndFlushFailuresBeforeExit) {
  for (bool flush_failure : {false, true})
    for (bool throwing : {false, true}) {
      NoticeStream buffer;
      buffer.fail_write = !flush_failure;
      buffer.fail_flush = flush_failure;
      std::ostream stream(&buffer);
      if (throwing)
        stream.exceptions(std::ios::badbit | std::ios::failbit);
      auto sink = std::make_shared<detail::CliDiagnostics>(stream);
      RestoreReceiver restore{state, *diagnostics};
      testing::internal::CaptureStderr();
      {
        detail::TerminalDiagnosticsScope<Surface> scope(terminal, sink, state);
        scope.activate(std::make_shared<Surface>(false, restored, calls));
        sink->files->submit(std::make_unique<FileNotice>("failed", FileNoticeKind::tool_result));
        sink->deliver_files();
        EXPECT_TRUE(stream.good());
        const auto writes = buffer.writes;
        buffer.fail_write = buffer.fail_flush = false;
        sink->files->submit(std::make_unique<FileNotice>("later", FileNoticeKind::tool_result));
        sink->deliver_files();
        EXPECT_GT(buffer.writes, writes);
        EXPECT_TRUE(stream.good());
        EXPECT_THAT(buffer.str(), HasSubstr("later\n"));
        EXPECT_GT(buffer.flushes, 0u);
        sink->deliver_files();
      }
      sink->restored(state);
      EXPECT_EQ(testing::internal::GetCapturedStderr(),
                "Tool result receipt candidate (publication unconfirmed): failed\n");
    }
}
TEST_F(CliDiagnosticsTest, StaleStreamErrorsDoNotPreventNewLineWrites) {
  output.setstate(std::ios::badbit);
  {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, diagnostics, state);
    scope.activate(std::make_shared<Surface>(false, restored, calls));
    notice("new-path");
    diagnostics->deliver_files();
    EXPECT_TRUE(output.good());
    EXPECT_THAT(output.str(), HasSubstr("new-path\n"));
  }
}
TEST_F(CliDiagnosticsTest, OrdinaryWritesRecoverAfterFailureWithoutRequiringAnotherNotice) {
  for (bool throwing : {false, true}) {
    NoticeStream buffer;
    std::ostream stream(&buffer);
    if (throwing)
      stream.exceptions(std::ios::badbit | std::ios::failbit);
    detail::CliDiagnostics sink(stream);
    buffer.fail_write = true;
    if (throwing)
      EXPECT_THROW(sink.emit("failed warning"), std::ios_base::failure);
    else
      EXPECT_NO_THROW(sink.emit("failed warning"));
    EXPECT_TRUE(stream.good());
    buffer.fail_write = false;
    sink.emit("hook warning");
    (void)detail::write_diagnostic(stream, {"[", "tool", "]"});
    const std::string body("raw\0body", 8);
    EXPECT_TRUE(detail::write_diagnostic(stream, {"codet: ", body}));
    EXPECT_EQ(buffer.str(), std::string("\nhook warning\n[tool]\ncodet: ") + body + "\n");
  }
}
TEST_F(CliDiagnosticsTest, FailedNoticeDoesNotSuppressLaterOrdinaryStderrDiagnostics) {
  NoticeStream buffer;
  struct RestoreStderr {
    std::streambuf *original;
    ~RestoreStderr() {
      std::cerr.rdbuf(original);
      detail::recover_diagnostic_stream(std::cerr);
    }
  } restore{std::cerr.rdbuf(&buffer)};
  buffer.fail_write = true;
  EXPECT_THROW(write_file_diagnostic(std::cerr, "failed notice"), Error);
  EXPECT_TRUE(std::cerr.good());
  buffer.fail_write = false;
  report_diagnostic({}, "hook warning");
  detail::CliDiagnostics sink;
  sink.emit("later warning");
  EXPECT_EQ(buffer.str(), "\nhook warning\nlater warning\n");
}
TEST_F(CliDiagnosticsTest, NonblockingStderrFailureRecoversCppAndCStreamStateInAFreshProcess) {
  const auto child = test::start_helper({"--codet-stderr-recovery-helper"});
  ASSERT_GT(child, 0);
  int status = 0;
  ASSERT_TRUE(test::wait_helper(child, status));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}
TEST_F(CliDiagnosticsTest, MissingStreamBufferRecoveryCannotMaskTheOriginalFailure) {
  std::ostream stream(nullptr);
  EXPECT_FALSE(detail::recover_diagnostic_stream(stream));
  EXPECT_THROW(stream.exceptions(std::ios::badbit), std::ios_base::failure);
  EXPECT_FALSE(detail::recover_diagnostic_stream(stream));
  EXPECT_THROW(write_file_diagnostic(stream, "notice"), std::ios_base::failure);
  EXPECT_EQ(stream.exceptions(), std::ios::badbit);
}
TEST_F(CliDiagnosticsTest, RecoveryPreservesTheOriginalRendererExceptionAndExceptionMask) {
  const auto original = std::make_exception_ptr(std::logic_error("output failure"));
  struct ThrowingStream : std::streambuf {
    const std::exception_ptr &failure;
    explicit ThrowingStream(const std::exception_ptr &value) : failure(value) {}
    std::streamsize xsputn(const char *, std::streamsize) override {
      std::rethrow_exception(failure);
    }
  } buffer(original);
  std::ostream stream(&buffer);
  stream.exceptions(std::ios::badbit);
  try {
    (void)detail::write_diagnostic(stream, {"failed"});
    FAIL() << "renderer must throw";
  } catch (...) {
    EXPECT_EQ(std::current_exception(), original);
  }
  EXPECT_TRUE(stream.good());
  EXPECT_EQ(stream.exceptions(), std::ios::badbit);
}
TEST_F(CliDiagnosticsTest, ImmediateFlushFailureUsesEmergencyAndNextNoticeRecovers) {
  NoticeStream buffer;
  buffer.fail_flush = true;
  std::ostream stream(&buffer);
  detail::CliDiagnostics sink(stream);
  RestoreReceiver restore{state, *diagnostics};
  sink.install(state);
  auto session = state.create();
  const auto fail = [&](const std::string &id) {
    ToolCall call{id, "exec_command", Json::object()};
    state.tool_started(session.id, call);
    call.name = "wrong";
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = id;
    EXPECT_THROW(state.complete_tool(session, call, result), Error);
  };
  testing::internal::CaptureStderr();
  fail("failed");
  const auto emergency = testing::internal::GetCapturedStderr();
  EXPECT_THAT(emergency, HasSubstr("Uncommitted tool result preserved at "));
  buffer.fail_flush = false;
  buffer.str("");
  fail("recovered");
  EXPECT_TRUE(stream.good());
  EXPECT_THAT(buffer.str(), HasSubstr("Uncommitted tool result preserved at "));
}
TEST_F(CliDiagnosticsTest, FatalAssertionRestoresTheReceiverBeforeItsLocalStreamIsDestroyed) {
  TestPartResultArray failures;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &failures);
    [&] {
      std::ostringstream local;
      detail::CliDiagnostics sink(local);
      RestoreReceiver restore{state, *diagnostics};
      sink.install(state);
      ASSERT_FALSE(true) << "intentional receiver-lifetime assertion";
    }();
  }
  ASSERT_EQ(failures.size(), 1);
  EXPECT_TRUE(failures.GetTestPartResult(0).fatally_failed());
  auto session = state.create();
  ToolCall call{"after-assertion", "exec_command", Json::object()};
  state.tool_started(session.id, call);
  call.name = "wrong";
  codet::Message result;
  result.role = "tool";
  result.tool_call_id = call.id;
  EXPECT_THROW(state.complete_tool(session, call, result), Error);
  EXPECT_THAT(output.str(), HasSubstr("Uncommitted tool result preserved at "));
}
TEST_F(CliDiagnosticsTest, SuccessfulLineNoticeUsesCheckedStreamWithoutFullscreenReplay) {
  {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, diagnostics, state);
    scope.activate(std::make_shared<Surface>(false, restored, calls));
    notice("line-path");
    diagnostics->deliver_files();
    EXPECT_EQ(output.str(), "Uncommitted tool result preserved at line-path\n");
    EXPECT_EQ(calls, 0u);
  }
  EXPECT_EQ(output.str(), "Uncommitted tool result preserved at line-path\n");
}
TEST_F(CliDiagnosticsTest, NestedScopesRestoreThenReplayLiveCleanupAndFullNoticesOnce) {
  const std::string path = "/Jos\xc3\xa9/" + std::string(2048, 'p');
  {
    detail::TerminalDiagnosticsScope<Surface> outer(terminal, diagnostics, state);
    outer.activate(std::make_shared<Surface>(true, restored, calls));
    diagnostics->emit("live");
    notice(path);
    diagnostics->deliver_files();
    EXPECT_TRUE(output.str().empty());
    {
      detail::TerminalDiagnosticsScope<Surface> cleanup(terminal, diagnostics, state);
      diagnostics->buffering = true;
      diagnostics->emit("SessionEnd cleanup");
    }
    EXPECT_TRUE(restored);
    EXPECT_EQ(output.str(),
              "live\nSessionEnd cleanup\nUncommitted tool result preserved at " + path + "\n");
  }
  EXPECT_EQ(output.str(),
            "live\nSessionEnd cleanup\nUncommitted tool result preserved at " + path + "\n");
}
TEST_F(CliDiagnosticsTest, EarlyFailurePreservesExceptionAndRestoresFailedPreview) {
  const auto original = std::make_exception_ptr(Error("original operation", 42));
  testing::internal::CaptureStderr();
  try {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, diagnostics, state);
    scope.activate(std::make_shared<Surface>(true, restored, calls));
    terminal->render = [](const auto &) { throw std::bad_alloc(); };
    notice("failed-preview");
    diagnostics->deliver_files();
    std::rethrow_exception(original);
  } catch (...) {
    EXPECT_EQ(std::current_exception(), original);
  }
  EXPECT_TRUE(restored);
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Uncommitted tool result preserved at failed-preview\n");
}
TEST_F(CliDiagnosticsTest, RestorationSwitchesActualReceiptFailuresBackToImmediateDelivery) {
  auto session = state.create();
  const auto fail_completion = [&](const std::string &id) {
    ToolCall call{id, "exec_command", Json::object()};
    state.tool_started(session.id, call);
    call.name = "wrong";
    codet::Message result;
    result.role = "tool";
    result.tool_call_id = id;
    EXPECT_THROW(state.complete_tool(session, call, result), Error);
  };
  {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, diagnostics, state);
    scope.activate(std::make_shared<Surface>(true, restored, calls));
    fail_completion("queued");
    EXPECT_TRUE(output.str().empty());
  }
  EXPECT_TRUE(restored);
  EXPECT_THAT(output.str(), HasSubstr("Uncommitted tool result preserved at "));
  output.str("");
  fail_completion("immediate");
  EXPECT_THAT(output.str(), HasSubstr("Uncommitted tool result preserved at "));
  const auto reported = output.str();
  diagnostics->restored(state);
  EXPECT_EQ(output.str(), reported);
}
struct FailingStream : std::stringbuf {
  std::string marker;
  bool fired = false;
  explicit FailingStream(std::string match) : marker(std::move(match)) {}
  std::streamsize xsputn(const char *bytes, std::streamsize count) override {
    if (!fired && std::string(bytes, static_cast<size_t>(count)) == marker) {
      fired = true;
      throw std::bad_alloc();
    }
    return std::stringbuf::xsputn(bytes, count);
  }
};
TEST_F(CliDiagnosticsTest, ReplayFailureDoesNotSkipAnotherBufferOrFileNotices) {
  for (const auto *failed : {"live", "cleanup"}) {
    FailingStream buffer(failed);
    std::ostream stream(&buffer);
    stream.exceptions(std::ios::badbit);
    auto sink = std::make_shared<detail::CliDiagnostics>(stream);
    RestoreReceiver restore{state, *diagnostics};
    sink->live.add("live");
    sink->cleanup.add("cleanup");
    sink->files->submit(std::make_unique<FileNotice>("retained", FileNoticeKind::repair_backup));
    sink->restored(state);
    ASSERT_TRUE(buffer.fired);
    EXPECT_THAT(buffer.str(), HasSubstr(std::string(failed) == "live" ? "cleanup\n" : "live\n"));
    EXPECT_THAT(buffer.str(),
                HasSubstr("Session repair backup candidate (publication unconfirmed): retained\n"));
    const auto first = buffer.str();
    sink->restored(state);
    EXPECT_EQ(buffer.str(), first);
  }
}
TEST_F(CliDiagnosticsTest, PersistentStreamFailureStillFinishesNoticeAndPreservesCleanupException) {
  struct BrokenStream : std::streambuf {
    std::streamsize xsputn(const char *, std::streamsize) override {
      return 0;
    }
  } buffer;
  std::ostream stream(&buffer);
  stream.exceptions(std::ios::badbit);
  auto sink = std::make_shared<detail::CliDiagnostics>(stream);
  RestoreReceiver restore{state, *diagnostics};
  const auto original = std::make_exception_ptr(Error("SessionEnd failure"));
  testing::internal::CaptureStderr();
  try {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, sink, state);
    scope.activate(std::make_shared<Surface>(true, restored, calls));
    sink->live.add("live");
    sink->cleanup.add("cleanup");
    sink->files->submit(std::make_unique<FileNotice>("persistent", FileNoticeKind::repair_backup));
    std::rethrow_exception(original);
  } catch (...) {
    EXPECT_EQ(std::current_exception(), original);
  }
  sink->restored(state);
  EXPECT_TRUE(restored);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Session repair backup candidate (publication unconfirmed): persistent\n");
}
} // namespace
