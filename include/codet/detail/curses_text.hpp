#pragma once
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
  explicit CursesText(std::string_view rendered) {
    constexpr char hex[] = "0123456789ABCDEF";
    for (size_t offset = 0; offset < rendered.size();) {
      const auto parsed = rendered_token(rendered, offset);
      const auto token = rendered.substr(offset, parsed.bytes);
      if (parsed.valid && (parsed.point == '\n' || (parsed.point >= 0x20 && parsed.point != 0x7f &&
                                                    columns(token, parsed) >= 0)))
        text_.append(token);
      else
        for (const auto character : token) {
          const auto byte = static_cast<unsigned char>(character);
          const char escaped[] = {'\\', 'x', hex[byte >> 4u], hex[byte & 15u]};
          text_.append(escaped, sizeof(escaped));
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
  static std::string input(std::string_view prompt, const std::string &answer, size_t limit) {
    std::string result(prompt.substr(0, limit)); // Prompts are fixed ASCII.
    limit -= result.size();
    const CursesText rendered(literal_text(answer));
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
