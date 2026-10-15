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
int open_private(const fs::path &path, int flags) {
  const int fd = ::open(path.c_str(), flags | O_NOFOLLOW | O_NONBLOCK, 0600);
  if (fd < 0)
    throw Error("Cannot open output capture: " + std::string(std::strerror(errno)));
  struct stat status {};
  if (::fstat(fd, &status) || !S_ISREG(status.st_mode) || ::fchmod(fd, 0600) ||
      ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    ::close(fd);
    throw Error("Output capture must be a private regular file");
  }
  return fd;
}
} // namespace
OutputSpool::OutputSpool(fs::path directory, size_t limit, size_t quota, size_t records)
    : directory_(std::move(directory)), limit_(limit), quota_(quota), records_(records) {}
OutputSpool::~OutputSpool() {
  if (fd_ >= 0)
    ::close(fd_);
}
size_t OutputSpool::available(size_t requested) {
  struct Record {
    fs::path path;
    size_t bytes = 0;
    int64_t seconds = 0, nanoseconds = 0;
    bool active = false;
  };
  std::vector<Record> files;
  size_t total = 0, completed = 0;
  for (const auto &entry : fs::directory_iterator(directory_)) {
    const auto name = entry.path().filename().string();
    if (name.rfind("capture-", 0) || entry.path().extension() != ".txt")
      continue;
    Descriptor file(open_private(entry.path(), O_RDWR));
    struct stat status {};
    require(::fstat(file.fd, &status) == 0, "Inspect output capture");
    const bool own = fd_ >= 0 && entry.path() == path_;
    bool active = own;
    if (!own && ::flock(file.fd, LOCK_EX | LOCK_NB) < 0) {
      if (errno != EWOULDBLOCK && errno != EAGAIN)
        throw Error("Cannot inspect output ownership");
      active = true;
    }
    const auto size = static_cast<size_t>(status.st_size);
    if (size > SIZE_MAX - total)
      throw Error("Output store size overflow");
    total += size;
    if (!active)
      ++completed;
#ifdef __APPLE__
    const auto modified = status.st_mtimespec;
#else
    const auto modified = status.st_mtim;
#endif
    files.push_back({entry.path(), size, modified.tv_sec, modified.tv_nsec, active});
  }
  std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
    return std::tie(a.seconds, a.nanoseconds, a.path) < std::tie(b.seconds, b.nanoseconds, b.path);
  });
  for (const auto &file : files) {
    if (completed <= records_ && total <= quota_ && requested <= quota_ - total)
      break;
    if (file.active)
      continue;
    Descriptor owned(open_private(file.path, O_RDWR));
    if (::flock(owned.fd, LOCK_EX | LOCK_NB) < 0)
      continue;
    require(::unlink(file.path.c_str()) == 0, "Evict completed output capture");
    total -= file.bytes;
    --completed;
  }
  return total < quota_ ? std::min(requested, quota_ - total) : 0;
}
void OutputSpool::append_checked(const char *data, size_t count) {
  if (fs::is_symlink(directory_))
    throw Error("Output directory must not be a symlink");
  fs::create_directories(directory_);
  require(::chmod(directory_.c_str(), 0700) == 0, "Protect output directory");
  Descriptor lock(open_private(directory_ / ".capture.lock", O_CREAT | O_RDWR));
  require(::flock(lock.fd, LOCK_EX) == 0, "Lock output store");
  if (fd_ < 0) {
    path_ = directory_ / ("capture-" + unique_id() + ".txt");
    fd_ = open_private(path_, O_CREAT | O_EXCL | O_RDWR);
    require(::flock(fd_, LOCK_EX | LOCK_NB) == 0, "Own output capture");
  }
  const auto keep = available(std::min(count, limit_ - bytes_));
  size_t offset = 0;
  while (offset < keep) {
    const auto written = ::write(fd_, data + offset, keep - offset);
    if (written < 0 && errno == EINTR)
      continue;
    require(written > 0, "Write output capture");
    offset += static_cast<size_t>(written);
    bytes_ += static_cast<size_t>(written);
  }
  if (keep < count) {
    truncated_ = true;
    stopped_ = true;
  }
}
void OutputSpool::append(const std::string &text) {
  if (stopped_ || text.empty())
    return;
  try {
    append_checked(text.data(), text.size());
  } catch (const std::exception &error) {
    error_ = bounded_text(error.what(), 1024);
    stopped_ = truncated_ = true;
  }
}
void OutputSpool::close() {
  if (fd_ < 0)
    return;
  if (::fsync(fd_) < 0) {
    error_ = "Cannot synchronize output capture";
    truncated_ = true;
  }
  // Completion time, rather than last output time, orders completed-file eviction.
  if (::futimes(fd_, nullptr) < 0) {
    error_ = "Cannot timestamp output capture";
    truncated_ = true;
  }
  ::close(fd_);
  fd_ = -1;
  try {
    Descriptor lock(open_private(directory_ / ".capture.lock", O_CREAT | O_RDWR));
    require(::flock(lock.fd, LOCK_EX) == 0, "Lock output store");
    (void)available(0);
  } catch (const std::exception &error) {
    error_ = bounded_text(error.what(), 1024);
  }
}
std::string head_tail_text(const std::string &head, const std::string &tail, uint64_t total,
                           size_t limit) {
  if (total <= limit)
    return head + tail;
  // Reserve enough space for the marker before choosing the retained windows.
  const auto marker_size =
      std::string("\n[... " + std::to_string(total) + " bytes omitted ...]\n").size();
  if (marker_size >= limit)
    return std::string("[Output truncated]").substr(0, limit);
  const auto budget = limit - marker_size;
  const auto first = std::min(head.size(), budget / 2);
  const auto last = std::min(tail.size(), budget - first);
  const auto marker = "\n[... " + std::to_string(total - first - last) + " bytes omitted ...]\n";
  return head.substr(0, first) + marker + tail.substr(tail.size() - last);
}
} // namespace codet
