#include "codet/context.hpp"
#include "codet/core.hpp"
#include "codet/output.hpp"
#include "temp.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
using namespace codet;
namespace {
uint64_t peak_rss() {
  struct rusage usage {};
  if (getrusage(RUSAGE_SELF, &usage))
    throw Error("Cannot measure benchmark memory");
  const auto rss = static_cast<uint64_t>(usage.ru_maxrss);
#ifdef __APPLE__
  return rss;
#else
  return rss * 1024;
#endif
}
struct MeasuredFiles : FileSystem {
  NativeFileSystem native;
  mutable size_t canonicalizations = 0;
  size_t archives = 0;
  std::string read(const fs::path &p, size_t n) const override {
    return native.read(p, n);
  }
  void atomic_write(const fs::path &p, const std::string &s, bool secret) override {
    if (p.parent_path().parent_path().filename() == "results")
      ++archives;
    native.atomic_write(p, s, secret);
  }
  void remove(const fs::path &p) override {
    native.remove(p);
  }
  fs::path canonical(const fs::path &p) const override {
    ++canonicalizations;
    return native.canonical(p);
  }
};
struct Offline : HttpTransport {
  HttpResponse perform(const HttpRequest &, const Cancellation &, const ChunkSink &) override {
    throw Error("Unexpected benchmark network request");
  }
};
void inspection_bench() {
  TempDirectory temp;
  Config config;
  config.home = temp.root / "home";
  config.cwd = temp.root / "work";
  config.roots = {config.cwd};
  config.permission_mode = "plan";
  config.approval = "never";
  fs::create_directories(config.home);
  fs::create_directories(config.cwd);
  MeasuredFiles files;
  NativeProcesses processes;
  Offline http;
  SystemClock clock;
  Cancellation cancel;
  Tools tools(config, files, processes, http, clock);
  tools.use_session("benchmark");
  for (size_t entries : {1000u, 5000u, 10000u}) {
    const auto dir = config.cwd / std::to_string(entries);
    fs::create_directories(dir);
    for (size_t i = 0; i < entries; ++i)
      std::ofstream(dir / std::to_string(10000 + i)).close();
    const auto start = std::chrono::steady_clock::now();
    const auto before = files.canonicalizations;
    for (size_t page = 0; page < 10; ++page) {
      const auto result =
          tools.execute({unique_id(),
                         "read_file",
                         {{"path", dir.string()}, {"offset", 1 + page * 16}, {"limit", 16}}},
                        cancel);
      if (result.error)
        throw Error(result.content);
    }
    std::cout
        << Json({{"peak_rss_bytes", peak_rss()},
                 {"directory_entries", entries},
                 {"pages", 10},
                 {"canonicalizations", files.canonicalizations - before},
                 {"seconds",
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()}})
               .dump()
        << std::endl;
  }
  Message message;
  message.role = "assistant";
  message.message_id = unique_id();
  message.text = std::string(65536, 'x');
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 50; ++i) {
    auto retry = message;
    tools.retain_result(retry);
  }
  std::cout
      << Json({{"peak_rss_bytes", peak_rss()},
               {"archive_attempts", 50},
               {"archive_publications", files.archives},
               {"seconds",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()}})
             .dump()
      << std::endl;
}
void history_bench() {
  TempDirectory temp;
  Config config;
  config.cwd = temp.root / "work";
  config.home = temp.root / "home";
  config.provider = "codex";
  fs::create_directories(config.cwd);
  fs::create_directories(config.home);
  NativeFileSystem files;
  NativeProcesses processes;
  Offline http;
  SystemClock clock;
  Tools tools(config, files, processes, http, clock);
  tools.use_session("history-benchmark");
  for (size_t count : {100u, 300u, 1000u}) {
    std::vector<Message> messages;
    for (size_t i = 0; i < count; ++i) {
      const auto id = std::to_string(i);
      Message call, result;
      call.role = "assistant";
      call.calls = {{id, i % 2 ? "functions.read_file" : "apply_patch", {{"patch", "fixture"}}}};
      if (!(i % 2))
        call.replay = Json::array({{{"type", "custom_tool_call"},
                                    {"call_id", id},
                                    {"name", "apply_patch"},
                                    {"input", "fixture"}}});
      result.role = "tool";
      result.tool_call_id = id;
      result.text = Json({{"content", std::string(4096, 'x')},
                          {"is_error", false},
                          {"metadata", Json::object()}})
                        .dump();
      messages.push_back(std::move(call));
      messages.push_back(std::move(result));
    }
    const auto start = std::chrono::steady_clock::now();
    const auto size = provider_history_size(config, messages);
    const auto sized = std::chrono::steady_clock::now();
    if (size.encoded_parts > messages.size() * 2 + 1)
      throw Error("Wire sizing revisited existing message parts");
    if (size.aggregate_items)
      throw Error("Sizing retained aggregate wire history");
    const auto projected = project_history(messages, size.total / 2, tools);
    std::cout
        << Json({{"tool_results", count},
                 {"messages", messages.size()},
                 {"wire_parts_encoded", size.encoded_parts},
                 {"aggregate_wire_items", size.aggregate_items},
                 {"wire_bytes", size.total},
                 {"projected_bytes", tools.history_bytes(projected)},
                 {"sizing_seconds", std::chrono::duration<double>(sized - start).count()},
                 {"sizing_and_projection_seconds",
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()},
                 {"peak_rss_bytes", peak_rss()}})
               .dump()
        << std::endl;
  }
}
void provider_bench() {
  TempDirectory temp;
  StateStore state(temp.root / "state"), other(temp.root / "state");
  auto unrelated = other.create();
  for (size_t payload : {1024u * 1024, 9u * 1024 * 1024}) {
    auto session = state.create();
    session.provider_state = {{"provider", "anthropic"}, {"messages", Json::array()}};
    auto &messages = session.provider_state["messages"];
    messages.push_back(
        {{"source", "large"},
         {"wire",
          Json::array({{{"role", "user"},
                        {"content", Json::array({{{"type", "text"},
                                                  {"text", std::string(payload, 'i')}}})}}})}});
    state.save_provider_state(session);
    const auto before = state.serialized_bytes();
    const auto reads = state.provider_rows_read();
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) {
      const ProviderStateUpdate update{state.provider_revision(session.id), messages.size()};
      messages.push_back(
          {{"source", std::to_string(i)},
           {"wire",
            Json::array(
                {{{"role", "user"},
                  {"content", Json::array({{{"type", "text"}, {"text", "small update"}}})}}})}});
      other.finish_turn(unrelated.id, "complete");
      state.save_provider_state(session, update);
    }
    if (state.provider_rows_read() != reads)
      throw Error("Incremental provider save read unchanged rows");
    const auto written = state.serialized_bytes() - before;
    if (written >= 20000 || state.provider_cache_bytes() > size_t{8} * 1024 * 1024)
      throw Error("Provider appends exceeded changed-part/cache bounds");
    std::cout
        << Json({{"provider_payload_bytes", payload},
                 {"appends", 20},
                 {"unrelated_session_commits", 20},
                 {"append_serialized_bytes", written},
                 {"cached_payload_bytes", state.provider_cache_bytes()},
                 {"unchanged_rows_read", state.provider_rows_read() - reads},
                 {"peak_rss_bytes", peak_rss()},
                 {"seconds",
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()}})
               .dump()
        << std::endl;
  }
}
} // namespace
int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "provider") {
    provider_bench();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "history") {
    history_bench();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "inspection") {
    inspection_bench();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "captures") {
    TempDirectory temp;
    for (int retained : {40, 64}) {
      auto store = std::make_shared<CaptureStore>(temp.root / std::to_string(retained));
      for (int i = 0; i < retained; ++i) {
        OutputSpool old(store);
        old.append(std::string(4096, 'r'));
        old.close();
      }
      OutputSpool active(store);
      active.append(std::string(65536, 'x'));
      active.flush();
      const auto scans = store->metrics.scans;
      const auto start = std::chrono::steady_clock::now();
      for (int i = 0; i < 100; ++i) {
        active.append(std::string(65536, 'x'));
        active.flush();
      }
      if (store->metrics.scans != scans)
        throw Error("Ordinary flush scanned the whole store");
      active.close();
      if (!active.error().empty() || store->metrics.scans != scans)
        throw Error("Ordinary close failed or scanned the whole store");
      const auto flush_scans = store->metrics.scans - scans;
      size_t close_scans = 0;
      for (int i = 0; i < 20; ++i) {
        const auto before = store->metrics.scans;
        OutputSpool next(store);
        next.append("next");
        next.flush();
        next.close();
        close_scans += store->metrics.scans - before;
        if (!next.error().empty())
          throw Error(next.error());
      }
      if (close_scans)
        throw Error("At-capacity creation/close scanned the whole store");
      struct rusage io {};
      getrusage(RUSAGE_SELF, &io);
      std::cout
          << Json(
                 {{"flushes", 100},
                  {"capture_bytes", active.bytes()},
                  {"capture_syncs", store->metrics.syncs},
                  {"capture_batches", store->metrics.batches},
                  {"retained_files", retained},
                  {"repeated_closes", 20},
                  {"ordinary_close_scans", close_scans},
                  {"ordinary_flush_scans", flush_scans},
                  {"seconds",
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()},
                  {"peak_rss_bytes", peak_rss()},
                  {"input_blocks", io.ru_inblock},
                  {"output_blocks", io.ru_oublock}})
                 .dump()
          << std::endl;
    }
    return 0;
  }
  const size_t payload = argc > 1 ? static_cast<size_t>(std::stoul(argv[1])) : 512;
  TempDirectory temporary;
  const std::string mode = argc > 2 ? argv[2] : "durable";
  StateStore state(temporary.root / "state", {}, mode);
  auto session = state.create();
  uint64_t written = 0, transcript = 2;
  double save_seconds = 0;
  const auto start = std::chrono::steady_clock::now();
  for (size_t i = 1; i <= 300; ++i) {
    Message result;
    result.role = "tool";
    result.message_id = unique_id();
    result.tool_call_id = std::to_string(i);
    result.previewed = true;
    result.text = Json({{"content", std::string(payload, 'x')},
                        {"is_error", false},
                        {"metadata", Json::object()}})
                      .dump();
    transcript += Json(result).dump().size() + (i > 1 ? 1 : 0);
    session.messages.push_back(std::move(result));
    const auto saving = std::chrono::steady_clock::now();
    state.append_messages(session, session.messages.size() - 1);
    save_seconds +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - saving).count();
    written = state.serialized_bytes();
    if (written > transcript * 2)
      throw Error("Append serialized unchanged history");
    if (i % 100 == 0) {
      struct rusage usage {};
      if (getrusage(RUSAGE_SELF, &usage))
        return 1;
      const auto elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      const auto rss = static_cast<uint64_t>(usage.ru_maxrss);
#ifdef __APPLE__
      const auto rss_bytes = rss;
#else
      const auto rss_bytes = rss * 1024;
#endif
      std::cout << Json({{"results", i},
                         {"payload_bytes", payload},
                         {"transcript_bytes", transcript},
                         {"serialized_bytes", written},
                         {"database_bytes", fs::file_size(temporary.root / "state/state.sqlite3")},
                         {"wal_bytes", fs::file_size(temporary.root / "state/state.sqlite3-wal")},
                         {"durability", mode},
                         {"input_blocks", usage.ru_inblock},
                         {"output_blocks", usage.ru_oublock},
                         {"seconds", elapsed},
                         {"save_seconds", save_seconds},
                         {"peak_rss_bytes", rss_bytes}})
                       .dump()
                << std::endl;
    }
  }
}
