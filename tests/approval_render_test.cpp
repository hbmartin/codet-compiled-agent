#define _XOPEN_SOURCE_EXTENDED 1
#include "codet/detail/approval_review.hpp"
#include "codet/detail/curses_input.hpp"
#include "codet/detail/curses_render.hpp"
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <gtest/gtest.h>

namespace {
struct Screen {
  std::string locale = std::setlocale(LC_CTYPE, nullptr);
  FILE *output = std::tmpfile(), *input = std::tmpfile();
  SCREEN *screen = nullptr;
  bool unicode = false;
  ~Screen() {
    if (screen) {
      (void)::endwin();
      ::delscreen(screen);
    }
    if (output)
      std::fclose(output);
    if (input)
      std::fclose(input);
    (void)std::setlocale(LC_CTYPE, locale.c_str());
  }
  bool open(const char *name) {
    if (!std::setlocale(LC_CTYPE, name) || !output || !input)
      return false;
    char terminal[] = "xterm";
    screen = ::newterm(terminal, output, input);
    if (screen) {
      ::set_term(screen);
      unicode = codet::detail::curses_unicode_supported();
    }
    return screen != nullptr;
  }
};
void expect_row(int row, std::string_view text, int columns) {
  if (text.empty())
    return;
  ASSERT_EQ(mvaddnstr(row, 0, text.data(), static_cast<int>(text.size())), OK);
  int y = 0, x = 0;
  getyx(stdscr, y, x);
  EXPECT_EQ(y, row);
  EXPECT_LT(x, columns);
  EXPECT_EQ(static_cast<size_t>(x), codet::detail::CursesText::width(text));
  // Inspect every ASCII cell (including complete escapes and the unsafe suffix).
  // Unicode is additionally checked by its measured cursor advancement and the
  // complete prepared-document reconstruction below; no wide-curses dependency.
  size_t column = 0;
  for (size_t offset = 0; offset < text.size();) {
    const auto token = codet::detail::rendered_token(text, offset);
    if (token.point < 0x80)
      for (size_t i = 0; i < token.bytes; ++i)
        EXPECT_EQ(mvinch(row, static_cast<int>(column + i)) & A_CHARTEXT,
                  static_cast<chtype>(text[offset + i]));
    if (token.point >= 0x80) {
      cchar_t cell{};
      ASSERT_EQ(mvwin_wch(stdscr, row, static_cast<int>(column), &cell), OK);
      wchar_t points[CCHARW_MAX + 1]{};
      attr_t attributes{};
      short pair{};
      ASSERT_EQ(getcchar(&cell, points, &attributes, &pair, nullptr), OK);
      EXPECT_EQ(static_cast<uint32_t>(points[0]), token.point);
      EXPECT_EQ(points[1], L'\0');
    }
    column += codet::detail::CursesText::token_columns(text, offset);
    offset += token.bytes;
  }
  EXPECT_EQ(mvinch(row, columns - 1) & A_CHARTEXT, static_cast<chtype>(' '));
}
TEST(ApprovalCurses, OrdinaryNativeFailuresKeepStateAndNextFrameRecovers) {
  Screen native;
  ASSERT_TRUE(native.open("C"));
  ASSERT_EQ(::resizeterm(10, 80), OK);
  for (int failure = 0; failure != 5; ++failure) {
    struct Failure {
      int stage;
    } state{failure};
    codet::detail::CursesDrawOps ops;
    ops.context = &state;
    ops.erase = +[](void *p) { return static_cast<Failure *>(p)->stage == 0 ? ERR : ::erase(); };
    ops.move = +[](void *p, int y, int x) {
      return static_cast<Failure *>(p)->stage == 1 ? ERR : ::move(y, x);
    };
    ops.clear = +[](void *p) { return static_cast<Failure *>(p)->stage == 2 ? ERR : ::clrtoeol(); };
    ops.put = +[](void *p, std::string_view text) {
      return static_cast<Failure *>(p)->stage == 3
                 ? ERR
                 : ::addnstr(text.data(), static_cast<int>(text.size()));
    };
    ops.refresh =
        +[](void *p) { return static_cast<Failure *>(p)->stage == 4 ? ERR : ::refresh(); };
    bool dirty = false;
    codet::detail::CursesFrame frame(dirty, ops);
    EXPECT_NO_THROW(frame.row(1, "retained progress", 80));
    EXPECT_FALSE(frame.finish());
    EXPECT_TRUE(dirty);
    codet::detail::ApprovalReview review("session", 1, "request", "command");
    review.reshape(10, 80);
    EXPECT_THROW(frame.require_finish(), codet::Error);
    EXPECT_FALSE(review.approved_review());
    state.stage = -1;
    codet::detail::CursesFrame retry(dirty, ops);
    EXPECT_TRUE(retry.row(1, "retained progress", 80));
    EXPECT_TRUE(retry.finish());
    EXPECT_FALSE(dirty);
    expect_row(1, "retained progress", 80);
  }
}
TEST(ApprovalCurses, PreparedPagesFitRealCellsInCAndUtf8Locales) {
  bool exercised_unicode = false;
  for (const char *locale : {"C", "en_US.UTF-8", "C.UTF-8"}) {
    Screen native;
    if (!native.open(locale))
      continue;
    exercised_unicode |= native.unicode;
    for (int columns : {20, 40, 80}) {
      ASSERT_EQ(::resizeterm(10, columns), OK);
      const auto document = codet::detail::literal_text(
          std::string(90, 'a') + "雪界e\xcc\x81👩‍💻\xff\r\n\t\\x0A; UNSAFE_SUFFIX");
      const auto expected = codet::detail::CursesText(document, native.unicode).text();
      if (std::string(locale) == "C") {
        EXPECT_EQ(expected.find("雪"), std::string::npos);
        EXPECT_NE(expected.find("\\xE9\\x9B\\xAA"), std::string::npos);
      }
      codet::detail::ApprovalReview review("session", 1, "request", document, native.unicode);
      review.reshape(10, columns);
      std::string reconstructed;
      for (;;) {
        erase();
        for (size_t i = 0; i < review.capacity(); ++i) {
          const auto row = review.line(i);
          ASSERT_NO_FATAL_FAILURE(expect_row(static_cast<int>(i) + 1, row, columns));
          reconstructed += row;
        }
        ASSERT_EQ(::refresh(), OK);
        review.displayed();
        if (review.approved_review())
          break;
        review.next();
      }
      EXPECT_EQ(reconstructed, expected);
    }
  }
  if (std::getenv("CODET_REQUIRE_UTF8_CURSES"))
    EXPECT_TRUE(exercised_unicode) << "Required UTF-8/wide-curses branch did not run";
}
TEST(ApprovalCurses, InputRemainsWithinBottomRowForEveryBytePrefixAndSmallScreen) {
  for (const char *locale : {"C", "en_US.UTF-8", "C.UTF-8"}) {
    Screen native;
    if (!native.open(locale))
      continue;
    const std::string input = "雪界👩‍💻\xff\\x0A" + std::string(70, 'x') + "/quit";
    for (int columns : {2, 5, 19, 20, 40, 80}) {
      for (int rows : {1, 2, 5, 10}) {
        ASSERT_EQ(::resizeterm(rows, columns), OK);
        codet::detail::CursesInput cached;
        cached.unicode(native.unicode);
        for (size_t bytes = 0; bytes <= input.size(); ++bytes) {
          if (bytes)
            cached += input[bytes - 1];
          erase();
          const auto shown =
              cached.view(rows == 1 ? "!> " : "codet> ", static_cast<size_t>(columns - 1));
          ASSERT_NO_FATAL_FAILURE(expect_row(rows - 1, shown, columns));
        }
      }
    }
  }
}
TEST(ApprovalCurses, InvisibleAndExcessCombiningBytesAreExplicitInRealCells) {
  Screen native;
  if (!native.open("en_US.UTF-8") && !native.open("C.UTF-8"))
    GTEST_SKIP() << "No UTF-8 locale available";
  ASSERT_EQ(::resizeterm(10, 80), OK);
  for (const char *raw : {u8"\u200c", u8"\u200d", u8"\ufe0f", u8"\U000e0061", u8"\u034f",
                          u8"\u115f", u8"\u1160", u8"\u3164", u8"\uffa0", u8"\u0488"}) {
    const auto prepared =
        codet::detail::CursesText(codet::detail::literal_text(raw), native.unicode).text();
    EXPECT_EQ(prepared.find(raw), std::string::npos);
    EXPECT_EQ(prepared.size(), std::strlen(raw) * 4);
    erase();
    ASSERT_NO_FATAL_FAILURE(expect_row(1, prepared, 80));
  }
  const std::string marks = u8"e\u0301\u0301\u0301\u0301\u0301\u0301\u0301\u0301\u0301";
  const auto prepared = codet::detail::CursesText(marks, native.unicode).text();
  EXPECT_EQ(prepared.size(), 73u);
  erase();
  ASSERT_NO_FATAL_FAILURE(expect_row(1, prepared, 80));
  ASSERT_EQ(::resizeterm(1, 40), OK);
  erase();
  mvaddstr(0, 0, "Resize to review; approval disabled");
  const auto input = codet::detail::CursesText::input("!> ", "n", 39, native.unicode);
  codet::detail::curses_draw_row(0, input, 40, true);
  EXPECT_EQ(mvinch(0, 4) & A_CHARTEXT, static_cast<chtype>(' '));
}
TEST(ApprovalCurses, BackendFallbackEscapesBytesWithoutChangingSupportedLocaleText) {
  Screen native;
  if (!native.open("en_US.UTF-8") && !native.open("C.UTF-8"))
    GTEST_SKIP() << "No UTF-8 locale available";
  EXPECT_FALSE(
      codet::detail::curses_unicode_supported(+[](int, int) -> WINDOW * { return nullptr; }));
  const auto raw = codet::detail::literal_text("雪\\x0A");
  EXPECT_EQ(codet::detail::CursesText(raw).text(), raw);
  const auto fallback = codet::detail::CursesText(raw, false).text();
  EXPECT_EQ(fallback, "\\xE9\\x9B\\xAA\\x5Cx0A");
  ASSERT_EQ(::resizeterm(10, 20), OK);
  codet::detail::ApprovalReview review("session", 1, "request", raw, false);
  review.reshape(10, 20);
  erase();
  expect_row(1, review.line(0), 20);
}
TEST(ApprovalCurses, ProseNewlinesAndTabsCannotEscapeTheirAssignedRow) {
  Screen native;
  ASSERT_TRUE(native.open("C"));
  ASSERT_EQ(::resizeterm(5, 20), OK);
  erase();
  codet::detail::curses_draw_row(0, "abc\nnext row", 20);
  EXPECT_EQ(mvinch(1, 0) & A_CHARTEXT, static_cast<chtype>(' '));
  codet::detail::curses_draw_row(4, "a\tb\tc", 20);
  EXPECT_EQ(mvinch(4, 8) & A_CHARTEXT, static_cast<chtype>('b'));
  EXPECT_EQ(mvinch(4, 16) & A_CHARTEXT, static_cast<chtype>('c'));
  ASSERT_EQ(::resizeterm(1, 8), OK);
  EXPECT_NO_THROW(codet::detail::curses_draw_row(0, "a\tb", 8));
  EXPECT_EQ(mvinch(0, 7) & A_CHARTEXT, static_cast<chtype>(' '));
}
} // namespace
