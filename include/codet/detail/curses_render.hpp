#pragma once
// Native-only: do not expose curses macros through the shared text helpers.
#include "codet/core.hpp"
#include "codet/detail/curses_text.hpp"
#include <curses.h>

namespace codet::detail {
inline bool curses_unicode_supported(WINDOW *(*allocate)(int, int) = &::newpad) {
  constexpr std::string_view sample = "\xc2\xa3";
  if (CursesText(sample).text() != sample)
    return false;
  auto *window = allocate(2, 16); // Unrefreshed cells, independent of terminal size.
  if (!window)
    return false;
  const auto result = ::waddnstr(window, sample.data(), static_cast<int>(sample.size()));
  int row = 0, column = 0;
  getyx(window, row, column);
  (void)::delwin(window);
  return result != ERR && row == 0 && column == 1;
}
// Takes already prepared text. Every row has one clipping/clearing/drawing boundary.
inline void curses_draw_row(int row, std::string_view text, int columns, bool exact = false) {
  if (columns <= 1)
    return;
  const auto size = CursesText::prefix(text, static_cast<size_t>(columns - 1),
                                       static_cast<size_t>(std::max(TABSIZE, 1)));
  if (exact && size != text.size())
    throw Error("Approval review exceeded its assigned row");
  text = text.substr(0, size);
  if (move(row, 0) == ERR || clrtoeol() == ERR ||
      (!text.empty() && mvaddnstr(row, 0, text.data(), static_cast<int>(text.size())) == ERR))
    throw Error("Cannot render terminal row");
  int actual_row = 0, actual_column = 0;
  getyx(stdscr, actual_row, actual_column);
  if (exact && (actual_row != row || actual_column < 0 || actual_column >= columns ||
                static_cast<size_t>(actual_column) != CursesText::width(text)))
    throw Error("Approval review exceeded its assigned row");
}
} // namespace codet::detail
