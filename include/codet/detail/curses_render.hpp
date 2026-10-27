#pragma once
// Native-only: do not expose curses macros through the shared text helpers.
#include "codet/detail/curses_text.hpp"
#include <curses.h>
#include <new>

namespace codet::detail {
inline bool curses_unicode_supported() {
  constexpr std::string_view sample = "\xc2\xa3";
  if (CursesText(sample).text() != sample)
    return false;
  auto *window = ::newpad(2, 16); // Unrefreshed cells, independent of terminal size.
  if (!window)
    throw std::bad_alloc();
  const auto result = ::waddnstr(window, sample.data(), static_cast<int>(sample.size()));
  int row = 0, column = 0;
  getyx(window, row, column);
  (void)::delwin(window);
  return result != ERR && row == 0 && column == 1;
}
} // namespace codet::detail
