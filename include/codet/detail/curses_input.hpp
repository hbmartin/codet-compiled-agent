#pragma once
#include "codet/detail/curses_text.hpp"
#include <array>
#include <vector>
namespace codet::detail {
// Raw bytes are authoritative; only the last (possibly incomplete) scalar is reparsed.
class CursesInput {
  struct Token {
    uint32_t source_end, display_end, columns;
  };
  std::string raw_, display_;
  std::vector<Token> tokens_;
  bool unicode_ = true;
  size_t rebuilt_ = 0;
  size_t changed_ = 0;
  struct Mutation {
    CursesInput &input;
    bool complete = false;
    ~Mutation() {
      if (!complete)
        input.clear();
    }
  };
  void rebuild(size_t start) {
    while (start < raw_.size()) {
      const auto bytes = static_cast<unsigned char>(raw_[start]) < 0x80
                             ? size_t{1}
                             : rendered_token(raw_, start).bytes;
      const CursesText token(literal_text(raw_.substr(start, bytes)), unicode_);
      const auto display_start = display_.size();
      display_ += token.text();
      start += bytes;
      for (size_t offset = 0; offset < token.text().size();) {
        const auto columns = CursesText::token_columns(token.text(), offset);
        offset += rendered_token(token.text(), offset).bytes;
        tokens_.push_back({static_cast<uint32_t>(start),
                           static_cast<uint32_t>(display_start + offset),
                           static_cast<uint32_t>(columns)});
      }
      rebuilt_ += bytes;
    }
  }
  size_t rewind() noexcept {
    if (tokens_.empty())
      return 0;
    const auto source_end = tokens_.back().source_end;
    do {
      tokens_.pop_back();
    } while (!tokens_.empty() && tokens_.back().source_end == source_end);
    const auto start = tokens_.empty() ? 0u : tokens_.back().source_end;
    display_.resize(tokens_.empty() ? 0u : tokens_.back().display_end);
    return start;
  }

public:
  struct Tail {
    std::string_view text;
    size_t columns = 0;
    bool omitted = false;
  };
  static constexpr size_t limit = 65536;
  const std::string &raw() const noexcept {
    return raw_;
  }
  bool empty() const noexcept {
    return raw_.empty();
  }
  size_t size() const noexcept {
    return raw_.size();
  }
  char back() const noexcept {
    return raw_.back();
  }
  size_t rebuilt_bytes() const noexcept {
    return rebuilt_;
  }
  std::string_view prepared() const noexcept {
    return display_;
  }
  size_t changed_offset() const noexcept {
    return changed_;
  }
  size_t committed_end() const noexcept {
    if (raw_.empty())
      return 0;
    size_t start = raw_.size() - 1;
    while (start && raw_.size() - start < 4 &&
           (static_cast<unsigned char>(raw_[start]) & 0xc0) == 0x80)
      --start;
    const auto lead = static_cast<unsigned char>(raw_[start]);
    const size_t expected = lead >= 0xc2 && lead <= 0xdf   ? 2u
                            : lead >= 0xe0 && lead <= 0xef ? 3u
                            : lead >= 0xf0 && lead <= 0xf4 ? 4u
                                                           : 0u;
    const auto length = raw_.size() - start;
    bool partial = expected && length < expected;
    for (size_t i = start + 1; partial && i < raw_.size(); ++i) {
      const auto byte = static_cast<unsigned char>(raw_[i]);
      partial = byte >= 0x80 && byte <= 0xbf;
      if (i == start + 1)
        partial = partial && !(lead == 0xe0 && byte < 0xa0) && !(lead == 0xed && byte > 0x9f) &&
                  !(lead == 0xf0 && byte < 0x90) && !(lead == 0xf4 && byte > 0x8f);
    }
    if (!partial)
      return display_.size();
    size_t token = tokens_.size();
    while (token && tokens_[token - 1].source_end > start)
      --token;
    return token ? tokens_[token - 1].display_end : 0u;
  }
  void unicode(bool enabled) {
    if (unicode_ != enabled) {
      Mutation mutation{*this};
      unicode_ = enabled;
      display_.clear();
      tokens_.clear();
      rebuild(0);
      mutation.complete = true;
    }
  }
  void clear() noexcept {
    raw_.clear();
    display_.clear();
    tokens_.clear();
    changed_ = 0;
  }
  void operator+=(char byte) {
    if (raw_.size() == limit)
      return;
    Mutation mutation{*this};
    std::array<char, 16> previous{};
    const auto old_end = display_.size();
    size_t token = tokens_.size();
    while (token && tokens_[token - 1].source_end == tokens_.back().source_end)
      --token;
    const size_t display_start = token ? tokens_[token - 1].display_end : 0u;
    const auto previous_size = std::min(old_end - display_start, previous.size());
    std::copy_n(display_.data() + display_start, previous_size, previous.data());
    const auto start = rewind();
    raw_ += byte;
    rebuild(start);
    size_t common = 0;
    while (common < previous_size && display_start + common < display_.size() &&
           previous[common] == display_[display_start + common])
      ++common;
    changed_ = display_start + common;
    mutation.complete = true;
  }
  void pop_back() {
    if (raw_.empty())
      return;
    Mutation mutation{*this};
    const auto start = rewind();
    raw_.pop_back();
    rebuild(start);
    mutation.complete = true;
  }
  void erase_character() {
    if (raw_.empty())
      return;
    Mutation mutation{*this};
    const auto end = raw_.size();
    size_t start = end - 1;
    // Inspect at most one scalar. A malformed byte is its own editing unit;
    // only a validated complete scalar or valid trailing prefix is grouped.
    while (start && end - start < 4 && (static_cast<unsigned char>(raw_[start]) & 0xc0) == 0x80)
      --start;
    const auto lead = static_cast<unsigned char>(raw_[start]);
    if (lead < 0xc2 || lead > 0xf4 || rendered_token(raw_, start).bytes != end - start)
      start = end - 1;
    while (!tokens_.empty() && tokens_.back().source_end > start)
      tokens_.pop_back();
    display_.resize(tokens_.empty() ? 0u : tokens_.back().display_end);
    raw_.resize(start);
    rebuild(tokens_.empty() ? 0u : tokens_.back().source_end);
    mutation.complete = true;
  }
  Tail tail(size_t columns, bool committed = false) const noexcept {
    const auto end = committed ? committed_end() : display_.size();
    size_t finish = tokens_.size();
    while (finish && tokens_[finish - 1].display_end > end)
      --finish;
    size_t start = finish, used = 0;
    while (start && tokens_[start - 1].columns <= columns - used) {
      used += tokens_[--start].columns;
    }
    if (start && columns) {
      --columns;
      while (start < finish && used > columns)
        used -= tokens_[start++].columns;
    }
    const auto offset = start ? tokens_[start - 1].display_end : 0u;
    // Token offsets belong to display_; avoid a throwing bounds-checked substr in this view.
    return {std::string_view(display_.data() + offset, end - offset), used, start != 0};
  }
  std::string view(std::string_view prompt, size_t columns) const {
    std::string result(prompt.substr(0, columns));
    const auto suffix = tail(columns - result.size());
    if (suffix.omitted && columns > result.size())
      result += '<';
    result += suffix.text;
    return result;
  }
};
} // namespace codet::detail
