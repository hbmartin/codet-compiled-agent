#include "codet/detail/approval_review.hpp"
#include <clocale>
#include <cstdio>
#include <curses.h>
#include <gtest/gtest.h>

namespace {
struct Screen {
  std::string locale = std::setlocale(LC_CTYPE, nullptr);
  FILE *output = std::tmpfile(), *input = std::tmpfile();
  SCREEN *screen = nullptr;
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
    if (screen)
      ::set_term(screen);
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
    column += codet::detail::CursesText::token_columns(text, offset);
    offset += token.bytes;
  }
  EXPECT_EQ(mvinch(row, columns - 1) & A_CHARTEXT, static_cast<chtype>(' '));
}
TEST(ApprovalCurses, PreparedPagesFitRealCellsInCAndUtf8Locales) {
  for (const char *locale : {"C", "en_US.UTF-8", "C.UTF-8"}) {
    Screen native;
    if (!native.open(locale))
      continue;
    for (int columns : {20, 40, 80}) {
      ASSERT_EQ(::resizeterm(10, columns), OK);
      const auto document = codet::detail::literal_text(
          std::string(90, 'a') + "雪界e\xcc\x81👩‍💻\xff\r\n\t\\x0A; UNSAFE_SUFFIX");
      const auto expected = codet::detail::CursesText(document).text();
      if (std::string(locale) == "C") {
        EXPECT_EQ(expected.find("雪"), std::string::npos);
        EXPECT_NE(expected.find("\\xE9\\x9B\\xAA"), std::string::npos);
      }
      codet::detail::ApprovalReview review("session", 1, "request", document);
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
        for (size_t bytes = 0; bytes <= input.size(); ++bytes) {
          erase();
          const auto shown = codet::detail::CursesText::input(rows == 1 ? "!> " : "codet> ",
                                                              input.substr(0, bytes),
                                                              static_cast<size_t>(columns - 1));
          ASSERT_NO_FATAL_FAILURE(expect_row(rows - 1, shown, columns));
        }
      }
    }
  }
}
} // namespace
