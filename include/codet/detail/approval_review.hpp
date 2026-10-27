#pragma once
#include "codet/detail/curses_text.hpp"
#include <algorithm>
#include <climits>
#include <vector>

namespace codet::detail {
// The review document is independent of transcript retention and diagnostic budgets.
class ApprovalReview {
  struct Row {
    size_t offset, size;
  };
  CursesText document_;
  std::vector<Row> lines_;
  size_t page_ = 0, shown_ = 0;
  int rows_ = 0, columns_ = 0;

public:
  std::string session, request;
  uint64_t turn;
  ApprovalReview(std::string session_id, uint64_t turn_id, std::string request_id,
                 std::string document, bool unicode = true)
      : document_(document, unicode), session(std::move(session_id)),
        request(std::move(request_id)), turn(turn_id) {}
  bool reshape(int rows, int columns) {
    if (rows == rows_ && columns == columns_)
      return false;
    rows_ = rows;
    columns_ = columns;
    page_ = shown_ = 0;
    lines_.clear();
    if (!usable())
      return true;
    const auto limit = static_cast<size_t>(columns_ - 1);
    const auto &document = document_.text();
    size_t start = 0, offset = 0, used = 0;
    while (offset < document.size()) {
      if (document[offset] == '\n') {
        lines_.push_back({start, offset - start});
        start = ++offset;
        used = 0;
        continue;
      }
      const auto bytes = rendered_token(document, offset).bytes;
      const auto columns_used = CursesText::token_columns(document, offset);
      if (columns_used > limit - used || offset - start > size_t{INT_MAX} - bytes) {
        lines_.push_back({start, offset - start});
        start = offset;
        used = 0;
      }
      offset += bytes;
      used += columns_used;
    }
    lines_.push_back({start, offset - start});
    return true;
  }
  bool usable() const noexcept {
    return rows_ >= 6 && columns_ >= 20;
  }
  const std::string &document() const noexcept {
    return document_.text();
  }
  size_t capacity() const noexcept {
    return usable() ? static_cast<size_t>(rows_ - 4) : 0;
  }
  size_t pages() const noexcept {
    return usable() ? (lines_.size() + capacity() - 1) / capacity() : 0;
  }
  size_t page() const noexcept {
    return page_;
  }
  std::string_view line(size_t index) const noexcept {
    const auto row = page_ * capacity() + index;
    if (row >= lines_.size())
      return {};
    return {document_.text().data() + lines_[row].offset, lines_[row].size};
  }
  // Called only after the whole page has rendered successfully.
  void displayed() noexcept {
    if (usable() && page_ <= shown_)
      shown_ = std::max(shown_, page_ + 1);
  }
  bool approved_review() const noexcept {
    return usable() && shown_ == pages();
  }
  void next() noexcept {
    if (usable() && page_ < shown_ && page_ + 1 < pages())
      ++page_;
  }
  void previous() noexcept {
    if (page_)
      --page_;
  }
};
} // namespace codet::detail
