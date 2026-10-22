// Host-only measurement of the serialization component of saves and streamed events.
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/prompt.hpp"
#include "journal_fixture.hpp"
#include "journal_metrics.hpp"
#include "temp.hpp"
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <new>
namespace {
std::atomic<size_t> allocations{0}, allocated_bytes{0};
void *allocate(size_t size) {
  if (void *memory = std::malloc(size ? size : 1)) {
    ++allocations;
    allocated_bytes += size;
    return memory;
  }
  throw std::bad_alloc();
}
} // namespace
void *operator new(size_t size) {
  return allocate(size);
}
void *operator new[](size_t size) {
  return allocate(size);
}
void operator delete(void *p) noexcept {
  std::free(p);
}
void operator delete[](void *p) noexcept {
  std::free(p);
}
void operator delete(void *p, size_t) noexcept {
  std::free(p);
}
void operator delete[](void *p, size_t) noexcept {
  std::free(p);
}
extern "C" int sqlite3_step(sqlite3_stmt *statement) {
  using Step = int (*)(sqlite3_stmt *);
  static const auto step = reinterpret_cast<Step>(dlsym(RTLD_NEXT, "sqlite3_step"));
  codet::test::journal_metrics.before(statement);
  const auto status = step(statement);
  codet::test::journal_metrics.after(statement, status);
  return status;
}
namespace {
template <class Work> void measure_journal(codet::Json description, Work work) {
  codet::test::journal_metrics = {true};
  allocations = allocated_bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 4; ++i)
    work(i);
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  const auto count = allocations.load(), bytes = allocated_bytes.load();
  const auto sql = codet::test::journal_metrics;
  codet::test::journal_metrics = {};
  description.update({{"iterations", 4},
                      {"milliseconds", elapsed},
                      {"allocations", count},
                      {"allocated_bytes", bytes},
                      {"candidate_queries", sql.queries},
                      {"candidate_rows", sql.rows},
                      {"candidate_vm_steps", sql.vm_steps},
                      {"candidate_fullscan_steps", sql.fullscan_steps},
                      {"candidate_max_bindings", sql.max_bindings}});
  std::cout << description.dump() << '\n';
}
void journal_bench() {
  using namespace codet;
  for (const size_t bytes : {65536u, 1048576u})
    for (const size_t calls : {1u, 8u}) {
      TempDirectory temp;
      StateStore store(temp.root / "state");
      auto session = store.create();
      sqlite3 *raw = nullptr;
      if (sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &raw) != SQLITE_OK)
        throw Error("Cannot open benchmark journal");
      std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
      test::journal_history(store, session, raw, calls, "complete", bytes);
      measure_journal({{"case", "journal_load"}, {"argument_bytes", bytes}, {"calls", calls}},
                      [&](int) {
                        const auto loaded = store.load_id(session.id);
                        if (loaded.messages.size() != calls * 2)
                          throw Error("Benchmark load changed completed history");
                      });
    }
  for (const size_t bytes : {65536u, 1048576u})
    for (bool fallback : {false, true}) {
      TempDirectory temp;
      StateStore store(temp.root / "state");
      auto session = store.create();
      sqlite3 *raw = nullptr;
      if (sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &raw) != SQLITE_OK)
        throw Error("Cannot open benchmark journal");
      std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
      test::journal_history(store, session, raw, 1, "provisional", bytes, fallback);
      if (fallback)
        session.messages[0].calls[0].arguments["invalid"] = std::string(1, static_cast<char>(0xff));
      measure_journal({{"case", "journal_revision"},
                       {"argument_bytes", bytes},
                       {"sanitization_fallback", fallback}},
                      [&](int i) {
                        session.messages[1].text = "revision-" + std::to_string(i);
                        store.revise_messages(session, {1});
                      });
    }
  for (const size_t calls : {64u, 512u, 4096u})
    for (const auto *mode : {"none", "complete", "mixed"}) {
      TempDirectory temp;
      StateStore store(temp.root / "state");
      auto session = store.create();
      sqlite3 *raw = nullptr;
      if (sqlite3_open((temp.root / "state/state.sqlite3").c_str(), &raw) != SQLITE_OK)
        throw Error("Cannot open benchmark journal");
      std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
      test::journal_history(store, session, raw, calls, mode);
      measure_journal({{"case", "journal_replace"}, {"calls", calls}, {"journal", mode}},
                      [&](int) { store.save(session); });
      measure_journal({{"case", "journal_single_revision"}, {"calls", calls}, {"journal", mode}},
                      [&](int i) {
                        session.messages.back().text = "revision-" + std::to_string(i);
                        store.revise_messages(session, {session.messages.size() - 1});
                      });
      session.messages.reserve(session.messages.size() + 8);
      measure_journal({{"case", "journal_single_append"}, {"calls", calls}, {"journal", mode}},
                      [&](int i) {
                        const auto first = session.messages.size();
                        const auto id = "append-" + std::to_string(i);
                        Message assistant;
                        assistant.role = "assistant";
                        assistant.calls = {{id, "exec_command", Json::object()}};
                        Message tool;
                        tool.role = "tool";
                        tool.tool_call_id = id;
                        tool.text = "appended-" + std::to_string(i);
                        session.messages.push_back(std::move(assistant));
                        session.messages.push_back(std::move(tool));
                        store.append_messages(session, first);
                      });
    }
}
void review_bench() {
  using namespace codet;
  for (const size_t bytes : {size_t{65536}, size_t{1048576}}) {
    TempDirectory temp;
    StateStore store(temp.root / "state");
    auto session = store.create();
    Message assistant;
    assistant.role = "assistant";
    assistant.calls = {
        {"large", "exec_command", {{"payload", std::string(bytes, 'x')}, {"iteration", 0}}}};
    session.messages = {assistant};
    store.save(session);
    allocations = allocated_bytes = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 8; ++i) {
      session.messages[0].calls[0].arguments["iteration"] = i;
      store.revise_messages(session, {0});
    }
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto count = allocations.load(), allocated = allocated_bytes.load();
    std::cout << Json({{"case", "large_arguments"},
                       {"argument_bytes", bytes},
                       {"iterations", 8},
                       {"milliseconds", ms},
                       {"allocations", count},
                       {"allocated_bytes", allocated}})
                     .dump()
              << '\n';
  }
  for (const size_t messages : {size_t{128}, size_t{1024}, size_t{8192}})
    for (const auto *position : {"early", "middle", "late"}) {
      TempDirectory temp;
      StateStore store(temp.root / "state");
      auto original = store.create();
      original.messages.resize(messages, Message{"user", "ordinary prefix"});
      const auto index = std::string(position) == "early"    ? size_t{0}
                         : std::string(position) == "middle" ? messages / 2
                                                             : messages - 1;
      original.messages[index].role = "assistant";
      original.messages[index].calls = {{"historical", "exec_command", Json::object()}};
      store.save(original);
      auto session = store.load_id(original.id);
      const Session &view = session;
      store.revise_messages(view, {});
      const auto value = Json::parse(session.messages[index + 1].text);
      const auto pretty2 = value.dump(2), pretty4 = value.dump(4);
      const auto reads = store.transcript_rows_read();
      allocations = allocated_bytes = 0;
      const auto start = std::chrono::steady_clock::now();
      for (int i = 0; i < 4; ++i) {
        session.messages[index + 1].text = i % 2 ? pretty2 : pretty4;
        store.revise_messages(view, {});
      }
      const auto ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
              .count();
      const auto count = allocations.load(), allocated = allocated_bytes.load();
      std::cout << Json({{"case", "receipt_mismatch"},
                         {"messages", messages},
                         {"position", position},
                         {"iterations", 4},
                         {"milliseconds", ms},
                         {"allocations", count},
                         {"allocated_bytes", allocated},
                         {"transcript_rows_read", store.transcript_rows_read() - reads}})
                       .dump()
                << '\n';
    }
}
} // namespace
int main(int argc, char **argv) {
  using namespace codet;
  if (argc > 1 && std::string(argv[1]) == "journal") {
    journal_bench();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "review") {
    review_bench();
    return 0;
  }
  Json transcript = Json::array();
  for (int i = 0; i < 100; ++i)
    transcript.push_back(
        {{"role", "assistant"}, {"text", std::string(8192, 'x') + "雪"}, {"calls", Json::array()}});
  const Json event = {{"type", "item.updated"},
                      {"item", {{"text_delta", std::string(64, 'x') + "雪"}}}};
  const auto old = [](const Json &value) {
    return Json::parse(value.dump(-1, ' ', false, Json::error_handler_t::replace));
  };
  for (const auto &value : {transcript, event}) {
    if (safe_json(value).dump() != old(value).dump())
      return 1;
    for (bool previous : {true, false}) {
      const int iterations = value.is_array() ? 20 : 20000;
      allocations = 0;
      allocated_bytes = 0;
      const auto began = std::chrono::steady_clock::now();
      size_t bytes = 0;
      for (int i = 0; i < iterations; ++i)
        bytes += (previous ? old(value) : safe_json(value)).dump().size();
      const auto elapsed =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      const auto count = allocations.load(), total = allocated_bytes.load();
      std::cout << Json({{"case",
                          value.is_array() ? "transcript_save_serialization" : "streamed_event"},
                         {"implementation", previous ? "previous_roundtrip" : "utf8_tree"},
                         {"iterations", iterations},
                         {"milliseconds", elapsed},
                         {"allocations", count},
                         {"allocated_bytes", total},
                         {"serialized_bytes", bytes}})
                       .dump()
                << '\n';
    }
  }
  TempDirectory temporary;
  auto store = std::make_shared<CaptureStore>(temporary.root / "output");
  const std::string chunk(8192, 'x');
  allocations = 0;
  allocated_bytes = 0;
  const auto began = std::chrono::steady_clock::now();
  size_t retained_capacity = 0;
  {
    OutputCapture capture(store, 65536);
    for (int i = 0; i < 128; ++i)
      capture.append(chunk);
    capture.finish();
    retained_capacity = capture.memory_bytes();
    if (capture.render(65536).omitted == 0)
      return 1;
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto count = allocations.load(), bytes = allocated_bytes.load();
  std::cout << Json({{"case", "lazy_capture_1MiB_8KiB_chunks"},
                     {"milliseconds", elapsed},
                     {"allocations", count},
                     {"allocated_bytes", bytes},
                     {"retained_capacity", retained_capacity},
                     {"flushes", store->metrics.batches},
                     {"scans", store->metrics.scans},
                     {"syncs", store->metrics.syncs},
                     {"renders", store->metrics.renders}})
                   .dump()
            << '\n';
  Session session;
  for (int i = 0; i < 100; ++i) {
    Message m;
    m.role = "assistant";
    m.text = std::string(8192, 'x') + "雪";
    session.messages.push_back(std::move(m));
  }
  Message context;
  context.role = "system";
  context.text = "new context";
  Message user;
  user.role = "user";
  user.text = "new prompt";
  for (bool previous : {true, false}) {
    allocations = 0;
    allocated_bytes = 0;
    const auto start = std::chrono::steady_clock::now();
    size_t prepared = 0;
    for (int i = 0; i < 2000; ++i) {
      if (previous) {
        Session candidate = session;
        candidate.messages.push_back(context);
        candidate.messages.push_back(user);
        prepared += candidate.messages.size();
      } else {
        PromptAppend pending(session, {context, user});
        prepared += session.messages.size();
      }
    }
    const auto time =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto count = allocations.load(), bytes = allocated_bytes.load();
    std::cout << Json({{"case", "prompt_preparation"},
                       {"implementation", previous ? "full_copy" : "rollback_append"},
                       {"iterations", 2000},
                       {"milliseconds", time},
                       {"allocations", count},
                       {"allocated_bytes", bytes},
                       {"prepared_messages", prepared}})
                     .dump()
              << '\n';
  }
  struct Counting : FileSystem {
    NativeFileSystem native;
    mutable size_t canonical_calls = 0;
    std::string read(const fs::path &p, size_t n) const override {
      return native.read(p, n);
    }
    void atomic_write(const fs::path &p, const std::string &t, bool b) override {
      native.atomic_write(p, t, b);
    }
    void remove(const fs::path &p) override {
      native.remove(p);
    }
    fs::path canonical(const fs::path &p) const override {
      ++canonical_calls;
      return native.canonical(p);
    }
  } files;
  Config cfg;
  cfg.cwd = files.native.canonical(temporary.root / "glob");
  cfg.home = temporary.root / "home";
  cfg.roots = {cfg.cwd};
  fs::create_directories(cfg.cwd);
  for (int i = 0; i < 1000; ++i)
    files.native.atomic_write(cfg.cwd / ("file-" + std::to_string(i)), "x");
  cfg.set_deny({"Read(secret*)"});
  NativeProcesses procs;
  CurlTransport http;
  SystemClock clock;
  Cancellation cancel;
  Tools tools(cfg, files, procs, http, clock);
  allocations = 0;
  allocated_bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  const auto result = tools.execute({"glob", "Glob", {{"pattern", "does-not-match"}}}, cancel);
  const auto time =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  const auto n = allocations.load(), b = allocated_bytes.load();
  if (result.error || !result.content.empty())
    return 1;
  std::cout << Json({{"case", "glob_1000_nonmatches"},
                     {"milliseconds", time},
                     {"allocations", n},
                     {"allocated_bytes", b},
                     {"canonical_calls", files.canonical_calls}})
                   .dump()
            << '\n';
}
