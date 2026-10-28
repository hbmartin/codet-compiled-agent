#pragma once
#include "codet/detail/approval_unicode.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <algorithm>
#include <cwchar>
#include <optional>

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

  static size_t prepare(std::string_view token, RenderedToken parsed, char *output, bool unicode,
                        bool prose_tabs, bool strict) noexcept {
    if (parsed.valid &&
        (parsed.point == '\n' || (prose_tabs && parsed.point == '\t') ||
         (parsed.point >= 0x20 && parsed.point != 0x7f && (unicode || parsed.point < 0x80) &&
          (!strict || parsed.point < 0x80 || !approval_invisible(parsed.point)) &&
          (columns(token, parsed) > 0 || (!strict && columns(token, parsed) == 0))))) {
      std::copy(token.begin(), token.end(), output);
      return token.size();
    }
    size_t written = 0;
    for (const auto character : token) {
      const auto escaped = escaped_byte(static_cast<unsigned char>(character));
      std::copy(escaped.begin(), escaped.end(), output + written);
      written += escaped.size();
    }
    return written;
  }

public:
  // Ordinary transcript rows may retain native tab expansion; approval layout never enables it.
  explicit CursesText(std::string_view rendered, bool unicode = true, bool prose_tabs = false,
                      bool strict = true) {
    for (size_t offset = 0; offset < rendered.size();) {
      const auto parsed = rendered_token(rendered, offset);
      if (parsed.valid && parsed.point < 0x80 &&
          (parsed.point == '\n' || (prose_tabs && parsed.point == '\t') ||
           (parsed.point >= 0x20 && parsed.point != 0x7f))) {
        text_.append(rendered.substr(offset, parsed.bytes));
        offset += parsed.bytes;
        continue;
      }
      char output[16]; // One UTF-8 scalar or one ASCII escape, expanded at most fourfold.
      const auto written = prepare(rendered.substr(offset, parsed.bytes), parsed, output, unicode,
                                   prose_tabs, strict);
      text_.append(output, written);
      offset += parsed.bytes;
    }
  }
  // Ordinary history only: raw rendering and locale preparation share a bounded pipeline.
  // Approval documents continue using the unlimited strict constructor above.
  static RenderedText bounded(std::string_view source, size_t budget, std::string_view marker,
                              bool unicode, std::optional<RenderMode> raw = {},
                              size_t *processed = nullptr) {
    RenderedText result;
    if (processed)
      *processed = 0;
    if (budget <= marker.size() + 1) {
      result.truncated = !source.empty();
      return result;
    }
    const auto available = budget - marker.size() - 1;
    size_t offset = 0;
    while (offset < source.size()) {
      char human[512];
      const auto first = raw ? RenderedToken{} : rendered_token(source, offset);
      const auto progress =
          raw ? terminal_text_into(source.substr(offset), human, sizeof(human), *raw)
              : RenderProgress{first.bytes, first.bytes};
      const auto fragment =
          raw ? std::string_view(human, progress.written) : source.substr(offset, progress.written);
      if (processed)
        *processed += progress.consumed;
      bool full = progress.consumed == 0;
      for (size_t part = 0; !full && part < fragment.size();) {
        const auto token = raw ? rendered_token(fragment, part) : first;
        char prepared[16];
        const bool ascii = token.valid && token.point < 0x80 &&
                           (token.point == '\n' || token.point == '\t' ||
                            (token.point >= 0x20 && token.point != 0x7f));
        const auto written = ascii ? token.bytes
                                   : prepare(fragment.substr(part, token.bytes), token, prepared,
                                             unicode, true, false);
        if (written > available - result.text.size()) {
          full = true;
          break;
        }
        if (ascii)
          result.text.append(fragment.substr(part, written));
        else
          result.text.append(prepared, written);
        part += token.bytes;
      }
      if (full)
        break;
      offset += progress.consumed;
    }
    result.truncated = offset != source.size();
    if (result.truncated) {
      result.text += '\n';
      result.text += marker;
    }
    return result;
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
