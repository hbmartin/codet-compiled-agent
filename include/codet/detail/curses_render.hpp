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
// Allocation-free native seam; injected operations can fail without replacing UI state.
struct CursesDrawOps {
  void *context = nullptr;
  int (*erase)(void *) = +[](void *) { return ::erase(); };
  int (*move)(void *, int, int) = +[](void *, int row, int column) { return ::move(row, column); };
  int (*clear)(void *) = +[](void *) { return ::clrtoeol(); };
  int (*put)(void *, std::string_view) = +[](void *, std::string_view text) {
    return ::addnstr(text.data(), static_cast<int>(text.size()));
  };
  int (*refresh)(void *) = +[](void *) { return ::refresh(); };
};
// Takes already prepared text. Every row has one clipping/clearing/drawing boundary.
inline bool curses_draw_row(int row, std::string_view text, int columns, bool exact = false,
                            const CursesDrawOps &ops = {}) {
  if (columns <= 1)
    return text.empty();
  const auto size = CursesText::prefix(text, static_cast<size_t>(columns - 1),
                                       static_cast<size_t>(std::max(TABSIZE, 1)));
  if (exact && size != text.size())
    return false;
  text = text.substr(0, size);
  if (ops.move(ops.context, row, 0) == ERR || ops.clear(ops.context) == ERR ||
      (!text.empty() && ops.put(ops.context, text) == ERR))
    return false;
  int actual_row = 0, actual_column = 0;
  getyx(stdscr, actual_row, actual_column);
  if (exact && (actual_row != row || actual_column < 0 || actual_column >= columns ||
                static_cast<size_t>(actual_column) != CursesText::width(text)))
    return false;
  return true;
}
class CursesFrame {
  bool &dirty_;
  const CursesDrawOps &ops_;
  bool success_;

public:
  explicit CursesFrame(bool &dirty, const CursesDrawOps &ops)
      : dirty_(dirty), ops_(ops), success_(ops.erase(ops.context) != ERR) {
    dirty_ = true;
  }
  bool row(int index, std::string_view text, int columns, bool exact = false) {
    const bool drawn = curses_draw_row(index, text, columns, exact, ops_);
    success_ = drawn && success_;
    return success_;
  }
  bool finish() {
    success_ = (ops_.refresh(ops_.context) != ERR) && success_;
    dirty_ = !success_;
    return success_;
  }
  void require_finish() {
    if (!finish())
      throw Error("Cannot render approval review");
  }
};
} // namespace codet::detail
