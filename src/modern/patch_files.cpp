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
#include <openssl/evp.h>
#include <sys/acl.h>
#endif
#if defined(__APPLE__) || defined(__linux__)
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

// Attribute names are nonempty. The empty key records the ACL independently of them.
using PatchMetadata = std::map<std::string, std::string>;
PatchMetadata read_metadata(const fs::path &path, const Cancellation &cancel);

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

struct FileVersion {
  struct stat status {};
  std::string hash;
  PatchMetadata metadata;

  // link and rename may advance ctime. Only accept the new timestamp once the
  // identity, other stat fields, contents and independent metadata still match.
  struct stat verify(const fs::path &path, const Cancellation &cancel,
                     bool namespace_changed) const {
    const auto observed = inspect(path);
    if (!same_version(status, observed, !namespace_changed) ||
        digest(NativeFileSystem{}.read(path)) != hash ||
        (namespace_changed && read_metadata(path, cancel) != metadata) ||
        !same_version(inspect(path), observed))
      throw Error("File changed during patch: " + path.string());
    return observed;
  }
  void verify(const fs::path &path, const Cancellation &cancel) const {
    (void)verify(path, cancel, false);
  }
  void record_namespace_change(const fs::path &path, const Cancellation &cancel) {
    status = verify(path, cancel, true);
  }
};

using SourceVersions = std::map<std::pair<dev_t, ino_t>, FileVersion>;

[[noreturn]] void rollback_error(const std::exception_ptr &primary, const std::string &failure) {
  try {
    std::rethrow_exception(primary);
  } catch (const Error &error) {
    throw Error(std::string(error.what()) + "\nPatch rollback needs attention:" + failure,
                error.code);
  } catch (const std::exception &error) {
    throw Error(std::string(error.what()) + "\nPatch rollback needs attention:" + failure);
  }
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
  void restore_retired(const fs::path &path, const PatchFileOps &ops,
                       const std::exception_ptr &primary, FileVersion *known = nullptr) {
    try {
      ops.link_exclusive(retired, path);
      if (known)
        known->record_namespace_change(path, Cancellation{});
      ops.sync_directory(path.parent_path());
    } catch (const std::exception &error) {
      keep();
      rollback_error(primary, "\nCould not restore " + path.string() + "; retained at " +
                                  retired.string() + ": " + error.what());
    }
  }
};

// POSIX has no conditional unlink. Move the public name into a private directory, then
// verify what actually moved before permitting cleanup. If another writer won the race,
// restore that exact inode without overwriting a new occupant.
void retire_verified(const fs::path &path, Stage &stage, FileVersion &expected,
                     const PatchFileOps &ops, const Cancellation &cancel) {
  // A directory cannot replace a regular file by rename. With an empty slot
  // the rename could instead take an entire competing directory off its public path.
  Descriptor placeholder(::open(stage.retired.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600));
  if (placeholder.fd < 0)
    io_error("Reserve patch retirement slot", stage.retired);
  ops.retire(path, stage.retired);
  try {
    expected.record_namespace_change(stage.retired, cancel);
  } catch (const Error &error) {
    stage.restore_retired(path, ops, std::current_exception(),
                          error.code == 130 ? &expected : nullptr);
    throw Error("Cannot retire " + path.string() + ": " + error.what(), error.code);
  } catch (...) {
    stage.restore_retired(path, ops, std::current_exception());
    throw;
  }
}

// The checked pathname can change before rename. Capture its occupant in private storage,
// then claim the public name exclusively. Apply the same ordering when undoing an Update.
void replace_verified(const fs::path &path, Stage &displaced, FileVersion &expected,
                      const fs::path &replacement, const PatchFileOps &ops,
                      const Cancellation &cancel) {
  retire_verified(path, displaced, expected, ops, cancel);
  try {
    ops.link_exclusive(replacement, path);
  } catch (...) {
    displaced.restore_retired(path, ops, std::current_exception(), &expected);
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
  if (n > ssize_t{1024} * 1024)
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
  if (n > size_t{1024} * 1024)
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
template <class Consume>
void read_fork(const fs::path &source, const Cancellation &cancel, Consume consume) {
  const auto from = source / "..namedfork/rsrc";
  Descriptor input(::open(from.c_str(), O_RDONLY));
  if (input.fd < 0)
    io_error("Open patch resource fork", from);
  std::array<char, 32768> data{};
  for (;;) {
    cancel.check();
    const auto n = ::read(input.fd, data.data(), data.size());
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      io_error("Read patch resource fork", from);
    if (!n)
      return;
    consume(data.data(), static_cast<size_t>(n));
  }
}

void copy_fork(const fs::path &source, const fs::path &target, const Cancellation &cancel) {
  const auto to = target / "..namedfork/rsrc";
  Descriptor output(::open(to.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0600));
  if (output.fd < 0)
    io_error("Open staged resource fork", to);
  read_fork(source, cancel,
            [&](const char *data, size_t size) { write_bytes(output.fd, data, size, to); });
  if (::fsync(output.fd))
    io_error("Sync staged resource fork", to);
}

std::string fork_digest(const fs::path &source, const Cancellation &cancel) {
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
    throw Error("Cannot initialize resource fork digest: " + source.string());
  read_fork(source, cancel, [&](const char *data, size_t size) {
    if (EVP_DigestUpdate(ctx.get(), data, size) != 1)
      throw Error("Cannot hash resource fork: " + source.string());
  });
  std::array<unsigned char, EVP_MAX_MD_SIZE> sum{};
  unsigned int length = 0;
  if (EVP_DigestFinal_ex(ctx.get(), sum.data(), &length) != 1)
    throw Error("Cannot finish resource fork digest: " + source.string());
  return {reinterpret_cast<const char *>(sum.data()), length};
}
#elif defined(__linux__)
constexpr char access_acl[] = "system.posix_acl_access";

std::optional<std::string> linux_acl(int fd, const fs::path &path) {
  const auto size = ::fgetxattr(fd, access_acl, nullptr, 0);
  if (size < 0) {
    if (errno == ENODATA || errno == EOPNOTSUPP)
      return std::nullopt;
    io_error("Inspect patch access ACL on", path);
  }
  std::string value(static_cast<size_t>(size), '\0');
  if (::fgetxattr(fd, access_acl, value.data(), value.size()) != size)
    throw Error("Cannot read patch access ACL: " + path.string());
  return value;
}
#endif

PatchMetadata read_metadata(const fs::path &path, int fd, const Cancellation &cancel) {
  cancel.check();
  PatchMetadata result;
#ifdef __APPLE__
  result.emplace("", acl_text(fd, path));
  for (const auto &name : attr_names(fd, path)) {
    cancel.check();
    result.emplace(name, name == "com.apple.ResourceFork" ? fork_digest(path, cancel)
                                                          : digest(attribute(fd, name, path)));
  }
#elif defined(__linux__)
  if (auto acl = linux_acl(fd, path))
    result.emplace("", std::move(*acl));
#else
  (void)path;
  (void)fd;
#endif
  return result;
}

PatchMetadata read_metadata(const fs::path &path, const Cancellation &cancel) {
  Descriptor input(::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW));
  if (input.fd < 0)
    io_error("Open patch file metadata in", path);
  return read_metadata(path, input.fd, cancel);
}

struct Prepared {
  const PatchChange &change;
  const PatchFileOps &ops;
  std::unique_ptr<Stage> backup, staged;
  FileVersion *original = nullptr;
  FileVersion replacement;
  bool published = false, retired = false, unreadable_add = false;

  Prepared(const PatchChange &c, const PatchFileOps &o) : change(c), ops(o) {}
  void prepare(SourceVersions &sources, const Cancellation &cancel) {
    const auto &src = change.source, &dst = change.destination;
    if (!src.empty()) {
      const auto status = inspect(src);
      auto [it, inserted] = sources.try_emplace({status.st_dev, status.st_ino});
      original = &it->second;
      if (inserted)
        *original = {status, change.before_hash, read_metadata(src, cancel)};
      if (change.before_hash != original->hash)
        throw Error("File changed while preparing patch: " + src.string());
      original->verify(src, cancel);
      if (!dst.empty() && (status.st_mode & 07000))
        throw Error("Cannot preserve special mode bits on an edited file: " + src.string());
#ifdef __APPLE__
      if (!dst.empty() && status.st_flags != 0)
        throw Error("Cannot preserve file flags on an edited file: " + src.string());
#endif
      backup = std::make_unique<Stage>(src.parent_path());
      // Keep the original inode, including the resource fork and ACL, for rollback.
      ops.link_exclusive(src, backup->file);
      original->record_namespace_change(backup->file, cancel);
      original->verify(src, cancel);
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
      const auto &status = original->status;
      Descriptor input(::open(backup->file.c_str(), O_RDONLY | O_NOFOLLOW));
      if (input.fd < 0)
        io_error("Open patch before-image", src);
      struct stat created {};
      if (::fstat(out.fd, &created) ||
          ((created.st_uid != status.st_uid || created.st_gid != status.st_gid) &&
           ::fchown(out.fd, status.st_uid, status.st_gid)) ||
          ::fchmod(out.fd, status.st_mode & 0777))
        io_error("Preserve patch permissions on", dst);
      ops.preserve_metadata(backup->file, input.fd, staged->file, out.fd, cancel);
      if (::fstat(out.fd, &created) || created.st_uid != status.st_uid ||
          created.st_gid != status.st_gid || (created.st_mode & 07777) != (status.st_mode & 07777))
        throw Error("Cannot verify staged patch permissions: " + dst.string());
#ifdef __APPLE__
      if (created.st_flags != status.st_flags)
        throw Error("Cannot verify staged patch flags: " + dst.string());
#endif
    }
    if (::fsync(out.fd))
      io_error("Sync staged patch file", dst);
    replacement.status = inspect(staged->file);
    replacement.hash = digest(change.after);
    if (replacement.status.st_size != static_cast<off_t>(change.after.size()))
      throw Error("Cannot verify staged patch size: " + dst.string());
    if (!original && ::access(staged->file.c_str(), R_OK) != 0) {
      if (errno != EACCES)
        io_error("Inspect staged patch access on", dst);
      // An Add can be write-only from birth. Its exclusive link is safe, but
      // a later rollback cannot prove its contents and metadata are still ours.
      unreadable_add = true;
    } else {
      replacement.metadata = read_metadata(staged->file, out.fd, cancel);
      if (original && replacement.metadata != original->metadata)
        throw Error("Cannot verify staged patch metadata: " + dst.string());
    }
  }

  void commit(const Cancellation &cancel) {
    const auto &src = change.source, &dst = change.destination;
    if (backup)
      original->verify(src, cancel);
    if (staged) {
      if (src == dst)
        replace_verified(dst, *backup, *original, staged->file, ops, cancel);
      else
        ops.link_exclusive(staged->file, dst);
      published = true;
      if (unreadable_add) {
        if (!same_version(replacement.status, inspect(dst), false))
          throw Error("Patch destination changed after publication: " + dst.string());
      } else {
        replacement.record_namespace_change(dst, Cancellation{});
      }
      ops.sync_directory(dst.parent_path());
    }
    if (backup && src != dst) {
      cancel.check();
      if (published)
        replacement.verify(dst, cancel);
      original->verify(src, cancel);
      retire_verified(src, *backup, *original, ops, cancel);
      retired = true;
      ops.sync_directory(src.parent_path());
      if (published)
        replacement.verify(dst, cancel);
    }
  }

  void rollback() {
    const auto &src = change.source, &dst = change.destination;
    const Cancellation undo;
    if (retired) {
      original->verify(backup->file, undo);
      ops.link_exclusive(backup->file, src);
      original->record_namespace_change(src, undo);
      ops.sync_directory(src.parent_path());
    }
    if (published) {
      if (unreadable_add)
        throw Error("Cannot verify unreadable patch addition for rollback: " + dst.string());
      replacement.verify(dst, undo);
      if (src == dst) {
        original->verify(backup->file, undo);
        replace_verified(dst, *staged, replacement, backup->file, ops, undo);
        original->record_namespace_change(dst, undo);
        ops.sync_directory(dst.parent_path());
        return;
      }
      if (backup)
        original->verify(src, undo);
      retire_verified(dst, *staged, replacement, ops, undo);
      try {
        ops.sync_directory(dst.parent_path());
        if (backup)
          original->verify(src, undo);
      } catch (...) {
        // A competing change can remove the restored source during destination retirement.
        if (backup)
          staged->restore_retired(dst, ops, std::current_exception(), &replacement);
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
      copy_fork(source, staged, cancel);
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
#elif defined(__linux__)
  cancel.check();
  const auto source_acl = linux_acl(source_fd, source);
  const auto staged_acl = linux_acl(staged_fd, staged);
  if (source_acl != staged_acl) {
    if (source_acl) {
      if (::fsetxattr(staged_fd, access_acl, source_acl->data(), source_acl->size(), 0))
        io_error("Copy patch access ACL to", staged);
    } else if (::fremovexattr(staged_fd, access_acl)) {
      io_error("Remove inherited patch access ACL from", staged);
    }
  }
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
  // Each operation owns its recovery names, but multiple names can address one
  // source inode. Share its verified ctime through forward and reverse operations.
  SourceVersions sources;
  std::vector<Prepared> prepared;
  prepared.reserve(changes.size());
  for (const auto &change : changes) {
    cancel.check();
    prepared.emplace_back(change, ops);
    prepared.back().prepare(sources, cancel);
  }
  try {
    for (auto &operation : prepared) {
      cancel.check();
      operation.commit(cancel);
    }
    cancel.check();
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
    if (!failures.empty())
      rollback_error(primary, failures);
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
