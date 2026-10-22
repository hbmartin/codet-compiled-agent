#include "../src/modern/cli_diagnostics.hpp"
#include "temp.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace codet;
using namespace testing;
namespace {
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
TEST_F(CliDiagnosticsTest, FailedLineNoticeIsRetainedForOneEmergencyAttemptAfterRestoration) {
  testing::internal::CaptureStderr();
  {
    detail::TerminalDiagnosticsScope<Surface> scope(terminal, diagnostics, state);
    scope.activate(std::make_shared<Surface>(false, restored, calls));
    notice("line-path");
    output.setstate(std::ios::badbit);
    diagnostics->deliver_files();
    diagnostics->deliver_files();
    EXPECT_FALSE(restored);
    EXPECT_EQ(calls, 0u);
    EXPECT_TRUE(output.str().empty());
  }
  diagnostics->restored(state);
  EXPECT_TRUE(restored);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "Uncommitted tool result preserved at line-path\n");
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
    diagnostics->install(state);
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
  diagnostics->install(state);
}
} // namespace
