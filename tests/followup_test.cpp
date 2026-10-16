#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/process_control.hpp"
#include "temp.hpp"
#include <cerrno>
#include <csignal>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace codet;
using namespace testing;
namespace {
struct Followup : Test {
  TempDirectory temp;
  NativeFileSystem files;
  NativeProcesses processes;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  Config config;
  void SetUp() override {
    config.home = temp.root / "state";
    config.cwd = temp.root / "workspace";
    fs::create_directories(config.home);
    fs::create_directories(config.cwd);
    config.roots = {config.cwd};
    config.permission_mode = "bypass";
    config.tool_output_limit = 512;
  }
  ProcessSpec command(const std::string &text) {
    ProcessSpec spec;
    spec.argv = {"/bin/sh", "-c", text};
    spec.cwd = config.cwd;
    spec.spool_directory = config.home / "tool-output";
    spec.output_limit = 512;
    return spec;
  }
};
TEST_F(Followup, SelectionRespectsConfiguredPairsAndExplicitFields) {
  config.configured = {"openai", "top-level"};
  config.providers["openai"].model = "nested";
  config.providers["ollama"].model = "ollama-model";
  EXPECT_EQ(resolve_selection(config).model, "top-level");
  config.selected_provider = "ollama";
  EXPECT_EQ(resolve_selection(config).model, "ollama-model");
  config.selected_provider = "openai";
  EXPECT_EQ(resolve_selection(config, Selection{"ollama", "saved"}).model, "top-level");
  EXPECT_EQ(resolve_selection(config, Selection{"openai", "saved"}).model, "saved");
  config.selected_provider.reset();
  config.selected_model = "explicit";
  const auto selected = resolve_selection(config, Selection{"ollama", "saved"});
  EXPECT_EQ(selected.provider, "ollama");
  EXPECT_EQ(selected.model, "explicit");
}
TEST_F(Followup, EveryAdapterOmitsEmptyToolsAndKeepsNonemptyDeclarations) {
  CredentialStore credentials(config.home, files, clock);
  ApiProvider provider(http, credentials, clock);
  Tools tools(config, files, processes, http, clock);
  for (const auto &name : {"codex", "anthropic", "google", "openai", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    const auto empty = Json::parse(
        provider.request(config, "system", {}, Json::array(), {{"access_token", "fake"}}).body);
    EXPECT_FALSE(empty.contains("tools")) << name;
    const auto nonempty = Json::parse(
        provider.request(config, "system", {}, tools.schemas(), {{"access_token", "fake"}}).body);
    EXPECT_FALSE(nonempty["tools"].empty()) << name;
  }
}
struct FixtureHttp : HttpTransport {
  std::string provider;
  std::vector<Json> requests;
  HttpResponse perform(const HttpRequest &request, const Cancellation &,
                       const ChunkSink &sink) override {
    requests.push_back(Json::parse(request.body));
    const auto text = requests.size() == 1 ? "summary" : "done";
    std::vector<Json> frames;
    if (provider == "codex")
      frames.push_back(
          {{"type", "response.completed"},
           {"response",
            {{"status", "completed"},
             {"output", Json::array({{{"type", "message"},
                                      {"role", "assistant"},
                                      {"content", Json::array({{{"type", "output_text"},
                                                                {"text", text}}})}}})}}}});
    else if (provider == "anthropic") {
      frames = {{{"type", "content_block_start"},
                 {"index", 0},
                 {"content_block", {{"type", "text"}, {"text", ""}}}},
                {{"type", "content_block_delta"},
                 {"index", 0},
                 {"delta", {{"type", "text_delta"}, {"text", text}}}},
                {{"type", "message_stop"}}};
    } else if (provider == "google")
      frames.push_back(
          {{"candidates", Json::array({{{"content", {{"parts", Json::array({{{"text", text}}})}}},
                                        {"finishReason", "STOP"}}})}});
    else if (provider == "ollama")
      frames.push_back({{"message", {{"role", "assistant"}, {"content", text}}}, {"done", true}});
    else
      frames.push_back({{"choices", Json::array({{{"delta", {{"content", text}}},
                                                  {"finish_reason", "stop"}}})}});
    for (const auto &frame : frames)
      sink((provider == "ollama" ? "" : "data: ") + frame.dump() + "\n\n");
    if (provider == "openai")
      sink("data: [DONE]\n\n");
    return {200, ""};
  }
};
TEST_F(Followup, CompactionUsesRealAdaptersWithoutToolsAndRetainsNormalTools) {
  config.context_tokens = 512;
  config.model = "fixture";
  files.atomic_write(config.home / "token.json", R"({"access_token":"fake"})");
  CredentialStore credentials(config.home, files, clock);
  credentials.import(config.home / "token.json");
  for (const auto &name : {"codex", "anthropic", "google", "openai", "ollama"}) {
    config.provider = name;
    config.providers[name].api_key = "fixture";
    StateStore state(config.home);
    auto session = state.create();
    for (int i = 0; i < 3; ++i) {
      codet::Message message;
      message.role = i % 2 ? "assistant" : "user";
      message.text = std::string(17 + i, 'x');
      for (int j = 0; j < 400; ++j)
        message.text += "雪🌲e\xcc\x81";
      session.messages.push_back(message);
    }
    state.save(session);
    FixtureHttp transport;
    transport.provider = name;
    ApiProvider provider(transport, credentials, clock);
    Tools tools(config, files, processes, transport, clock);
    Agent agent(config, provider, tools, state, {});
    EXPECT_EQ(agent.run(session, "objective", cancel).text, "done") << name;
    ASSERT_EQ(transport.requests.size(), 2u) << name;
    EXPECT_FALSE(transport.requests[0].contains("tools")) << name;
    EXPECT_FALSE(transport.requests[1]["tools"].empty()) << name;
  }
}
TEST_F(Followup, ShellSpoolPreservesOutputAndSupportsReadPaging) {
  std::string text = "HEAD\n";
  for (int i = 0; i < 300; ++i)
    text += "line-" + std::to_string(i) + "\n";
  text += "TAIL\n";
  files.atomic_write(config.cwd / "source", text);
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute({"shell", "Bash", {{"command", "cat source"}}}, cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_LE(result.content.size(), config.tool_output_limit);
  EXPECT_THAT(result.content, HasSubstr("HEAD"));
  EXPECT_THAT(result.content, HasSubstr("TAIL"));
  ASSERT_TRUE(result.metadata.contains("spill_path"));
  const fs::path path = result.metadata["spill_path"].get<std::string>();
  EXPECT_EQ(files.read(path), text);
  EXPECT_EQ(result.metadata["total_output_bytes"], text.size());
  EXPECT_EQ(result.metadata["spill_bytes"], text.size());
  EXPECT_EQ(result.metadata["spill_truncated"], false);
  struct stat status {};
  ASSERT_EQ(::stat(path.c_str(), &status), 0);
  EXPECT_EQ(status.st_mode & 0777, 0600);
  config.permission_mode = "default";
  const auto page = tools.execute(
      {"page", "Read", {{"file_path", path.string()}, {"offset", 290}, {"limit", 5}}}, cancel);
  ASSERT_FALSE(page.error) << page.content;
  EXPECT_THAT(page.content, HasSubstr("line-288"));
  config.deny = {"Read(*capture-*)"};
  EXPECT_TRUE(tools.execute({"denied", "Read", {{"file_path", path.string()}}}, cancel).error);
}
TEST_F(Followup, SmallCommandsDoNotCreateCaptureFiles) {
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute({"small", "Bash", {{"command", "printf approved"}}}, cancel);
  ASSERT_FALSE(result.error);
  EXPECT_EQ(result.content, "approved");
  EXPECT_FALSE(result.metadata.contains("spill_path"));
  EXPECT_FALSE(fs::exists(config.home / "tool-output"));
  EXPECT_EQ(result.metadata["omitted_output_bytes"], 0);
}
TEST_F(Followup, InvalidByteExpansionKeepsOriginalCaptureAndQuotaMetadata) {
  files.atomic_write(config.cwd / "source", std::string(300, '\xff'));
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute({"bytes", "Bash", {{"command", "cat source"}}}, cancel);
  ASSERT_FALSE(result.error);
  EXPECT_LE(result.content.size(), config.tool_output_limit);
  const fs::path path = result.metadata["spill_path"].get<std::string>();
  EXPECT_THAT(path.filename().string(), StartsWith("capture-"));
  EXPECT_EQ(files.read(path), std::string(300, '\xff'));
  EXPECT_EQ(result.metadata["spill_bytes"], 300);
  EXPECT_EQ(result.metadata["total_output_bytes"], 300);
}
TEST_F(Followup, PortableShellSearchSkipsGeneratedAndBinaryFiles) {
  for (const auto &dir : {".git", "build", "node_modules"}) {
    fs::create_directories(config.cwd / dir);
    files.atomic_write(config.cwd / dir / "hidden", "needle\n");
  }
  files.atomic_write(config.cwd / "unicode-雪.txt", "needle\n");
  files.atomic_write(config.cwd / "binary", std::string("needle\0binary", 13));
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute(
      {"search",
       "Bash",
       {{"command",
         "find . -type d \\( -name .git -o -name build -o -name node_modules \\) -prune -o "
         "-type f -exec grep -I -n -E -e 'needle|alternative' /dev/null {} +"}}},
      cancel);
  ASSERT_FALSE(result.error) << result.content;
  EXPECT_EQ(result.content, "./unicode-雪.txt:1:needle\n");
}
TEST_F(Followup, LongLineCaptureUsesEightMiBCapAndPreservesPreviewTail) {
  const auto cap = size_t{8} * 1024 * 1024;
  const std::string original = std::string(cap + 10000, 'x') + "TAIL";
  files.atomic_write(config.cwd / "source", original);
  const auto result = processes.run(command("cat source"), cancel);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.spill_bytes, cap);
  EXPECT_TRUE(result.spill_truncated);
  EXPECT_EQ(result.total_output_bytes, original.size());
  EXPECT_EQ(files.read(result.spill_path), original.substr(0, cap));
  EXPECT_THAT(result.output, HasSubstr("TAIL"));
  const auto page = processes.run(
      command("dd if='" + result.spill_path + "' bs=1024 skip=8 count=1 2>/dev/null"), cancel);
  EXPECT_EQ(page.exit_code, 0);
  EXPECT_EQ(page.total_output_bytes, 1024u);
  // Preview bounds are independent of retained bytes.
  EXPECT_EQ(files.read(page.spill_path), std::string(1024, 'x'));
  struct stat capture {};
  ASSERT_EQ(::stat(result.spill_path.c_str(), &capture), 0);
  EXPECT_EQ(capture.st_mode & 0777, 0600);
}
TEST_F(Followup, ActiveCapturesAreProtectedAndCompletedFilesEvict) {
  const auto dir = config.home / "tool-output";
  OutputSpool first(dir, 16, 16, 1);
  first.append("1234567890");
  first.flush();
  OutputSpool second(dir, 16, 16, 1);
  second.append("abcdefghij");
  second.flush();
  EXPECT_EQ(second.bytes(), 6u);
  EXPECT_TRUE(second.truncated());
  EXPECT_TRUE(fs::exists(first.path()));
  first.close();
  OutputSpool third(dir, 16, 16, 1);
  third.append("new");
  third.flush();
  EXPECT_FALSE(fs::exists(first.path()));
  EXPECT_TRUE(fs::exists(second.path()));
  second.close();
  third.close();
  size_t records = 0, bytes = 0;
  for (const auto &entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".txt") {
      ++records;
      bytes += fs::file_size(entry.path());
    }
  EXPECT_LE(records, 1u);
  EXPECT_LE(bytes, 16u);
}
TEST_F(Followup, CaptureOwnershipCoordinatesAcrossProcesses) {
  int ready[2], release[2];
  ASSERT_EQ(::pipe(ready), 0);
  ASSERT_EQ(::pipe(release), 0);
  const auto pid = ::fork();
  ASSERT_GE(pid, 0);
  if (!pid) {
    ::close(ready[0]);
    ::close(release[1]);
    OutputSpool active(config.home / "tool-output", 8, 8, 64);
    active.append("123456");
    active.flush();
    char token = active.bytes() == 6 ? 'y' : 'n';
    (void)::write(ready[1], &token, 1);
    (void)::read(release[0], &token, 1);
    active.close();
    ::_exit(0);
  }
  ::close(ready[1]);
  ::close(release[0]);
  char token = 0;
  EXPECT_EQ(::read(ready[0], &token, 1), 1);
  EXPECT_EQ(token, 'y');
  OutputSpool other(config.home / "tool-output", 8, 8, 64);
  other.append("abcd");
  other.flush();
  EXPECT_EQ(other.bytes(), 2u);
  EXPECT_TRUE(other.truncated());
  (void)::write(release[1], &token, 1);
  int status = 0;
  EXPECT_EQ(::waitpid(pid, &status, 0), pid);
  EXPECT_EQ(status, 0);
  ::close(ready[0]);
  ::close(release[1]);
}
TEST_F(Followup, SpoolFailureKeepsExitStatusAndSerializablePreview) {
  fs::create_directory_symlink(config.cwd, config.home / "tool-output");
  files.atomic_write(config.cwd / "source", std::string(3000, '\xff') + "TAIL\n");
  Tools tools(config, files, processes, http, clock);
  const auto result = tools.execute({"shell", "Bash", {{"command", "cat source"}}}, cancel);
  EXPECT_FALSE(result.error);
  EXPECT_EQ(result.metadata["exit_code"], 0);
  EXPECT_TRUE(result.metadata.contains("capture_error"));
  EXPECT_NO_THROW(Json(result.content).dump());
  EXPECT_LE(result.content.size(), config.tool_output_limit);
}
TEST_F(Followup, BackgroundAndPtyCommandsRetainLargeOutput) {
  files.atomic_write(config.cwd / "source", std::string(2000, 'x') + "TAIL\n");
  for (bool pty : {false, true}) {
    auto spec = command("cat source");
    spec.pty = pty;
    const auto id = processes.start(spec, cancel);
    ProcessResult result;
    for (int i = 0; i < 500; ++i) {
      result = processes.poll(id);
      if (!result.running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_FALSE(result.running);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.spill_path.empty());
    EXPECT_THAT(files.read(result.spill_path), HasSubstr("TAIL"));
    EXPECT_THAT(result.output, HasSubstr("TAIL"));
  }
}
struct FaultControl : ProcessControl {
  std::shared_ptr<ProcessControl> native = native_process_control();
  bool observed = false, fail_signal = false, fail_reap = false, lose_ownership = false;
  bool lose_on_observe = false;
  bool interrupt_read = false, interrupt_release = false;
  Cancellation *cancel_read = nullptr, *cancel_release = nullptr;
  int exit_events = 0, signals = 0;
  int watch(pid_t pid) override {
    return native->watch(pid);
  }
  bool exited(pid_t pid, int fd) override {
    if (observed)
      return false; // Darwin's consumed one-shot notification.
    if (native->exited(pid, fd)) {
      observed = true;
      ++exit_events;
      if (lose_on_observe) {
        (void)native->reap(pid);
        throw ProcessOwnershipLost("injected observation ownership loss");
      }
      return true;
    }
    return false;
  }
  void signal(pid_t target, int signal) override {
    ++signals;
    if (fail_signal && observed) {
      fail_signal = false;
      throw Error("injected signal failure");
    }
    native->signal(target, signal);
  }
  int reap(pid_t pid) override {
    if (lose_ownership) {
      lose_ownership = false;
      (void)native->reap(pid);
      throw ProcessOwnershipLost("injected ownership loss");
    }
    if (fail_reap && observed) {
      fail_reap = false;
      throw Error("injected reap failure");
    }
    return native->reap(pid);
  }
  int read_setup(int fd, int &error) override {
    if (cancel_read) {
      cancel_read->requested = true;
      errno = EINTR;
      return -1;
    }
    if (interrupt_read) {
      interrupt_read = false;
      errno = EINTR;
      return -1;
    }
    return native->read_setup(fd, error);
  }
  int release(int fd) override {
    if (cancel_release) {
      cancel_release->requested = true;
      errno = EINTR;
      return -1;
    }
    if (interrupt_release) {
      interrupt_release = false;
      errno = EINTR;
      return -1;
    }
    return native->release(fd);
  }
};
TEST_F(Followup, OneShotExitSurvivesSignalAndReapFailures) {
  for (bool reap : {false, true}) {
    auto control = std::make_shared<FaultControl>();
    control->fail_reap = reap;
    control->fail_signal = !reap;
    NativeProcesses native(control);
    const auto id = native.start(command("true"), cancel);
    bool failure = false;
    for (int i = 0; i < 500 && !failure; ++i) {
      try {
        (void)native.poll(id);
      } catch (const Error &) {
        failure = true;
      }
      if (!failure)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(failure);
    const auto result = native.poll(id);
    EXPECT_FALSE(result.running);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(control->exit_events, 1);
    const auto signals = control->signals;
    native.stop(id);
    native.stop(id);
    EXPECT_EQ(control->signals, signals);
  }
}
TEST_F(Followup, OwnershipLossInvalidatesFurtherSignals) {
  auto control = std::make_shared<FaultControl>();
  control->lose_ownership = true;
  NativeProcesses native(control);
  const auto id = native.start(command("true"), cancel);
  ProcessResult result;
  for (int i = 0; i < 500; ++i) {
    result = native.poll(id);
    if (!result.running)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_FALSE(result.running);
  EXPECT_EQ(result.exit_code, 1);
  EXPECT_THAT(result.capture_error, HasSubstr("ownership loss"));
  const auto signals = control->signals;
  native.stop(id);
  native.stop(id);
  EXPECT_EQ(control->signals, signals);
}
TEST_F(Followup, ObservationOwnershipLossNeverSignals) {
  auto control = std::make_shared<FaultControl>();
  control->lose_on_observe = true;
  NativeProcesses native(control);
  const auto result = native.run(command("true"), cancel);
  EXPECT_EQ(result.exit_code, 1);
  EXPECT_EQ(control->signals, 0);
  EXPECT_THAT(result.capture_error, HasSubstr("ownership loss"));
}
TEST_F(Followup, HandshakeRetriesEintrAndCancellationWinsOverSetupErrors) {
  auto control = std::make_shared<FaultControl>();
  control->interrupt_read = control->interrupt_release = true;
  NativeProcesses native(control);
  EXPECT_EQ(native.run(command("true"), cancel).exit_code, 0);
  for (bool release : {false, true}) {
    auto fault = std::make_shared<FaultControl>();
    if (release)
      fault->cancel_release = &cancel;
    else
      fault->cancel_read = &cancel;
    NativeProcesses process(fault);
    try {
      (void)process.run(command("printf side-effect > never"), cancel);
      ADD_FAILURE();
    } catch (const Error &e) {
      EXPECT_EQ(e.code, 130);
    }
    EXPECT_FALSE(fs::exists(config.cwd / "never"));
    cancel.requested = false;
  }
}
TEST_F(Followup, CommandsResetInheritedHangupAndSignalMask) {
  struct sigaction ignored {
  }, previous{};
  ignored.sa_handler = SIG_IGN;
  sigemptyset(&ignored.sa_mask);
  ASSERT_EQ(::sigaction(SIGHUP, &ignored, &previous), 0);
  struct RestoreDisposition {
    struct sigaction previous;
    ~RestoreDisposition() {
      (void)::sigaction(SIGHUP, &previous, nullptr);
    }
  } restore_disposition{previous};
  const auto hangup = processes.run(command("kill -HUP $$; printf survived"), cancel);
  EXPECT_EQ(hangup.exit_code, 128 + SIGHUP);
  EXPECT_THAT(hangup.output, Not(HasSubstr("survived")));
  sigset_t mask, old;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  ASSERT_EQ(::sigprocmask(SIG_BLOCK, &mask, &old), 0);
  struct RestoreMask {
    sigset_t previous;
    ~RestoreMask() {
      (void)::sigprocmask(SIG_SETMASK, &previous, nullptr);
    }
  } restore_mask{old};
  const auto signal = processes.run(command("kill -USR1 $$; printf survived"), cancel);
  EXPECT_EQ(signal.exit_code, 128 + SIGUSR1);
}
} // namespace
