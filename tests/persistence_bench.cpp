#include "codet/core.hpp"
#include "temp.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace codet;
int main(int argc, char **argv) {
  const size_t payload = argc > 1 ? static_cast<size_t>(std::stoul(argv[1])) : 512;
  TempDirectory temporary;
  StateStore state(temporary.root / "state");
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
    state.save(session);
    save_seconds +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - saving).count();
    written += transcript;
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
                         {"cumulative_transcript_bytes", written},
                         {"seconds", elapsed},
                         {"save_seconds", save_seconds},
                         {"peak_rss_bytes", rss_bytes}})
                       .dump()
                << std::endl;
    }
  }
}
