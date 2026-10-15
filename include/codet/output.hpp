#pragma once
#include "codet/core.hpp"

namespace codet {
// Byte-preserving capture. Conversion to display text happens at tool boundaries.
class OutputSpool {
  fs::path directory_, path_;
  size_t limit_, quota_, records_, bytes_ = 0;
  int fd_ = -1;
  bool stopped_ = false, truncated_ = false;
  std::string error_;
  size_t available(size_t requested);
  void append_checked(const char *, size_t);

public:
  OutputSpool(fs::path directory, size_t limit, size_t quota, size_t records);
  ~OutputSpool();
  OutputSpool(const OutputSpool &) = delete;
  OutputSpool &operator=(const OutputSpool &) = delete;
  void append(const std::string &);
  void close();
  const fs::path &path() const {
    return path_;
  }
  size_t bytes() const {
    return bytes_;
  }
  bool truncated() const {
    return truncated_;
  }
  const std::string &error() const {
    return error_;
  }
};
std::string head_tail_text(const std::string &head, const std::string &tail, uint64_t total,
                           size_t limit);
} // namespace codet
