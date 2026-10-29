#pragma once
#include "codet/detail/curses_text.hpp"
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
  }
  void operator+=(char byte) {
    if (raw_.size() == limit)
      return;
    Mutation mutation{*this};
    const auto start = rewind();
    raw_ += byte;
    rebuild(start);
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
  std::string view(std::string_view prompt, size_t columns) const {
    std::string result(prompt.substr(0, columns));
    columns -= result.size();
    size_t start = tokens_.size(), used = 0;
    while (start && tokens_[start - 1].columns <= columns - used) {
      used += tokens_[--start].columns;
    }
    if (start && columns) {
      result += '<';
      --columns;
      while (start < tokens_.size() && used > columns)
        used -= tokens_[start++].columns;
    }
    const auto offset = start ? tokens_[start - 1].display_end : 0u;
    result.append(display_, offset, display_.size() - offset);
    return result;
  }
};
} // namespace codet::detail
