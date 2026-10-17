#pragma once
#include "codet/core.hpp"
#include <sys/types.h>

namespace codet {
struct CaptureMetrics {
  size_t scans = 0, batches = 0, syncs = 0, files = 0, renders = 0, display_bytes = 0;
};
struct CaptureIo {
  virtual ~CaptureIo() = default;
  virtual ssize_t write(int, const char *, size_t);
  virtual int sync(int);
};
class CaptureStore {
  fs::path directory_;
  size_t limit_, quota_, records_;
  DiagnosticSink diagnostics_;
  std::shared_ptr<CaptureIo> io_;
  std::set<std::string> warnings_;
  friend class OutputSpool;

public:
  CaptureMetrics metrics;
  explicit CaptureStore(fs::path, size_t limit = size_t{8} * 1024 * 1024,
                        size_t quota = size_t{64} * 1024 * 1024, size_t records = 64,
                        DiagnosticSink diagnostics = {}, std::shared_ptr<CaptureIo> io = {});
  void warn(const std::string &);
};
// Disk capture is byte-preserving. A bounded staging buffer is flushed at publication/close.
class OutputSpool {
  std::shared_ptr<CaptureStore> store_;
  fs::path path_;
  size_t bytes_ = 0, pending_size_ = 0;
  int fd_ = -1;
  bool stopped_ = false, truncated_ = false, closed_ = false, admitted_ = false;
  std::string error_;
  std::unique_ptr<char[]> pending_;
  size_t available(size_t requested);
  void flush_checked();

public:
  static constexpr size_t batch_bytes = 65536;
  explicit OutputSpool(std::shared_ptr<CaptureStore>);
  OutputSpool(fs::path directory, size_t limit, size_t quota, size_t records);
  ~OutputSpool();
  OutputSpool(const OutputSpool &) = delete;
  OutputSpool &operator=(const OutputSpool &) = delete;
  void append(const std::string &);
  void flush();
  void close();
  const fs::path &path() const {
    return path_;
  }
  size_t bytes() const {
    return bytes_;
  }
  size_t memory_bytes() const {
    return pending_ ? batch_bytes : 0;
  }
  bool truncated() const {
    return truncated_;
  }
  const std::string &error() const {
    return error_;
  }
};
struct OutputSnapshot {
  std::string head, tail;
  uint64_t total = 0;
  bool final = true;
};
struct RenderedOutput {
  std::string text;
  uint64_t omitted = 0;
};
RenderedOutput render_output(const OutputSnapshot &, size_t limit);
fs::path capture_directory(const Config &);
bool capture_name(const fs::path &);
// Owns raw windows and the optional lazy capture through process and tool lifetimes.
class OutputCapture {
  std::shared_ptr<CaptureStore> store_;
  size_t limit_;
  OutputSnapshot data_;
  std::unique_ptr<OutputSpool> spool_;
  bool finished_ = false;
  size_t display_bytes_ = 0;
  std::string pending_utf8_;

public:
  OutputCapture(std::shared_ptr<CaptureStore>, size_t limit);
  void append(const std::string &);
  void retain();
  void flush();
  void finish();
  std::shared_ptr<const OutputSnapshot> snapshot() const;
  RenderedOutput render(size_t limit) const;
  uint64_t total() const {
    return data_.total;
  }
  size_t memory_bytes() const;
  void metadata(ProcessResult &, uint64_t total) const;
};
} // namespace codet
