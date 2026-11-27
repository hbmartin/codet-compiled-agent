#include "codet/patch.hpp"
#include "codet/support.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <copyfile.h>
#include <sys/acl.h>
#include <sys/xattr.h>
#endif

namespace codet {
namespace {
[[noreturn]] void io_error(const char *operation, const fs::path &path) {
  const int error = errno;
  throw Error(std::string(operation) + " " + path.string() + ": " + std::strerror(error));
}

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

void write_bytes(int fd, const char *data, size_t size, const fs::path &path) {
  while (size) {
    const auto n = ::write(fd, data, std::min<size_t>(size, 65536));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      io_error("Write patch stage", path);
    data += n;
    size -= static_cast<size_t>(n);
  }
}

struct stat inspect(const fs::path &path) {
  struct stat status {};
  if (::lstat(path.c_str(), &status) != 0)
    io_error("Inspect patch file", path);
  if (!S_ISREG(status.st_mode))
    throw Error("Patch requires a regular file: " + path.string());
  return status;
}

bool same_identity(const struct stat &a, const struct stat &b) {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

bool same_version(const struct stat &a, const struct stat &b, bool check_ctime = true) {
#ifdef __APPLE__
  if (a.st_flags != b.st_flags)
    return false;
  const auto modified_a = a.st_mtimespec, modified_b = b.st_mtimespec;
  const auto changed_a = a.st_ctimespec, changed_b = b.st_ctimespec;
#else
  const auto modified_a = a.st_mtim, modified_b = b.st_mtim;
  const auto changed_a = a.st_ctim, changed_b = b.st_ctim;
#endif
  return same_identity(a, b) && a.st_size == b.st_size && a.st_mode == b.st_mode &&
         a.st_uid == b.st_uid && a.st_gid == b.st_gid && modified_a.tv_sec == modified_b.tv_sec &&
         modified_a.tv_nsec == modified_b.tv_nsec &&
         (!check_ctime ||
          (changed_a.tv_sec == changed_b.tv_sec && changed_a.tv_nsec == changed_b.tv_nsec));
}

void require_version(const fs::path &path, const struct stat &expected, const std::string &hash) {
  NativeFileSystem files;
  if (!same_version(inspect(path), expected) || digest(files.read(path)) != hash ||
      !same_version(inspect(path), expected))
    throw Error("File changed during patch: " + path.string());
}

class Stage {
  fs::path directory_;
  bool keep_ = false;

public:
  fs::path file, retired;
  explicit Stage(const fs::path &parent) {
    auto pattern = (parent / ".codet-patch-XXXXXX").string();
    if (!::mkdtemp(pattern.data()))
      io_error("Create private patch stage in", parent);
    directory_ = pattern;
    file = directory_ / "data";
    retired = directory_ / "retired";
  }
  ~Stage() {
    if (!keep_) {
      (void)::unlink(retired.c_str());
      (void)::unlink(file.c_str());
      (void)::rmdir(directory_.c_str());
    }
  }
  Stage(const Stage &) = delete;
  Stage &operator=(const Stage &) = delete;
  void keep() noexcept {
    keep_ = true;
  }
  void restore_retired(const fs::path &path, const PatchFileOps &ops) {
    try {
      ops.link_exclusive(retired, path);
      ops.sync_directory(path.parent_path());
    } catch (const std::exception &error) {
      keep();
      throw Error("Could not restore " + path.string() + "; retained at " + retired.string() +
                  ": " + error.what());
    }
  }
};

// POSIX has no conditional unlink. Move the public name into a private directory, then
// verify what actually moved before permitting cleanup. If another writer won the race,
// restore that exact inode without overwriting a new occupant.
void retire_verified(const fs::path &path, Stage &stage, const struct stat &expected,
                     const std::string &hash, const PatchFileOps &ops) {
  ops.retire(path, stage.retired);
  try {
    const auto captured = inspect(stage.retired);
    // A rename can change ctime. Check the other fields before verifying the captured version.
    if (!same_version(expected, captured, false))
      throw Error("File changed during patch: " + path.string());
    require_version(stage.retired, captured, hash);
  } catch (...) {
    stage.restore_retired(path, ops);
    throw;
  }
}

// The checked pathname can change before rename. Capture its occupant in private storage,
// then claim the public name exclusively. Apply the same ordering when undoing an Update.
void replace_verified(const fs::path &path, Stage &displaced, const struct stat &expected,
                      const std::string &hash, const fs::path &replacement,
                      const PatchFileOps &ops) {
  retire_verified(path, displaced, expected, hash, ops);
  try {
    ops.link_exclusive(replacement, path);
  } catch (...) {
    displaced.restore_retired(path, ops);
    throw;
  }
}

void create_parents(const fs::path &parent, const PatchFileOps &ops) {
  std::vector<fs::path> missing;
  for (auto path = parent; !fs::exists(path); path = path.parent_path()) {
    if (path.empty() || path == path.root_path())
      throw Error("Missing patch destination root");
    missing.push_back(path);
  }
  for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
    fs::create_directory(*it);
    ops.sync_directory(it->parent_path());
  }
}

#ifdef __APPLE__
size_t attr_size(int fd, const std::string &name, const fs::path &path) {
  const auto n = ::fgetxattr(fd, name.c_str(), nullptr, 0, 0, 0);
  if (n < 0)
    io_error("Read patch attribute in", path);
  return static_cast<size_t>(n);
}
std::set<std::string> attr_names(int fd, const fs::path &path) {
  const auto n = ::flistxattr(fd, nullptr, 0, 0);
  if (n < 0)
    io_error("Enumerate patch attributes in", path);
  if (n > 1024 * 1024)
    throw Error("Attribute list too large to verify: " + path.string());
  if (!n)
    return {};
  std::string names(static_cast<size_t>(n), '\0');
  if (::flistxattr(fd, names.data(), names.size(), 0) != n)
    throw Error("Cannot verify attribute list: " + path.string());
  std::set<std::string> result;
  for (size_t i = 0; i < names.size();) {
    const auto end = names.find('\0', i);
    if (end == std::string::npos || end == i)
      throw Error("Invalid attribute list: " + path.string());
    result.emplace(names.substr(i, end - i));
    i = end + 1;
  }
  return result;
}
std::string attribute(int fd, const std::string &name, const fs::path &path) {
  const auto n = attr_size(fd, name, path);
  if (n > 1024 * 1024)
    throw Error("Attribute too large to preserve: " + path.string() + " (" + name + ")");
  std::string value(n, '\0');
  if (::fgetxattr(fd, name.c_str(), value.data(), n, 0, 0) != static_cast<ssize_t>(n))
    throw Error("Cannot read attribute: " + path.string() + " (" + name + ")");
  return value;
}
std::string acl_text(int fd, const fs::path &path) {
  const auto acl = ::acl_get_fd(fd);
  if (!acl) {
    if (errno == ENOENT)
      return {};
    io_error("Read patch ACL on", path);
  }
  std::unique_ptr<void, decltype(&::acl_free)> owner(acl, ::acl_free);
  ssize_t size = 0;
  char *text = ::acl_to_text(acl, &size);
  if (!text || size < 0)
    io_error("Inspect patch ACL on", path);
  std::unique_ptr<void, decltype(&::acl_free)> text_owner(text, ::acl_free);
  return {text, static_cast<size_t>(size)};
}
// Resource forks can exceed the entire working memory of a target machine.
void transfer_fork(const fs::path &source, const fs::path &target, bool verify,
                   const Cancellation &cancel) {
  const auto from = source / "..namedfork/rsrc", to = target / "..namedfork/rsrc";
  Descriptor input(::open(from.c_str(), O_RDONLY));
  Descriptor output(::open(to.c_str(), verify ? O_RDONLY : O_WRONLY | O_TRUNC | O_CREAT, 0600));
  if (input.fd < 0 || output.fd < 0)
    throw Error("Cannot open resource fork for patch: " + source.string());
  std::array<char, 32768> data{}, other{};
  for (;;) {
    cancel.check();
    const auto n = ::read(input.fd, data.data(), data.size());
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      io_error("Read patch resource fork", from);
    if (verify) {
      size_t done = 0;
      do {
        const auto want = n ? static_cast<size_t>(n) - done : size_t{1};
        ssize_t got;
        do {
          got = ::read(output.fd, other.data(), want);
        } while (got < 0 && errno == EINTR);
        if (got < 0 || (!n && got) || (n && !got) ||
            std::memcmp(data.data() + done, other.data(), static_cast<size_t>(got)) != 0)
          throw Error("Resource fork verification failed: " + source.string());
        done += static_cast<size_t>(got);
        if (!n)
          break;
      } while (done < static_cast<size_t>(n));
    } else {
      write_bytes(output.fd, data.data(), static_cast<size_t>(n), to);
    }
    if (!n)
      break;
  }
  if (!verify && ::fsync(output.fd))
    io_error("Sync staged resource fork", to);
}
#endif

#ifdef __APPLE__
void verify_metadata(const fs::path &source, int source_fd, const fs::path &staged, int staged_fd,
                     const Cancellation &cancel) {
  const auto names = attr_names(source_fd, source);
  if (acl_text(staged_fd, staged) != acl_text(source_fd, source) ||
      attr_names(staged_fd, staged) != names)
    throw Error("Cannot verify staged Mac metadata: " + source.string());
  for (const auto &name : names) {
    cancel.check();
    if (name == "com.apple.ResourceFork")
      transfer_fork(source, staged, true, cancel);
    else if (attribute(source_fd, name, source) != attribute(staged_fd, name, staged))
      throw Error("Cannot verify staged attribute: " + name);
  }
}
#endif

struct Prepared {
  const PatchChange &change;
  const PatchFileOps &ops;
  std::unique_ptr<Stage> backup, staged;
  struct stat original {
  }, stage_inode{}, installed{};
  std::string after_hash;
  bool published = false, retired = false, installed_known = false;

  Prepared(const PatchChange &c, const PatchFileOps &o) : change(c), ops(o) {}
  void prepare(const Cancellation &cancel) {
    const auto &src = change.source, &dst = change.destination;
    if (!src.empty()) {
      original = inspect(src);
      if (!dst.empty() && (original.st_mode & 07000))
        throw Error("Cannot preserve special mode bits on an edited file: " + src.string());
#ifdef __APPLE__
      if (!dst.empty() && original.st_flags != 0)
        throw Error("Cannot preserve file flags on an edited file: " + src.string());
#endif
      backup = std::make_unique<Stage>(src.parent_path());
      // Keep the original inode, including the resource fork and ACL, for rollback.
      ops.link_exclusive(src, backup->file);
      const auto linked = inspect(backup->file);
      if (!same_version(original, linked, false))
        throw Error("File changed while preparing patch: " + src.string());
      original = linked;
      require_version(src, original, change.before_hash);
      // A private before-image must survive directory writeback before any name is retired.
      ops.sync_directory(backup->file.parent_path());
      ops.sync_directory(src.parent_path());
    }
    if (dst.empty())
      return;
    if (change.after.size() > size_t{16} * 1024 * 1024)
      throw Error("Write exceeds 16 MiB limit");
    create_parents(dst.parent_path(), ops);
    staged = std::make_unique<Stage>(dst.parent_path());
    // The stage directory is private. Let open apply the process umask to a new file.
    Descriptor out(::open(staged->file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));
    if (out.fd < 0)
      io_error("Create staged patch file", dst);
    write_bytes(out.fd, change.after.data(), change.after.size(), staged->file);
    if (backup) {
      Descriptor input(::open(backup->file.c_str(), O_RDONLY | O_NOFOLLOW));
      if (input.fd < 0)
        io_error("Open patch before-image", src);
      struct stat created {};
      if (::fstat(out.fd, &created) ||
          ((created.st_uid != original.st_uid || created.st_gid != original.st_gid) &&
           ::fchown(out.fd, original.st_uid, original.st_gid)) ||
          ::fchmod(out.fd, original.st_mode & 0777))
        io_error("Preserve patch permissions on", dst);
      ops.preserve_metadata(backup->file, input.fd, staged->file, out.fd, cancel);
#ifdef __APPLE__
      verify_metadata(backup->file, input.fd, staged->file, out.fd, cancel);
#endif
      if (::fstat(out.fd, &created) || created.st_uid != original.st_uid ||
          created.st_gid != original.st_gid ||
          (created.st_mode & 07777) != (original.st_mode & 07777))
        throw Error("Cannot verify staged patch permissions: " + dst.string());
#ifdef __APPLE__
      if (created.st_flags != original.st_flags)
        throw Error("Cannot verify staged patch flags: " + dst.string());
#endif
    }
    if (::fsync(out.fd))
      io_error("Sync staged patch file", dst);
    stage_inode = inspect(staged->file);
    after_hash = digest(change.after);
  }

  void commit(const Cancellation &cancel) {
    const auto &src = change.source, &dst = change.destination;
    if (backup)
      require_version(src, original, change.before_hash);
    if (staged) {
      if (src == dst)
        replace_verified(dst, *backup, original, change.before_hash, staged->file, ops);
      else
        ops.link_exclusive(staged->file, dst);
      published = true;
      if (src == dst)
        original = inspect(backup->file);
      installed = inspect(dst);
      if (!same_identity(installed, stage_inode))
        throw Error("Patch destination changed after publication: " + dst.string());
      installed_known = true;
      ops.sync_directory(dst.parent_path());
    }
    if (backup && src != dst) {
      cancel.check();
      if (published)
        require_version(dst, installed, after_hash);
      require_version(src, original, change.before_hash);
      retire_verified(src, *backup, original, change.before_hash, ops);
      retired = true;
      original = inspect(backup->file);
      ops.sync_directory(src.parent_path());
      if (published)
        require_version(dst, installed, after_hash);
    }
  }

  void rollback() {
    const auto &src = change.source, &dst = change.destination;
    if (retired) {
      require_version(backup->file, original, change.before_hash);
      ops.link_exclusive(backup->file, src);
      original = inspect(backup->file);
      ops.sync_directory(src.parent_path());
    }
    if (published) {
      if (!installed_known)
        throw Error("Cannot verify published patch destination: " + dst.string());
      require_version(dst, installed, after_hash);
      if (src == dst) {
        require_version(backup->file, original, change.before_hash);
        replace_verified(dst, *staged, installed, after_hash, backup->file, ops);
        ops.sync_directory(dst.parent_path());
        return;
      }
      if (backup)
        require_version(src, original, change.before_hash);
      retire_verified(dst, *staged, installed, after_hash, ops);
      try {
        ops.sync_directory(dst.parent_path());
        if (backup)
          require_version(src, original, change.before_hash);
      } catch (...) {
        // A competing change can remove the restored source during destination retirement.
        if (backup)
          staged->restore_retired(dst, ops);
        throw;
      }
    }
  }
};
} // namespace

void PatchFileOps::link_exclusive(const fs::path &from, const fs::path &to) const {
  if (::link(from.c_str(), to.c_str()))
    io_error("Cannot create exclusive patch link at", to);
}
void PatchFileOps::retire(const fs::path &from, const fs::path &private_name) const {
  if (::rename(from.c_str(), private_name.c_str()))
    io_error("Cannot retire patch file", from);
}
void PatchFileOps::sync_directory(const fs::path &path) const {
  Descriptor directory(::open(path.c_str(), O_RDONLY | O_NOFOLLOW));
  if (directory.fd < 0 || ::fsync(directory.fd))
    io_error("Sync patch directory", path);
}
void PatchFileOps::preserve_metadata(const fs::path &source, int source_fd, const fs::path &staged,
                                     int staged_fd, const Cancellation &cancel) const {
#ifdef __APPLE__
  // Apple's copyfile can silently skip some attributes. Copy and check each named value.
  const auto names = attr_names(source_fd, source);
  const auto inherited = attr_names(staged_fd, staged);
  for (const auto &name : inherited)
    if (!names.count(name) && ::fremovexattr(staged_fd, name.c_str(), 0))
      io_error("Remove inherited staged attribute", staged);
  for (const auto &name : names) {
    cancel.check();
    if (name == "com.apple.ResourceFork") {
      transfer_fork(source, staged, false, cancel);
    } else {
      const auto value = attribute(source_fd, name, source);
      if ((!inherited.count(name) || attribute(staged_fd, name, staged) != value) &&
          ::fsetxattr(staged_fd, name.c_str(), value.data(), value.size(), 0, 0))
        io_error("Copy patch attribute to", staged);
    }
  }
  const auto acl = acl_text(source_fd, source);
  if (acl_text(staged_fd, staged) != acl &&
      ::fcopyfile(source_fd, staged_fd, nullptr, COPYFILE_ACL))
    io_error("Copy patch ACL to", staged);
#else
  (void)source;
  (void)source_fd;
  (void)staged;
  (void)staged_fd;
  cancel.check();
#endif
}

void publish_native_patch(const std::vector<PatchChange> &changes, const Cancellation &cancel,
                          const PatchFileOps &ops) {
  std::vector<Prepared> prepared;
  prepared.reserve(changes.size());
  for (const auto &change : changes) {
    cancel.check();
    prepared.emplace_back(change, ops);
    prepared.back().prepare(cancel);
  }
  try {
    for (auto &operation : prepared) {
      cancel.check();
      operation.commit(cancel);
    }
  } catch (...) {
    const auto primary = std::current_exception();
    std::string failures;
    for (auto it = prepared.rbegin(); it != prepared.rend(); ++it) {
      try {
        it->rollback();
      } catch (const std::exception &error) {
        failures += "\n" + it->change.source.string() + " -> " + it->change.destination.string() +
                    ": " + error.what();
        if (it->backup) {
          it->backup->keep();
          failures += " (before-image: " + it->backup->file.string() + ")";
        }
      }
    }
    if (!failures.empty()) {
      try {
        std::rethrow_exception(primary);
      } catch (const std::exception &error) {
        throw Error(std::string(error.what()) + "\nPatch rollback needs attention:" + failures);
      }
    }
    std::rethrow_exception(primary);
  }
}

void FileSystem::publish_patch(const std::vector<PatchChange> &, const Cancellation &) {
  throw Error("This filesystem cannot publish patches safely");
}
void NativeFileSystem::publish_patch(const std::vector<PatchChange> &changes,
                                     const Cancellation &cancel) {
  publish_native_patch(changes, cancel, PatchFileOps{});
}
} // namespace codet
