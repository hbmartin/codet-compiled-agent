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
} // namespace
int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "inspection") {
    inspection_bench();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "captures") {
    TempDirectory temp;
    auto store = std::make_shared<CaptureStore>(temp.root / "captures");
    for (int i = 0; i < 40; ++i) {
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
    struct rusage io {};
    getrusage(RUSAGE_SELF, &io);
    std::cout
        << Json({{"flushes", 100},
                 {"capture_bytes", active.bytes()},
                 {"capture_syncs", store->metrics.syncs},
                 {"capture_batches", store->metrics.batches},
                 {"retained_files", 40},
                 {"ordinary_flush_scans", store->metrics.scans - scans},
                 {"seconds",
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()},
                 {"peak_rss_bytes", peak_rss()},
                 {"input_blocks", io.ru_inblock},
                 {"output_blocks", io.ru_oublock}})
               .dump()
        << std::endl;
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
