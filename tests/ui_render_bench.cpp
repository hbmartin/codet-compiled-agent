// Identical host harness for the original and cached UI boundaries.
#include "codet/core.hpp"
#include "codet/detail/curses_text.hpp"
#if __has_include("codet/detail/curses_input.hpp")
#include "codet/detail/curses_input.hpp"
#include "codet/detail/ui_history.hpp"
#define CODET_CACHED_UI 1
#endif
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <sstream>
namespace {
bool observing = false;
size_t allocations = 0, allocated = 0;
void *allocate(size_t size) {
  if (observing) {
    ++allocations;
    allocated += size;
  }
  if (auto *memory = std::malloc(size ? size : 1))
    return memory;
  throw std::bad_alloc();
}
} // namespace
void *operator new(size_t n) {
  return allocate(n);
}
void *operator new[](size_t n) {
  return allocate(n);
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
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  (void)std::setlocale(LC_ALL, "C");
  const std::string mode = argv[1];
  const size_t size = mode.find("65536") != std::string::npos  ? 65536
                      : mode.find("8192") != std::string::npos ? 8192
                                                               : 1024;
  const bool redraw = mode.find("redraw") != std::string::npos;
  const std::string huge(mode == "history-large" ? 2 * 1024 * 1024 : 128, 'a');
  size_t work = 0, checksum = 0, retained = 0;
#ifdef CODET_CACHED_UI
  codet::detail::CursesInput input;
  codet::detail::UiHistory history;
#else
  std::string input;
  std::vector<std::string> history;
#endif
  if (redraw)
    for (size_t i = 0; i != size; ++i)
      input += 'a';
  observing = true;
  const auto start = std::chrono::steady_clock::now();
  if (mode == "allocation-positive") {
    auto *memory = ::operator new(4096);
    ::operator delete(memory);
  } else if (mode.rfind("input-", 0) == 0) {
    const auto iterations = redraw ? size_t{256} : size;
    for (size_t i = 0; i != iterations; ++i) {
      if (!redraw)
        input += 'a';
#ifdef CODET_CACHED_UI
      checksum += input.view("codet> ", 79).size();
#else
      checksum += codet::detail::CursesText::input("codet> ", input, 79).size();
      work += input.size();
#endif
    }
#ifdef CODET_CACHED_UI
    work = redraw ? 0 : input.rebuilt_bytes();
#endif
  } else if (mode.rfind("history-", 0) == 0) {
    for (size_t i = 0; i != 30; ++i) {
#ifdef CODET_CACHED_UI
      history.add(huge, codet::detail::UiHistory::approval_limit);
#else
      std::vector<std::string> lines;
      std::istringstream stream(huge);
      std::string line;
      while (std::getline(stream, line))
        lines.push_back(line);
      for (const auto &row : lines)
        history.push_back(row);
      if (history.size() > 1000)
        history.erase(history.begin(), history.begin() + static_cast<long>(history.size() - 1000));
#endif
    }
#ifdef CODET_CACHED_UI
    retained = history.bytes();
#else
    for (const auto &row : history)
      retained += row.size() + 1;
#endif
  } else
    return 2;
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  observing = false;
  const codet::Json result = {{"milliseconds", elapsed},
                              {"allocations", allocations},
                              {"allocated_bytes", allocated},
                              {"prepared_input_bytes", work},
                              {"retained_payload_bytes", retained},
                              {"checksum", checksum}};
  std::ofstream report(argv[2]);
  report << result.dump() << '\n';
  return report ? 0 : 1;
}
