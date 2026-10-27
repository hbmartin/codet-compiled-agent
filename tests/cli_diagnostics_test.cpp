#include "codet/detail/approval_identity.hpp"
#include "codet/detail/approval_review.hpp"
#include "codet/detail/cleanup_diagnostics.hpp"
#include "codet/detail/cli_diagnostics.hpp"
#include "native_io_metrics.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>

using namespace codet;
using namespace testing;
namespace {
TEST(ApprovalIdentity, RequiresCompleteTypedIdentityAndNeverDefaultsApproval) {
  const Json valid = {
      {"codet_session_id", "session"}, {"codet_turn", uint64_t{3}}, {"request_id", "request"}};
  const auto identity = detail::ApprovalIdentity::from(valid);
  EXPECT_TRUE(identity.matches(valid));
  for (const auto *key : {"codet_session_id", "codet_turn", "request_id"}) {
    auto missing = valid;
    missing.erase(key);
    EXPECT_THROW(detail::ApprovalIdentity::from(missing), Error);
    EXPECT_FALSE(identity.matches(missing));
    auto wrong = valid;
    wrong[key] = Json::array();
    EXPECT_THROW(detail::ApprovalIdentity::from(wrong), Error);
    EXPECT_FALSE(identity.matches(wrong));
  }
  for (Json turn : {Json(0), Json(-1), Json(1.5)}) {
    auto invalid = valid;
    invalid["codet_turn"] = turn;
    EXPECT_THROW(detail::ApprovalIdentity::from(invalid), Error);
  }
  auto other = valid;
  other["request_id"] = "expired";
  EXPECT_FALSE(identity.matches(other));
  other = valid;
  other["codet_session_id"] = "different";
  EXPECT_FALSE(identity.matches(other));
  other = valid;
  other["codet_turn"] = 4;
  EXPECT_FALSE(identity.matches(other));
  for (const auto *answer : {"y", "yes"})
    EXPECT_TRUE(detail::approves(answer));
  for (const auto *answer : {"Y", "YES", " y", "yes ", "n", "/quit", ""})
    EXPECT_FALSE(detail::approves(answer));
}
TEST(ApprovalReview, EveryPageMustRenderAndResizeRestartsReview) {
  detail::ApprovalReview review("session", 3, "request", std::string(3000, 'a') + "suffix");
  EXPECT_TRUE(review.reshape(8, 40));
  ASSERT_GT(review.pages(), 2u);
  EXPECT_FALSE(review.approved_review());
  review.next();
  EXPECT_EQ(review.page(), 0u);
  while (review.page() + 1 < review.pages()) {
    review.displayed();
    EXPECT_FALSE(review.approved_review());
    review.next();
  }
  EXPECT_FALSE(review.approved_review());
  review.displayed();
  EXPECT_TRUE(review.approved_review());
  review.previous();
  EXPECT_TRUE(review.approved_review());
  EXPECT_FALSE(review.reshape(8, 40));
  EXPECT_TRUE(review.approved_review());
  EXPECT_TRUE(review.reshape(8, 30));
  EXPECT_EQ(review.page(), 0u);
  EXPECT_FALSE(review.approved_review());
  EXPECT_TRUE(review.reshape(3, 5));
  EXPECT_FALSE(review.usable());
  review.displayed();
  EXPECT_FALSE(review.approved_review());
}
TEST(ApprovalReview, WrappingRetainsCompleteUnicodeAndEscapeTokensBeyondTranscriptLimits) {
  std::string source;
  for (size_t i = 0; i != 1100; ++i)
    source += "雪\\x0A ";
  const auto document = detail::literal_text(source + "\r\n\t\xff");
  const auto expected = detail::CursesText(document).text();
  for (int columns : {20, 40, 80}) {
    detail::ApprovalReview review("session", 1, "request", document);
    review.reshape(6, columns);
    std::string reconstructed;
    for (;;) {
      for (size_t i = 0; i < review.capacity(); ++i) {
        const auto row = review.line(i);
        EXPECT_EQ(safe_text(std::string(row)), row);
        EXPECT_EQ(row.size(),
                  detail::rendered_prefix(expected.substr(reconstructed.size()), row.size()));
        reconstructed += row;
      }
      review.displayed();
      if (review.approved_review())
        break;
      review.next();
    }
    EXPECT_EQ(reconstructed, expected);
  }
}
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
TEST(DiagnosticWriter, TurnCleanupKeepsOriginalFailureAfterQuiescenceAndResetsCancellation) {
  for (bool quitting : {false, true}) {
    PartialDiagnosticStream buffer;
    std::ostream stream(&buffer);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    detail::DiagnosticWriter writer(stream);
    Cancellation cancel;
    std::atomic<bool> finished{false};
    auto worker = std::async(std::launch::async, [&] {
      while (!cancel.requested)
        std::this_thread::yield();
      finished = true;
    });
    const auto original = std::make_exception_ptr(std::runtime_error("original failure"));
    unsigned reports = 0;
    try {
      detail::recover_turn(
          original, cancel, quitting, worker,
          [&] {
            EXPECT_TRUE(finished);
            throw std::bad_alloc();
          },
          [&] { throw std::runtime_error("drain failure"); },
          [&](std::string_view text) {
            if (++reports == 1)
              (void)writer.write({text});
            else
              throw std::bad_alloc(); // Formatting is secondary too.
          });
    } catch (...) {
      EXPECT_EQ(std::current_exception(), original);
    }
    EXPECT_TRUE(finished);
    EXPECT_EQ(reports, 2u);
    EXPECT_EQ(cancel.requested.load(), quitting);
    if (!quitting)
      EXPECT_NO_THROW(cancel.check());
    worker.get();
  }
}
#ifdef CODET_NATIVE_TERMINAL_METRICS
TEST(DiagnosticWriter, EmergencyChunksShareEightInterruptionRetriesWithInspectionAndSeparator) {
  FileNotice notice("/" + std::string(4097, 'a') + "\xff", FileNoticeKind::tool_result);
  notice.confirmed = true;
  const auto expected = notice.text() + "\n";
  for (bool exhausted : {false, true}) {
    detail::reset_standard_diagnostics();
    testing::internal::CaptureStderr();
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.stats_interrupted = 4;
    metrics.writes_interrupted = exhausted ? 5 : 4;
    emergency_file_diagnostic(notice);
    const auto writes = metrics.writes;
    metrics = {};
    const auto output = testing::internal::GetCapturedStderr();
    if (exhausted) {
      EXPECT_TRUE(output.empty());
      EXPECT_EQ(writes, 5u);
    } else {
      EXPECT_EQ(output, expected);
      EXPECT_GT(writes, 8u); // Positive progress through every path chunk does not spend retries.
    }
  }
  detail::reset_standard_diagnostics();
  testing::internal::CaptureStderr();
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.stats_interrupted = 9;
  EXPECT_FALSE(detail::immediate_stderr({"must not be written"}));
  EXPECT_EQ(metrics.inspections, 9u);
  EXPECT_EQ(metrics.writes, 0u);
  EXPECT_EQ(metrics.vectors, 0u);
  metrics = {};
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
  detail::reset_standard_diagnostics();
}
TEST(DiagnosticWriter, ImmediateVectorOutputRetriesInterruptedCallsAndStopsAtItsBudget) {
  for (unsigned interruptions : {1u, 8u, 9u}) {
    detail::reset_standard_diagnostics();
    testing::internal::CaptureStderr();
    auto &metrics = test::native_io_metrics;
    metrics.interrupt_fd = STDERR_FILENO;
    metrics.vectors_interrupted = interruptions;
    EXPECT_EQ(detail::immediate_stderr({"first", "second"}), interruptions <= 8);
    EXPECT_EQ(metrics.vectors, std::min(interruptions + 1, 9u));
    metrics = {};
    EXPECT_EQ(testing::internal::GetCapturedStderr(), interruptions <= 8 ? "firstsecond\n" : "");
  }
  detail::reset_standard_diagnostics();
}
TEST(DiagnosticWriter, InterruptedIdentityInspectionCannotEraseAnAlreadyOwedSeparator) {
  detail::reset_standard_diagnostics();
  testing::internal::CaptureStderr();
  EXPECT_EQ(::write(STDERR_FILENO, "fi", 2), 2);
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.vectors_interrupted = 9;
  EXPECT_FALSE(detail::write_diagnostic(std::cerr, {"rst"}));
  metrics.stats_interrupted = 9;
  EXPECT_FALSE(detail::immediate_stderr({"inspection fails before delivery"}));
  metrics = {};
  EXPECT_TRUE(detail::write_diagnostic(std::cerr, {"later"}));
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "fi\nlater\n");
  detail::reset_standard_diagnostics();
}
TEST(DiagnosticWriter, ImmediatePipeInspectionReadinessAndWritesShareOneRetryBudget) {
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  const int saved = ::dup(STDERR_FILENO);
  ASSERT_GE(saved, 0);
  struct Restore {
    int saved, read, write;
    ~Restore() {
      test::native_io_metrics = {};
      (void)::dup2(saved, STDERR_FILENO);
      ::close(saved);
      ::close(read);
      ::close(write);
      detail::reset_standard_diagnostics();
    }
  } restore{saved, descriptors[0], descriptors[1]};
  ASSERT_EQ(::dup2(descriptors[1], STDERR_FILENO), STDERR_FILENO);
  detail::reset_standard_diagnostics();
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  metrics.stats_interrupted = metrics.bounds_interrupted = metrics.polls_interrupted =
      metrics.vectors_interrupted = 2;
  EXPECT_TRUE(detail::immediate_stderr({"checked pipe"}));
  EXPECT_EQ(metrics.vectors, 3u);
  metrics = {};
  char bytes[64]{};
  ASSERT_EQ(::read(descriptors[0], bytes, sizeof(bytes)), 13);
  EXPECT_EQ(std::string(bytes, 13), "checked pipe\n");
}
TEST(DiagnosticWriter, InspectionRefreshesFlagsAndCachesPipeBoundsOnlyForTheCurrentDestination) {
  int first[2], second[2];
  ASSERT_EQ(::pipe(first), 0);
  ASSERT_EQ(::pipe(second), 0);
  const int saved = ::dup(STDERR_FILENO);
  ASSERT_GE(saved, 0);
  struct Restore {
    int saved, first[2], second[2];
    ~Restore() {
      test::native_io_metrics = {};
      (void)::dup2(saved, STDERR_FILENO);
      ::close(saved);
      for (int fd : first)
        ::close(fd);
      for (int fd : second)
        ::close(fd);
      detail::reset_standard_diagnostics();
    }
  } restore{saved, {first[0], first[1]}, {second[0], second[1]}};
  ASSERT_EQ(::dup2(first[1], STDERR_FILENO), STDERR_FILENO);
  detail::reset_standard_diagnostics();
  auto &metrics = test::native_io_metrics;
  metrics.interrupt_fd = STDERR_FILENO;
  detail::DiagnosticWriter writer(std::cerr);
  EXPECT_TRUE(writer.write({"first"}));
  EXPECT_EQ(metrics.inspections, 1u);
  EXPECT_EQ(metrics.bound_queries, 1u);
  const int flags = ::fcntl(STDERR_FILENO, F_GETFL);
  ASSERT_GE(flags, 0);
  ASSERT_EQ(::fcntl(STDERR_FILENO, F_SETFL, flags | O_NONBLOCK), 0);
  EXPECT_TRUE(writer.write({"nonblocking"}));
  ASSERT_EQ(::fcntl(STDERR_FILENO, F_SETFL, flags), 0);
  EXPECT_TRUE(writer.write({"blocking again"}));
  EXPECT_EQ(metrics.inspections, 3u);
  EXPECT_EQ(metrics.bound_queries, 1u);
  ASSERT_EQ(::dup2(second[1], STDERR_FILENO), STDERR_FILENO);
  EXPECT_TRUE(writer.write({"new destination"}));
  EXPECT_EQ(metrics.inspections, 4u);
  EXPECT_EQ(metrics.bound_queries, 2u);
}
#endif
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
TEST(DiagnosticWriter, NativeDescriptorFailureKeepsConfiguredExceptionMasksAndRecovers) {
  const int saved = ::dup(STDERR_FILENO);
  ASSERT_GE(saved, 0);
  bool threw = false, recovered = false, mask_preserved = false;
  {
    const auto mask = std::cerr.exceptions();
    struct Restore {
      int saved;
      std::ios::iostate mask;
      ~Restore() {
        std::cerr.exceptions(std::ios::goodbit);
        (void)::dup2(saved, STDERR_FILENO);
        ::close(saved);
        detail::recover_diagnostic_stream(std::cerr);
        std::cerr.exceptions(mask);
        detail::reset_standard_diagnostics();
      }
    } restore{saved, mask};
    ::close(STDERR_FILENO);
    std::cerr.exceptions(std::ios::badbit);
    detail::DiagnosticWriter writer(std::cerr);
    try {
      (void)writer.write({"unwritable"});
    } catch (const std::ios_base::failure &) {
      threw = true;
    }
    recovered = std::cerr.good();
    mask_preserved = std::cerr.exceptions() == std::ios::badbit;
  }
  EXPECT_TRUE(threw);
  EXPECT_TRUE(recovered);
  EXPECT_TRUE(mask_preserved);
}
TEST(DiagnosticWriter, PermissionChangesOnTheSameDescriptorIdentityKeepTheSeparator) {
  TempDirectory temporary;
  const auto path = temporary.root / "diagnostic";
  { std::ofstream create(path); }
  const int saved = ::dup(STDERR_FILENO);
  ASSERT_GE(saved, 0);
  struct Restore {
    int descriptor;
    ~Restore() {
      (void)::dup2(descriptor, STDERR_FILENO);
      ::close(descriptor);
      detail::reset_standard_diagnostics();
      detail::recover_diagnostic_stream(std::cerr);
    }
  } restore{saved};
  const int read_only = ::open(path.c_str(), O_RDONLY);
  ASSERT_GE(read_only, 0);
  ASSERT_EQ(::dup2(read_only, STDERR_FILENO), STDERR_FILENO);
  ::close(read_only);
  detail::DiagnosticWriter writer(std::cerr);
  EXPECT_FALSE(writer.write({"indeterminate"}));
  ASSERT_EQ(::chmod(path.c_str(), 0640), 0);
  const int writable = ::open(path.c_str(), O_WRONLY | O_APPEND);
  ASSERT_GE(writable, 0);
  ASSERT_EQ(::dup2(writable, STDERR_FILENO), STDERR_FILENO);
  ::close(writable);
  EXPECT_TRUE(writer.write({"later"}));
  std::ifstream read(path);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(read), {}), "\nlater\n");
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
TEST(DiagnosticWriter, LiteralIdentifiersDistinguishControlsAndEscapeLookingBytes) {
  const std::string raw = "command\r\nnext\t\\x0A 雪";
  EXPECT_EQ(detail::terminal_text(raw), "command\nnext\t\\x0A 雪");
  const auto expected = "command\\x0D\\x0Anext\\x09\\x5Cx0A 雪";
  EXPECT_EQ(detail::literal_text(raw), expected);
  for (size_t limit = 0; limit <= std::strlen(expected); ++limit) {
    const auto bounded = detail::render_terminal_text(raw, limit, detail::RenderMode::literal);
    EXPECT_EQ(bounded.text,
              std::string(expected).substr(0, detail::rendered_prefix(expected, limit)));
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
