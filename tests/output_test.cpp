#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "core_tools.hpp"
#include "temp.hpp"
#include <cerrno>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
struct OutputReview : Test {
  TempDirectory temp;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  Config config;
  void SetUp() override {
    config.cwd = temp.root / "work";
    config.home = temp.root / "home";
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.tool_output_limit = 512;
    fs::create_directories(config.cwd);
    fs::create_directories(config.home);
  }
  ProcessSpec command(const std::string &text) {
    ProcessSpec s;
    s.argv = {"/bin/sh", "-c", text};
    s.cwd = config.cwd;
    s.output_limit = 512;
    s.spool_directory = config.home / "tool-output";
    return s;
  }
  ToolResult shell(Tools &tools, const std::string &text) {
    return execute_core(tools, config,
                        {unique_id(), "exec_command", {{"cmd", text}, {"login", false}}}, cancel);
  }
  size_t captures(const fs::path &directory) {
    size_t n = 0;
    if (fs::exists(directory))
      for (const auto &p : fs::directory_iterator(directory))
        if (p.path().extension() == ".txt" && p.is_regular_file())
          ++n;
    return n;
  }
};
TEST_F(OutputReview, ToolPreviewCountsOriginalBytesAndDoesNotCutItsOwnMarker) {
  const std::string source = "HEAD" + std::string(1500000, 'x') + "TAIL";
  files.atomic_write(config.cwd / "source", source);
  Tools tools(config, files, processes, http, clock);
  const auto r = shell(tools, "cat source");
  ASSERT_FALSE(r.error) << r.content;
  const auto marker = r.content.find("\n[... "), end = r.content.find(" bytes omitted ...]\n");
  ASSERT_NE(marker, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  const auto omitted = std::stoull(r.content.substr(marker + 6, end - marker - 6));
  EXPECT_EQ(r.metadata["omitted_output_bytes"], omitted);
  EXPECT_GT(omitted, 1400000u);
  EXPECT_EQ(r.metadata["total_output_bytes"], source.size());
  const auto notice = r.content.find("\n[Output saved");
  ASSERT_NE(notice, std::string::npos);
  const auto retained = marker + notice - (end + std::string(" bytes omitted ...]\n").size());
  EXPECT_EQ(source.size() - omitted, retained);
  EXPECT_THAT(r.content, StartsWith("HEAD"));
  EXPECT_THAT(r.content, HasSubstr("TAIL"));
  EXPECT_LE(r.content.size(), 512u);
  EXPECT_EQ(files.read(r.metadata["spill_path"].get<std::string>()), source);
}
TEST_F(OutputReview, UnicodeCutsAndRepairExpansionHaveAccurateSourceCounts) {
  for (const auto &character : {std::string("雪"), std::string("🌲"), std::string("e\xcc\x81")}) {
    std::string original;
    for (int i = 0; i < 1000; ++i)
      original += character;
    for (size_t chunk = 1; chunk <= 7; ++chunk) {
      auto store = std::make_shared<CaptureStore>(config.home / ("chunks-" + unique_id()));
      OutputCapture capture(store, 512);
      for (size_t at = 0; at < original.size(); at += chunk)
        capture.append(original.substr(at, chunk));
      capture.finish();
      for (size_t limit = 100; limit < 112; ++limit) {
        const auto r = capture.render(limit);
        EXPECT_LE(r.text.size(), limit);
        EXPECT_NO_THROW(Json(r.text).dump());
        EXPECT_THAT(r.text, Not(HasSubstr("\xef\xbf\xbd")));
        const auto begin = r.text.find("\n[... "), end = r.text.find(" bytes omitted ...]\n");
        ASSERT_NE(begin, std::string::npos);
        ASSERT_NE(end, std::string::npos);
        const auto kept = begin + r.text.size() - end - std::string(" bytes omitted ...]\n").size();
        EXPECT_EQ(original.size() - r.omitted, kept);
      }
      ProcessResult retained;
      capture.metadata(retained, original.size());
      EXPECT_EQ(files.read(retained.spill_path), original);
    }
  }
  OutputCapture invalid(std::make_shared<CaptureStore>(config.home / "invalid"), 512);
  invalid.append(std::string(300, '\xff'));
  invalid.finish();
  const auto r = invalid.render(512);
  const auto first = r.text.find("\n[... "), last = r.text.find(" bytes omitted ...]\n") +
                                                    std::string(" bytes omitted ...]\n").size();
  EXPECT_EQ((first + r.text.size() - last) / 3, 300u - r.omitted);
}
TEST_F(OutputReview, LongUnicodePathsUseCompleteNoticeOrMetadataFallback) {
  for (size_t limit = 512; limit < 520; ++limit) {
    config.tool_output_limit = limit;
    config.capture_root = temp.root / std::string(180, 'p') / "雪-output";
    std::string original;
    for (int i = 0; i < 3000; ++i)
      original += "雪";
    files.atomic_write(config.cwd / "source", original);
    Tools tools(config, files, processes, http, clock);
    auto r = shell(tools, "cat source");
    ASSERT_FALSE(r.error);
    EXPECT_LE(r.content.size(), limit);
    EXPECT_THAT(r.content, EndsWith("]"));
    EXPECT_THAT(r.content, HasSubstr("metadata.spill_path"));
    EXPECT_NO_THROW(Json(r.content).dump());
    EXPECT_EQ(files.read(r.metadata["spill_path"].get<std::string>()), original);
  }
}
struct FaultIo : CaptureIo {
  size_t remaining = 12;
  bool interrupt_once = true, sync_error = false;
  ssize_t write(int fd, const char *text, size_t size) override {
    if (interrupt_once) {
      interrupt_once = false;
      errno = EINTR;
      return -1;
    }
    if (!remaining) {
      errno = EIO;
      return -1;
    }
    const auto n = ::write(fd, text, std::min(size, remaining));
    if (n > 0)
      remaining -= static_cast<size_t>(n);
    return n;
  }
  int sync(int fd) override {
    if (sync_error) {
      errno = EIO;
      return -1;
    }
    return CaptureIo::sync(fd);
  }
};
TEST_F(OutputReview, PartialCaptureFailureKeepsRealStatusAndPrefix) {
  auto io = std::make_shared<FaultIo>();
  auto store =
      std::make_shared<CaptureStore>(config.home / "partial", 8192, 8192, 64, DiagnosticSink{}, io);
  auto spec = command("cat source; exit 7");
  spec.capture_store = store;
  files.atomic_write(config.cwd / "source", std::string(3000, 'x') + "TAIL");
  const auto r = processes.run(spec, cancel);
  EXPECT_EQ(r.exit_code, 7);
  EXPECT_THAT(r.output, HasSubstr("TAIL"));
  EXPECT_EQ(r.spill_bytes, 12u);
  EXPECT_TRUE(r.spill_truncated);
  EXPECT_FALSE(r.capture_error.empty());
  EXPECT_EQ(files.read(r.spill_path), std::string(12, 'x'));
}
struct SmallWarning : Processes {
  ProcessResult run(const ProcessSpec &, const Cancellation &) override {
    ProcessResult r(0, "complete");
    r.capture_error = "test storage failure";
    r.spill_truncated = true;
    return r;
  }
  std::string start(const ProcessSpec &) override {
    return "unused";
  }
  ProcessResult poll(const std::string &) override {
    Cancellation cancel;
    return run({}, cancel);
  }
  void stop(const std::string &) override {}
};
TEST_F(OutputReview, CaptureWarningCannotTruncateCompleteSmallOutput) {
  SmallWarning fake;
  std::vector<std::string> diagnostics;
  Tools tools(config, files, fake, http, clock, {}, {},
              [&](const auto &s) { diagnostics.push_back(s); });
  const auto r = shell(tools, "printf complete");
  EXPECT_FALSE(r.error);
  EXPECT_EQ(r.content, "complete");
  EXPECT_EQ(r.metadata["exit_code"], 0);
  EXPECT_EQ(r.metadata["truncated"], false);
  EXPECT_EQ(r.metadata["omitted_output_bytes"], 0);
  EXPECT_EQ(r.metadata["spill_truncated"], true);
  EXPECT_THAT(diagnostics, Contains(HasSubstr("test storage failure")));
}
TEST_F(OutputReview, BadEntriesDoNotDisableHealthyCaptureAndLegacyFilesCount) {
  const auto dir = config.home / "store";
  fs::create_directories(dir);
  files.atomic_write(dir / "outside", "secret");
  fs::create_symlink(dir / "outside", dir / "capture-link.txt");
  fs::create_directory(dir / "capture-directory.txt");
  const auto legacy = dir / (unique_id() + ".txt");
  files.atomic_write(legacy, std::string(20, 'l'), true);
  const timeval old_times[2] = {{1, 0}, {1, 0}};
  ASSERT_EQ(::utimes(legacy.c_str(), old_times), 0);
  std::vector<std::string> diagnostics;
  auto store = std::make_shared<CaptureStore>(dir, 8, 32, 1,
                                              [&](const auto &s) { diagnostics.push_back(s); });
  OutputSpool spool(store);
  spool.append("newbytes");
  spool.flush();
  EXPECT_EQ(files.read(legacy), std::string(20, 'l'));
  EXPECT_EQ(spool.bytes(), 8u);
  spool.close();
  EXPECT_FALSE(fs::exists(legacy));
  EXPECT_TRUE(spool.error().empty());
  EXPECT_TRUE(fs::is_symlink(dir / "capture-link.txt"));
  EXPECT_TRUE(fs::is_directory(dir / "capture-directory.txt"));
  EXPECT_EQ(files.read(dir / "outside"), "secret");
  EXPECT_THAT(diagnostics, Contains(HasSubstr("Ignoring nonregular entry")));
  OutputSpool another(store);
  another.append("another");
  another.close();
  EXPECT_TRUE(another.error().empty());
}
TEST_F(OutputReview, BatchedWritesAvoidPerChunkScanningAndSyncOnlyAtCompletion) {
  auto store = std::make_shared<CaptureStore>(config.home / "batches");
  OutputSpool spool(store);
  for (int i = 0; i < 128; ++i)
    spool.append(std::string(8192, 'x'));
  EXPECT_EQ(store->metrics.batches, 16u);
  EXPECT_EQ(store->metrics.syncs, 0u);
  EXPECT_LE(spool.memory_bytes(), OutputSpool::batch_bytes);
  spool.close();
  EXPECT_EQ(store->metrics.syncs, 1u);
  EXPECT_LE(store->metrics.scans, 18u);
  EXPECT_EQ(files.read(spool.path()), std::string(1048576, 'x'));
  EXPECT_LT(spool.memory_bytes(), 32u);
}
TEST_F(OutputReview, SmallCommandsCannotEvictAnOlderLargeCapture) {
  files.atomic_write(config.cwd / "source", std::string(4000, 'x'));
  Tools tools(config, files, processes, http, clock);
  const auto first = shell(tools, "cat source");
  const auto path = first.metadata["spill_path"].get<std::string>();
  for (int i = 0; i < 70; ++i) {
    const auto r = shell(tools, "printf small");
    EXPECT_EQ(r.content, "small");
    EXPECT_FALSE(r.metadata.contains("spill_path"));
  }
  EXPECT_TRUE(fs::exists(path));
  EXPECT_EQ(captures(config.home / "tool-output"), 1u);
}
TEST_F(OutputReview, GenericSpillsShareTheManagedQuotaAndNewPerFileLimit) {
  const auto dir = config.home / "tool-output";
  fs::create_directories(dir);
  const auto legacy = dir / (unique_id() + ".txt");
  const int fd = ::open(legacy.c_str(), O_CREAT | O_WRONLY, 0600);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(::ftruncate(fd, 64 * 1024 * 1024), 0);
  ::close(fd);
  struct Desktop : ComputerBackend {
    Json capabilities() const override {
      return {{"available", true}, {"clipboard", true}};
    }
    DesktopResult invoke(const std::string &, const Json &, const Cancellation &) override {
      return {{{"text", std::string(9 * 1024 * 1024, 'x')}}, {}};
    }
  };
  Tools tools(config, files, processes, http, clock, {}, {}, {}, {}, std::make_shared<Desktop>());
  const auto result =
      tools.execute({"clipboard", "computer.clipboard_read", Json::object()}, cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_FALSE(fs::exists(legacy));
  const auto path = result.metadata.at("spill_path").get<std::string>();
  EXPECT_EQ(fs::file_size(path), 8u * 1024 * 1024);
  EXPECT_EQ(result.metadata["spill_truncated"], true);
  EXPECT_GT(result.metadata["total_output_bytes"], 9u * 1024 * 1024);
}
TEST_F(OutputReview, HookFeedbackCanActivateCaptureWithoutLosingOriginalBytes) {
  const std::string source(490, 'x');
  files.atomic_write(config.cwd / "source", source);
  config.hooks["PostToolUse"] =
      Json::array({{{"matcher", "Bash"}, {"command", "printf '%0100d' 0"}}});
  // Plain PostToolUse stdout is diagnostic-only; use accepted additionalContext instead.
  config.hooks["PostToolUse"][0]["command"] =
      "printf '%s' '{\"hookSpecificOutput\":{\"additionalContext\":\"context-for-result\"}}'";
  Tools tools(config, files, processes, http, clock);
  const auto r = shell(tools, "cat source");
  ASSERT_FALSE(r.error);
  EXPECT_GT(r.metadata["omitted_output_bytes"].get<size_t>(), 0u);
  EXPECT_EQ(files.read(r.metadata["spill_path"].get<std::string>()), source);
  EXPECT_THAT(r.content, HasSubstr("context-for-result"));
  EXPECT_LE(r.content.size(), 512u);
}
TEST_F(OutputReview, InternalPollingNeitherFormatsNorFlushesUnchangedOutput) {
  files.atomic_write(config.cwd / "source", std::string(1024, 'x'));
  auto store = std::make_shared<CaptureStore>(config.home / "internal");
  auto spec =
      command("cat source; printf ready > ready; while test ! -f release; do sleep .01; done");
  spec.capture_store = store;
  const auto id = processes.start(spec, cancel);
  for (int i = 0; i < 500 && !fs::exists(config.cwd / "ready"); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_TRUE(fs::exists(config.cwd / "ready"));
  for (int i = 0; i < 30; ++i)
    (void)processes.list();
  EXPECT_EQ(store->metrics.renders, 0u);
  EXPECT_EQ(store->metrics.batches, 0u);
  const auto snapshot = processes.poll(id);
  EXPECT_EQ(store->metrics.renders, 1u);
  EXPECT_EQ(snapshot.spill_bytes, 1024u);
  EXPECT_EQ(files.read(snapshot.spill_path), std::string(1024, 'x'));
  files.atomic_write(config.cwd / "release", "go");
  processes.stop(id);
}
TEST_F(OutputReview, CompletedRetentionCountsActualWindowAndStagingCapacities) {
  files.atomic_write(config.cwd / "source", std::string(131072, 'x'));
  auto spec = command("cat source");
  spec.spool_directory.clear();
  spec.output_limit = 65536;
  for (int i = 0; i < 70; ++i) {
    const auto id = processes.start(spec, cancel);
    while (processes.poll(id).running)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_LE(processes.retained_output_bytes(), 4u * 1024 * 1024);
  }
  EXPECT_LE(processes.list().size(), 64u);
}
TEST_F(OutputReview, InvocationCaptureRootAllowsReadPagingButNotWritesOrDeniedReads) {
  config.capture_root = temp.root / "private-output";
  files.atomic_write(config.cwd / "source", std::string(2000, 'x'));
  Tools tools(config, files, processes, http, clock);
  const auto r = shell(tools, "cat source");
  const auto path = r.metadata["spill_path"].get<std::string>();
  config.permission_mode = "accept-edits";
  EXPECT_FALSE(execute_core(tools, config, {"page", "read_file", {{"path", path}}}, cancel).error);
  EXPECT_TRUE(execute_core(tools, config,
                           {"write", "apply_patch", {{"path", path}, {"content", "changed"}}},
                           cancel)
                  .error);
  config.set_deny({"read_file(*capture-*)"});
  EXPECT_TRUE(execute_core(tools, config, {"denied", "read_file", {{"path", path}}}, cancel).error);
}
TEST_F(OutputReview, ActiveCaptureQuotaCannotBeStolenByAnotherCapture) {
  auto store = std::make_shared<CaptureStore>(config.home / "full", 32, 32, 64);
  OutputSpool active(store);
  active.append(std::string(32, 'a'));
  active.flush();
  OutputSpool other(store);
  other.append("no room");
  other.close();
  EXPECT_EQ(other.bytes(), 0u);
  EXPECT_TRUE(other.truncated());
  EXPECT_TRUE(fs::exists(active.path()));
  EXPECT_EQ(files.read(active.path()), std::string(32, 'a'));
  active.close();
  OutputSpool next(store);
  next.append("new");
  next.close();
  EXPECT_EQ(next.bytes(), 3u);
  EXPECT_FALSE(fs::exists(active.path()));
}
TEST_F(OutputReview, UnreadableRegularEntryCountsWithoutDisablingHealthyCapture) {
  if (::geteuid() == 0)
    GTEST_SKIP() << "Root can open mode-000 files";
  const auto dir = config.home / "unreadable";
  fs::create_directories(dir);
  const auto bad = dir / "capture-unreadable.txt";
  files.atomic_write(bad, std::string(20, 'x'), true);
  ASSERT_EQ(::chmod(bad.c_str(), 0000), 0);
  std::vector<std::string> diagnostics;
  auto store = std::make_shared<CaptureStore>(dir, 32, 32, 64,
                                              [&](const auto &s) { diagnostics.push_back(s); });
  OutputSpool capture(store);
  capture.append(std::string(20, 'y'));
  capture.close();
  EXPECT_EQ(capture.bytes(), 12u);
  EXPECT_TRUE(capture.truncated());
  EXPECT_THAT(capture.error(), HasSubstr("capacity exhausted"));
  EXPECT_TRUE(fs::exists(bad));
  EXPECT_FALSE(diagnostics.empty());
  EXPECT_EQ(fs::file_size(bad), 20u);
  EXPECT_EQ(fs::file_size(capture.path()), 12u);
}
TEST_F(OutputReview, SyncFailureKeepsCompleteBytesAndDoesNotInventMissingOutput) {
  auto io = std::make_shared<FaultIo>();
  io->remaining = 4096;
  io->sync_error = true;
  auto store =
      std::make_shared<CaptureStore>(config.home / "sync", 8192, 8192, 64, DiagnosticSink{}, io);
  OutputCapture capture(store, 512);
  capture.append(std::string(1000, 'x'));
  capture.finish();
  ProcessResult r;
  capture.metadata(r, 1000);
  EXPECT_EQ(r.spill_bytes, 1000u);
  EXPECT_FALSE(r.spill_truncated);
  EXPECT_FALSE(r.capture_error.empty());
  EXPECT_EQ(files.read(r.spill_path), std::string(1000, 'x'));
}
TEST_F(OutputReview, IncompleteUtf8PublicationDoesNotRepairACharacterSplitAcrossReads) {
  auto store = std::make_shared<CaptureStore>(config.home / "boundary");
  OutputCapture capture(store, 512);
  capture.append(std::string(511, 'a') + "\xf0\x9f");
  const auto partial = capture.render(512);
  EXPECT_EQ(partial.omitted, 0u);
  capture.flush();
  EXPECT_EQ(store->metrics.files, 0u);
  EXPECT_THAT(partial.text, Not(HasSubstr("\xef\xbf\xbd")));
  capture.append("\x8c\xb2");
  capture.finish();
  const auto complete = capture.render(512);
  EXPECT_THAT(complete.text, HasSubstr("🌲"));
  EXPECT_THAT(complete.text, Not(HasSubstr("\xef\xbf\xbd")));
  ProcessResult r;
  capture.metadata(r, 515);
  EXPECT_EQ(files.read(r.spill_path), std::string(511, 'a') + "🌲");
}
TEST_F(OutputReview, StoreWideFilesystemFailureCannotReplaceCommandStatus) {
  const auto directory = config.home / "not-a-directory";
  files.atomic_write(directory, "existing file");
  auto spec = command("cat source; exit 7");
  spec.capture_store = std::make_shared<CaptureStore>(directory);
  files.atomic_write(config.cwd / "source", std::string(2000, 'x') + "TAIL");
  const auto r = processes.run(spec, cancel);
  EXPECT_EQ(r.exit_code, 7);
  EXPECT_THAT(r.output, HasSubstr("TAIL"));
  EXPECT_FALSE(r.capture_error.empty());
  EXPECT_TRUE(r.spill_path.empty());
  EXPECT_EQ(files.read(directory), "existing file");
}
} // namespace
