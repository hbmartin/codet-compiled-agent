// Adapted from OpenAI Codex apply-patch (Copyright 2025 OpenAI).
// C++ translation; see licenses/codex-APACHE-2.0.txt and codex-NOTICE.txt.
#include "codet/patch.hpp"
#include <algorithm>

namespace codet {
namespace {
struct Line {
  std::string text, ending;
};
struct Replacement {
  size_t start, length;
  std::vector<std::string> lines;
};
struct Character {
  uint32_t value;
  size_t width;
};
Character character(const std::string &text, size_t offset) {
  const auto first = static_cast<unsigned char>(text[offset]);
  size_t width = first < 0x80                    ? 1
                 : first >= 0xf0 && first < 0xf5 ? 4
                 : first >= 0xe0 && first < 0xf0 ? 3
                 : first >= 0xc2 && first < 0xe0 ? 2
                                                 : 1;
  uint32_t value = width == 1 ? first : first & ((1u << (7 - width)) - 1);
  if (width > text.size() - offset)
    return {first, 1};
  for (size_t i = 1; i < width; ++i) {
    const auto next = static_cast<unsigned char>(text[offset + i]);
    if ((next & 0xc0) != 0x80)
      return {first, 1};
    value = (value << 6) | (next & 0x3f);
  }
  return {value, width};
}
bool whitespace(uint32_t c) {
  return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
         c == 0x3000;
}
std::string comparable(const std::string &text, int pass) {
  if (!pass)
    return text;
  size_t begin = 0, end = 0;
  for (size_t offset = 0; offset < text.size();) {
    const auto decoded = character(text, offset);
    offset += decoded.width;
    if (!whitespace(decoded.value))
      end = offset;
  }
  if (pass > 1)
    while (begin < end) {
      const auto decoded = character(text, begin);
      if (!whitespace(decoded.value))
        break;
      begin += decoded.width;
    }
  std::string out;
  out.reserve(end - begin);
  for (size_t offset = begin; offset < end;) {
    const auto decoded = character(text, offset);
    const auto c = decoded.value;
    if (pass == 3 && ((c >= 0x2010 && c <= 0x2015) || c == 0x2212))
      out += '-';
    else if (pass == 3 && c >= 0x2018 && c <= 0x201b)
      out += '\'';
    else if (pass == 3 && c >= 0x201c && c <= 0x201f)
      out += '"';
    else if (pass == 3 && (c == 0xa0 || (c >= 0x2002 && c <= 0x200a) || c == 0x202f ||
                           c == 0x205f || c == 0x3000))
      out += ' ';
    else
      out.append(text, offset, decoded.width);
    offset += decoded.width;
  }
  return out;
}
std::optional<size_t> seek(const std::vector<Line> &source, const std::vector<std::string> &pattern,
                           size_t cursor, bool eof, const Cancellation &cancel) {
  if (pattern.empty())
    return cursor;
  if (pattern.size() > source.size())
    return {};
  const auto start = eof ? std::max(cursor, source.size() - pattern.size()) : cursor;
  for (int pass = 0; pass < 4; ++pass)
    for (size_t at = start; at <= source.size() - pattern.size(); ++at) {
      if ((at & 255) == 0)
        cancel.check();
      bool matches = true;
      for (size_t j = 0; j < pattern.size(); ++j)
        if ((pass == 0 && source[at + j].text != pattern[j]) ||
            (pass != 0 && comparable(source[at + j].text, pass) != comparable(pattern[j], pass))) {
          matches = false;
          break;
        }
      if (matches)
        return at;
    }
  return {};
}
} // namespace
std::string patch_update(const std::string &text, const std::vector<std::string> &patch,
                         size_t &index, const Cancellation &cancel) {
  std::vector<Line> source;
  std::string preferred = "\n";
  bool chose_ending = false;
  for (size_t start = 0; start < text.size();) {
    const auto end = text.find_first_of("\r\n", start);
    if (end == std::string::npos) {
      source.push_back({text.substr(start), ""});
      break;
    }
    const size_t width =
        text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1;
    const auto ending = text.substr(end, width);
    if (!chose_ending) {
      preferred = ending;
      chose_ending = true;
    }
    source.push_back({text.substr(start, end - start), ending});
    start = end + width;
  }
  std::vector<Replacement> replacements;
  size_t cursor = 0, chunks = 0;
  while (index + 1 < patch.size() && patch[index].rfind("*** ", 0) != 0) {
    cancel.check();
    if (patch[index].empty()) {
      ++index;
      continue;
    }
    if (patch[index] == "@@")
      ++index;
    else if (patch[index].rfind("@@ ", 0) == 0) {
      const auto anchor = seek(source, {patch[index++].substr(3)}, cursor, false, cancel);
      if (!anchor)
        throw Error("Patch anchor not found");
      cursor = *anchor + 1;
    } else if (chunks)
      throw Error("Expected @@ patch hunk");
    std::vector<std::string> before, after;
    std::vector<std::pair<size_t, size_t>> context;
    bool eof = false;
    size_t body = 0;
    while (index + 1 < patch.size() && patch[index].rfind("@@", 0) != 0 &&
           patch[index].rfind("*** ", 0) != 0) {
      const auto &line = patch[index++];
      if (line.empty()) {
        before.push_back("");
        after.push_back("");
        context.emplace_back(before.size() - 1, after.size() - 1);
      } else if (line[0] == ' ') {
        context.emplace_back(before.size(), after.size());
        before.push_back(line.substr(1));
        after.push_back(line.substr(1));
      } else if (line[0] == '-')
        before.push_back(line.substr(1));
      else if (line[0] == '+')
        after.push_back(line.substr(1));
      else
        throw Error("Invalid patch line");
      ++body;
    }
    if (!body)
      throw Error("Empty update hunk");
    if (index < patch.size() && patch[index] == "*** End of File") {
      eof = true;
      ++index;
    }
    ++chunks;
    if (before.empty()) {
      replacements.push_back({source.size(), 0, after});
      continue;
    }
    auto at = seek(source, before, cursor, eof, cancel);
    if (!at && before.back().empty()) {
      before.pop_back();
      if (!after.empty() && after.back().empty())
        after.pop_back();
      at = seek(source, before, cursor, eof, cancel);
    }
    if (!at)
      throw Error("Patch context not found after previous hunk");
    const auto matched_at = at.value();
    size_t old_start = 0, new_start = 0;
    for (const auto &pair : context) {
      if (pair.first >= before.size() || pair.second >= after.size())
        break;
      if (pair.first != old_start || pair.second != new_start)
        replacements.push_back({matched_at + old_start,
                                pair.first - old_start,
                                {after.begin() + static_cast<ptrdiff_t>(new_start),
                                 after.begin() + static_cast<ptrdiff_t>(pair.second)}});
      old_start = pair.first + 1;
      new_start = pair.second + 1;
    }
    if (old_start != before.size() || new_start != after.size())
      replacements.push_back({matched_at + old_start,
                              before.size() - old_start,
                              {after.begin() + static_cast<ptrdiff_t>(new_start), after.end()}});
    cursor = matched_at + before.size();
  }
  if (!chunks)
    throw Error("Empty update file patch");
  std::stable_sort(replacements.begin(), replacements.end(),
                   [](const auto &a, const auto &b) { return a.start < b.start; });
  std::string result;
  size_t position = 0;
  const auto append_source = [&](size_t end) {
    while (position < end) {
      const auto &line = source[position++];
      result += line.text + (line.ending.empty() ? preferred : line.ending);
    }
  };
  for (const auto &replacement : replacements) {
    if (replacement.start < position)
      throw Error("Overlapping patch replacements");
    append_source(replacement.start);
    position += replacement.length;
    for (const auto &line : replacement.lines)
      result += line + preferred;
  }
  append_source(source.size());
  return result;
}
} // namespace codet
