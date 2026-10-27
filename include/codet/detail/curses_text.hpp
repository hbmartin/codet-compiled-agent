#pragma once
#include "codet/detail/approval_unicode.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <algorithm>
#include <cwchar>

namespace codet::detail {
// Curses must draw exactly the representation whose columns were measured. Unsupported
// locale characters become ASCII byte escapes; the source text and answer stay unchanged.
class CursesText {
  std::string text_;
  static int columns(std::string_view token, RenderedToken parsed) noexcept {
    if (!parsed.valid)
      return -1;
    if (parsed.point < 0x80)
      return static_cast<int>(parsed.bytes);
    std::mbstate_t state{};
    wchar_t point{};
    return std::mbrtowc(&point, token.data(), token.size(), &state) == token.size()
               ? ::wcwidth(point)
               : -1;
  }

public:
  // Ordinary transcript rows may retain native tab expansion; approval layout never enables it.
  explicit CursesText(std::string_view rendered, bool unicode = true, bool prose_tabs = false,
                      bool strict = true) {
    for (size_t offset = 0; offset < rendered.size();) {
      const auto parsed = rendered_token(rendered, offset);
      const auto token = rendered.substr(offset, parsed.bytes);
      if (parsed.valid &&
          (parsed.point == '\n' || (prose_tabs && parsed.point == '\t') ||
           (parsed.point >= 0x20 && parsed.point != 0x7f && (unicode || parsed.point < 0x80) &&
            (!strict || parsed.point < 0x80 || !approval_invisible(parsed.point)) &&
            (columns(token, parsed) > 0 || (!strict && columns(token, parsed) == 0)))))
        text_.append(token);
      else
        for (const auto character : token) {
          const auto byte = static_cast<unsigned char>(character);
          const auto escaped = escaped_byte(byte);
          text_.append(escaped.data(), escaped.size());
        }
      offset += parsed.bytes;
    }
  }
  const std::string &text() const noexcept {
    return text_;
  }
  static size_t token_columns(std::string_view text, size_t offset) noexcept {
    const auto parsed = rendered_token(text, offset);
    const auto measured = columns(text.substr(offset, parsed.bytes), parsed);
    return measured >= 0 ? static_cast<size_t>(measured) : parsed.bytes * 4;
  }
  static size_t width(std::string_view text) noexcept {
    size_t result = 0;
    for (size_t offset = 0; offset < text.size();) {
      result += token_columns(text, offset);
      offset += rendered_token(text, offset).bytes;
    }
    return result;
  }
  static size_t prefix(std::string_view text, size_t limit, size_t tab_width = 8) noexcept {
    tab_width = std::max(tab_width, size_t{1});
    size_t offset = 0, used = 0;
    while (offset < text.size()) {
      const auto token = rendered_token(text, offset);
      if (token.point == '\n')
        break;
      const auto columns =
          token.point == '\t' ? tab_width - used % tab_width : token_columns(text, offset);
      if (columns > limit)
        break;
      limit -= columns;
      used += columns;
      offset += token.bytes;
    }
    return offset;
  }
  static std::string input(std::string_view prompt, const std::string &answer, size_t limit,
                           bool unicode = true) {
    std::string result(prompt.substr(0, limit)); // Prompts are fixed ASCII.
    limit -= result.size();
    const CursesText rendered(literal_text(answer), unicode);
    const auto &text = rendered.text();
    size_t remaining = width(text), start = 0;
    if (remaining > limit && limit) {
      result += '<';
      --limit;
    }
    while (start < text.size() && remaining > limit) {
      remaining -= token_columns(text, start);
      start += rendered_token(text, start).bytes;
    }
    result.append(text, start, text.size() - start);
    return result;
  }
};
} // namespace codet::detail
