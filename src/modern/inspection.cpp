#include "codet/core.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
namespace codet {
namespace {
class MemoryRead final : public ReadHandle {
  const FileSystem &files_;
  fs::path path_;
  mutable std::optional<std::string> bytes_;
  size_t offset_ = 0;
  const std::string &bytes() const {
    if (!bytes_)
      bytes_ = files_.read(path_);
    return *bytes_;
  }

public:
  MemoryRead(const FileSystem &files, fs::path path) : files_(files), path_(std::move(path)) {}
  fs::path path() const override {
    return files_.canonical(path_);
  }
  uint64_t size() const override {
    return bytes().size();
  }
  size_t read(char *out, size_t count) override {
    const auto n = std::min(count, bytes().size() - offset_);
    std::memcpy(out, bytes().data() + offset_, n);
    offset_ += n;
    return n;
  }
  void seek(uint64_t offset) override {
    offset_ = static_cast<size_t>(std::min(offset, size()));
  }
};
class NativeRead final : public ReadHandle {
  int fd_ = -1;
  fs::path opened_;
  uint64_t size_ = 0;

public:
  explicit NativeRead(const fs::path &path) {
    fd_ = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    try {
      struct stat st {};
      if (fd_ < 0 || ::fstat(fd_, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
          ::fcntl(fd_, F_SETFD, FD_CLOEXEC) < 0)
        throw Error("Cannot open regular file for inspection: " + path.string());
      char target[PATH_MAX + 1]{};
#ifdef __APPLE__
      if (::fcntl(fd_, F_GETPATH, target) < 0)
        throw Error("Cannot inspect opened file path");
#else
      const auto link = "/proc/self/fd/" + std::to_string(fd_);
      const auto n = ::readlink(link.c_str(), target, PATH_MAX);
      if (n < 0 || n >= PATH_MAX)
        throw Error("Cannot inspect opened file path");
      target[n] = '\0';
#endif
      opened_ = fs::weakly_canonical(target);
      struct stat named {};
      if (::stat(opened_.c_str(), &named) || named.st_dev != st.st_dev || named.st_ino != st.st_ino)
        throw Error("Inspection target changed while opening");
      size_ = static_cast<uint64_t>(st.st_size);
    } catch (...) {
      if (fd_ >= 0)
        ::close(fd_);
      fd_ = -1;
      throw;
    }
  }
  NativeRead(const NativeRead &) = delete;
  NativeRead &operator=(const NativeRead &) = delete;
  ~NativeRead() override {
    if (fd_ >= 0)
      ::close(fd_);
  }
  fs::path path() const override {
    return opened_;
  }
  uint64_t size() const override {
    return size_;
  }
  size_t read(char *out, size_t count) override {
    ssize_t n;
    do {
      n = ::read(fd_, out, count);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
      throw Error("Inspection read failed: " + std::string(std::strerror(errno)));
    return static_cast<size_t>(n);
  }
  void seek(uint64_t offset) override {
    if (offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max()) ||
        ::lseek(fd_, static_cast<off_t>(offset), SEEK_SET) < 0)
      throw Error("Invalid inspection byte offset");
  }
};
} // namespace
std::unique_ptr<ReadHandle> FileSystem::open_read(const fs::path &path) const {
  return std::make_unique<MemoryRead>(*this, path);
}
std::unique_ptr<ReadHandle> NativeFileSystem::open_read(const fs::path &path) const {
  return std::make_unique<NativeRead>(path);
}
} // namespace codet
