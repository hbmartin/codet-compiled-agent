#include "codet/output.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

namespace codet {
namespace {
struct Descriptor {
  int fd;
  explicit Descriptor(int value) : fd(value) {}
  ~Descriptor() {
    if (fd >= 0)
      ::close(fd);
  }
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;
};
void require(bool good, const char *operation) {
  if (!good)
    throw Error(std::string(operation) + ": " + std::strerror(errno));
}
int open_regular(const fs::path &path, int flags, bool protect) {
  const int fd = ::open(path.c_str(), flags | O_NOFOLLOW | O_NONBLOCK, 0600);
  if (fd < 0)
    throw Error("Cannot open output capture: " + std::string(std::strerror(errno)));
  struct stat status {};
  if (::fstat(fd, &status) || !S_ISREG(status.st_mode) || status.st_uid != ::geteuid() ||
      (protect && ::fchmod(fd, 0600)) || ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    ::close(fd);
    throw Error("Output capture must be an owned private regular file");
  }
  return fd;
}
bool uuid_name(const std::string &name) {
  if (name.size() != 40 || name.substr(36) != ".txt")
    return false;
  for (size_t i = 0; i < 36; ++i) {
    const char ch = name[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (ch != '-')
        return false;
    } else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')))
      return false;
  }
  return true;
}
// The same maximal-subpart repair as safe_text, with source-byte accounting.
struct Unit {
  size_t bytes, display;
};
Unit unit(const std::string &text, size_t at, bool final) {
  const auto lead = static_cast<unsigned char>(text[at]);
  if (lead < 0x80)
    return {1, 1};
  const size_t width = lead >= 0xc2 && lead <= 0xdf   ? 2
                       : lead >= 0xe0 && lead <= 0xef ? 3
                       : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                      : 0;
  size_t accepted = 1;
  while (width && accepted < width && at + accepted < text.size()) {
    const auto byte = static_cast<unsigned char>(text[at + accepted]);
    const unsigned char low = accepted == 1 && lead == 0xe0   ? 0xa0
                              : accepted == 1 && lead == 0xf0 ? 0x90
                                                              : 0x80;
    const unsigned char high = accepted == 1 && lead == 0xed   ? 0x9f
                               : accepted == 1 && lead == 0xf4 ? 0x8f
                                                               : 0xbf;
    if (byte < low || byte > high)
      break;
    ++accepted;
  }
  if (width && accepted == width)
    return {width, width};
  if (width && at + accepted == text.size() && !final)
    return {0, 0};
  return {accepted, 3};
}
size_t display_size(const std::string &text, bool final) {
  size_t size = 0;
  for (size_t at = 0; at < text.size();) {
    const auto u = unit(text, at, final);
    if (!u.bytes)
      break;
    size += u.display;
    at += u.bytes;
  }
  return size;
}
size_t prefix_bytes(const std::string &text, size_t budget, bool final) {
  size_t at = 0;
  while (at < text.size()) {
    const auto u = unit(text, at, final);
    if (!u.bytes || u.display > budget)
      break;
    at += u.bytes;
    budget -= u.display;
  }
  return at;
}
std::pair<size_t, size_t> suffix_bytes(const std::string &text, size_t budget, size_t skip,
                                       bool final) {
  size_t end = 0, display = 0;
  while (end < text.size()) {
    const auto u = unit(text, end, final);
    if (!u.bytes)
      break;
    display += u.display;
    end += u.bytes;
  }
  size_t at = 0;
  while (at < end && (display > budget || at < skip)) {
    const auto u = unit(text, at, true);
    at += u.bytes;
    display -= u.display;
  }
  return {at, end};
}
} // namespace
bool capture_name(const fs::path &path) {
  auto name = path.filename().string();
  if (name.rfind("capture-", 0) == 0)
    name.erase(0, 8);
  return uuid_name(name);
}
ssize_t CaptureIo::write(int fd, const char *bytes, size_t count) {
  return ::write(fd, bytes, count);
}
int CaptureIo::sync(int fd) {
  return ::fsync(fd);
}
CaptureStore::CaptureStore(fs::path directory, size_t limit, size_t quota, size_t records,
                           DiagnosticSink diagnostics, std::shared_ptr<CaptureIo> io)
    : directory_(std::move(directory)), limit_(limit), quota_(quota), records_(records),
      diagnostics_(std::move(diagnostics)),
      io_(io ? std::move(io) : std::make_shared<CaptureIo>()) {}
void CaptureStore::warn(const std::string &message) {
  const auto text = bounded_text(message, 2048);
  // Diagnostics cannot make a large, hostile directory grow invocation memory without bound.
  if (warnings_.size() < 64 && warnings_.insert(text).second)
    report_diagnostic(diagnostics_, "Output capture: " + text);
}
OutputSpool::OutputSpool(std::shared_ptr<CaptureStore> store) : store_(std::move(store)) {
  pending_.reset(new char[batch_bytes]);
}
OutputSpool::OutputSpool(fs::path dir, size_t limit, size_t quota, size_t records)
    : OutputSpool(std::make_shared<CaptureStore>(std::move(dir), limit, quota, records)) {}
OutputSpool::~OutputSpool() {
  if (fd_ >= 0)
    ::close(fd_);
}
size_t OutputSpool::available(size_t requested) {
  return store_->reserve(*this, requested);
}
void OutputSpool::flush_checked() {
  if (pending_size_ == 0)
    return;
  if (fs::is_symlink(store_->directory_))
    throw Error("Output directory must not be a symlink");
  if (!admitted_) {
    fs::create_directories(store_->directory_);
    require(::chmod(store_->directory_.c_str(), 0700) == 0, "Protect output directory");
    admitted_ = true;
  }
  Descriptor lock(open_regular(store_->directory_ / ".capture.lock", O_CREAT | O_RDWR, true));
  require(::flock(lock.fd, LOCK_EX) == 0, "Lock output store");
  store_->prepare_catalog();
  ++store_->metrics.batches;
  if (fd_ < 0) {
    path_ = store_->directory_ / ("capture-" + unique_id() + ".txt");
    fd_ = open_regular(path_, O_CREAT | O_EXCL | O_RDWR, true);
    require(::flock(fd_, LOCK_EX | LOCK_NB) == 0, "Own output capture");
    ++store_->metrics.files;
  }
  const auto requested = std::min(pending_size_, store_->limit_ - bytes_);
  const auto keep = available(requested);
  if (keep < requested)
    error_ = "Output capture store capacity exhausted";
  size_t at = 0;
  while (at < keep) {
    const auto n = store_->io_->write(fd_, pending_.get() + at, keep - at);
    if (n < 0 && errno == EINTR)
      continue;
    require(n > 0, "Write output capture");
    at += static_cast<size_t>(n);
    bytes_ += static_cast<size_t>(n);
  }
  if (keep < pending_size_)
    stopped_ = truncated_ = true;
  pending_size_ = 0;
}
void OutputSpool::flush() {
  if (closed_ || stopped_)
    return;
  try {
    flush_checked();
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &e) {
    error_ = bounded_text(e.what(), 1024);
    stopped_ = truncated_ = true;
    pending_size_ = 0;
  }
}
void OutputSpool::append(const std::string &text) {
  if (closed_ || stopped_)
    return;
  size_t at = 0;
  while (at < text.size() && !stopped_) {
    const auto room =
        std::min(batch_bytes - pending_size_, store_->limit_ - bytes_ - pending_size_);
    const auto n = std::min(room, text.size() - at);
    std::memcpy(pending_.get() + pending_size_, text.data() + at, n);
    pending_size_ += n;
    at += n;
    if (pending_size_ == batch_bytes || !room || bytes_ + pending_size_ == store_->limit_)
      flush();
    if (!n && !stopped_) {
      stopped_ = truncated_ = true;
    }
  }
}
void OutputSpool::close() {
  if (closed_)
    return;
  struct Release {
    int &fd;
    bool &closed;
    std::unique_ptr<char[]> &pending;
    ~Release() {
      if (fd >= 0)
        ::close(fd);
      fd = -1;
      closed = true;
      pending.reset();
    }
  } release{fd_, closed_, pending_};
  flush();
  closed_ = true;
  pending_.reset();
  if (fd_ < 0)
    return;
  if (bytes_) {
    ++store_->metrics.syncs;
    if (store_->io_->sync(fd_) < 0) {
      error_ = "Cannot synchronize output capture";
      truncated_ = true;
    }
    if (::futimes(fd_, nullptr) < 0) {
      error_ = "Cannot timestamp output capture";
      truncated_ = true;
    }
  } else {
    // Only remove our own empty file, never a substituted directory entry.
    struct stat own {
    }, current{};
    if (::fstat(fd_, &own) == 0 && ::lstat(path_.c_str(), &current) == 0 &&
        own.st_dev == current.st_dev && own.st_ino == current.st_ino)
      (void)::unlink(path_.c_str());
  }
  ::close(fd_);
  fd_ = -1;
  try {
    store_->account_closed(*this);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &e) {
    error_ = e.what();
    truncated_ = true;
  }
}
RenderedOutput render_output(const OutputSnapshot &data, size_t limit) {
  const bool whole = data.total == data.head.size();
  if (whole && display_size(data.head, data.final) <= limit) {
    const auto kept = prefix_bytes(data.head, limit, data.final);
    return {safe_text(data.head.substr(0, kept)), 0};
  }
  const auto maximum_marker = "\n[... " + std::to_string(data.total) + " bytes omitted ...]\n";
  if (maximum_marker.size() >= limit)
    return {bounded_text("[Output omitted]", limit), data.total};
  const auto budget = limit - maximum_marker.size();
  const auto first = prefix_bytes(data.head, budget / 2, whole && data.final);
  const auto &tail = whole ? data.head : data.tail;
  const auto offset = data.total - tail.size();
  const auto last =
      suffix_bytes(tail, budget - safe_text(data.head.substr(0, first)).size(),
                   first > offset ? first - static_cast<size_t>(offset) : 0, data.final);
  const auto deferred = data.final ? 0 : tail.size() - prefix_bytes(tail, SIZE_MAX, false);
  const auto omitted = data.total - first - (last.second - last.first) - deferred;
  return {safe_text(data.head.substr(0, first)) + "\n[... " + std::to_string(omitted) +
              " bytes omitted ...]\n" +
              safe_text(tail.substr(last.first, last.second - last.first)),
          omitted};
}
fs::path capture_directory(const Config &config) {
  return config.capture_root.empty() ? invocation_directory(config) / "tool-output"
                                     : config.capture_root;
}
OutputCapture::OutputCapture(std::shared_ptr<CaptureStore> store, size_t limit)
    : store_(std::move(store)), limit_(limit) {
  data_.final = false;
}
void OutputCapture::retain() {
  if (!store_ || spool_)
    return;
  if (data_.total != data_.head.size())
    throw Error("Cannot activate capture after losing its prefix");
  spool_ = std::make_unique<OutputSpool>(store_);
  spool_->append(data_.head);
  if (finished_)
    spool_->close();
}
void OutputCapture::append(const std::string &text) {
  if (finished_)
    throw Error("Cannot append to completed output");
  for (size_t at = 0; at < text.size();) {
    const auto count = std::min(size_t{8192}, text.size() - at);
    const auto chunk = text.substr(at, count);
    at += count;
    const auto scan = pending_utf8_ + chunk;
    pending_utf8_.clear();
    for (size_t offset = 0; offset < scan.size();) {
      const auto next = unit(scan, offset, false);
      if (!next.bytes) {
        pending_utf8_ = scan.substr(offset);
        break;
      }
      display_bytes_ += next.display;
      if (store_)
        store_->metrics.display_bytes += next.bytes;
      offset += next.bytes;
    }
    const bool whole = data_.total == data_.head.size();
    if (whole && display_bytes_ > limit_)
      retain();
    if (spool_)
      spool_->append(chunk);
    data_.total += count;
    if (whole) {
      data_.head += chunk;
      if (display_bytes_ > limit_ && data_.head.size() > limit_) {
        const auto first = limit_ / 2 + 3, last = limit_ - limit_ / 2 + 3;
        data_.tail = data_.head.substr(data_.head.size() > last ? data_.head.size() - last : 0);
        data_.head.resize(std::min(first, data_.head.size()));
      }
    } else {
      const auto last = limit_ - limit_ / 2 + 3;
      if (chunk.size() >= last)
        data_.tail = chunk.substr(chunk.size() - last);
      else {
        data_.tail.erase(0, data_.tail.size() + chunk.size() > last
                                ? data_.tail.size() + chunk.size() - last
                                : 0);
        data_.tail += chunk;
      }
    }
  }
}
void OutputCapture::flush() {
  if (spool_)
    spool_->flush();
}
void OutputCapture::finish() {
  if (finished_)
    return;
  display_bytes_ += display_size(pending_utf8_, true);
  pending_utf8_.clear();
  if (data_.total == data_.head.size() && display_bytes_ > limit_)
    retain();
  finished_ = true;
  data_.final = true;
  if (spool_)
    spool_->close();
  std::string(data_.head).swap(data_.head);
  std::string(data_.tail).swap(data_.tail);
}
std::shared_ptr<const OutputSnapshot> OutputCapture::snapshot() const {
  return std::make_shared<const OutputSnapshot>(data_);
}
RenderedOutput OutputCapture::render(size_t limit) const {
  if (store_)
    ++store_->metrics.renders;
  return render_output(data_, limit);
}
size_t OutputCapture::memory_bytes() const {
  return data_.head.capacity() + data_.tail.capacity() + pending_utf8_.capacity() +
         (spool_ ? spool_->memory_bytes() : 0);
}
void OutputCapture::metadata(ProcessResult &result, uint64_t total) const {
  result.total_output_bytes = total;
  if (!spool_)
    return;
  result.spill_bytes = static_cast<size_t>(std::min(uint64_t{spool_->bytes()}, total));
  if (result.spill_bytes)
    result.spill_path = spool_->path().string();
  result.spill_truncated = result.spill_bytes < total && spool_->truncated();
  if (!spool_->error().empty())
    result.capture_error = spool_->error();
}
} // namespace codet
