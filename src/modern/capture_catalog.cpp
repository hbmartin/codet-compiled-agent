#include "codet/output.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace codet {
namespace {
struct File {
  int fd;
  explicit File(const fs::path &p, bool create = false) {
    fd = ::open(p.c_str(), O_RDWR | O_NOFOLLOW | O_NONBLOCK | (create ? O_CREAT : 0), 0600);
    struct stat st{};
    if (fd < 0 || ::fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
        ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
      if (fd >= 0)
        ::close(fd);
      throw Error("Capture catalog requires owned regular files: " + p.string());
    }
  }
  ~File() {
    ::close(fd);
  }
  File(const File &) = delete;
  File &operator=(const File &) = delete;
};
struct CatalogError : Error {
  int sqlite_code;
  CatalogError(sqlite3 *db, int status)
      : Error("Capture catalog: " + std::string(sqlite3_errmsg(db))), sqlite_code(status & 0xff) {}
};
void execute(sqlite3 *db, const char *sql) {
  const auto status = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
  if (status != SQLITE_OK)
    throw CatalogError(db, status);
}
struct CatalogTransaction {
  sqlite3 *db;
  bool active = true;
  explicit CatalogTransaction(sqlite3 *connection) : db(connection) {
    execute(db, "BEGIN IMMEDIATE");
  }
  ~CatalogTransaction() {
    if (active)
      (void)sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
  }
  void commit() {
    execute(db, "COMMIT");
    active = false;
  }
  CatalogTransaction(const CatalogTransaction &) = delete;
  CatalogTransaction &operator=(const CatalogTransaction &) = delete;
};
struct Query {
  sqlite3_stmt *s = nullptr;
  Query(sqlite3 *db, const char *sql) {
    const auto status = sqlite3_prepare_v2(db, sql, -1, &s, nullptr);
    if (status != SQLITE_OK)
      throw CatalogError(db, status);
  }
  ~Query() {
    sqlite3_finalize(s);
  }
  void text(int i, const std::string &v) {
    const auto bytes = static_cast<int>(v.size());
    // NOLINTNEXTLINE(performance-no-int-to-ptr): SQLite copies the bound bytes.
    const auto status = sqlite3_bind_text(s, i, v.c_str(), bytes, SQLITE_TRANSIENT);
    if (status != SQLITE_OK)
      throw CatalogError(sqlite3_db_handle(s), status);
  }
  void number(int i, sqlite3_int64 v) {
    const auto status = sqlite3_bind_int64(s, i, v);
    if (status != SQLITE_OK)
      throw CatalogError(sqlite3_db_handle(s), status);
  }
  int step() {
    const auto status = sqlite3_step(s);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
      throw CatalogError(sqlite3_db_handle(s), status);
    return status;
  }
  void run() {
    if (step() != SQLITE_DONE)
      throw Error("Capture catalog: " + std::string(sqlite3_errmsg(sqlite3_db_handle(s))));
  }
};
std::pair<int64_t, int64_t> modified(const struct stat &st) {
#ifdef __APPLE__
  return {st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec};
#else
  return {st.st_mtim.tv_sec, st.st_mtim.tv_nsec};
#endif
}
bool eligible(const fs::path &p) {
  const auto name = p.filename().string();
  return capture_name(p) || (name.rfind("capture-", 0) == 0 && p.extension() == ".txt");
}
void record(sqlite3 *db, const std::string &name, const struct stat &st, uint64_t bytes,
            bool active, bool readable) {
  Query q(db, "INSERT OR REPLACE INTO captures VALUES(?,?,?,?,?,?,?,?)");
  q.text(1, name);
  q.number(2, static_cast<sqlite3_int64>(st.st_dev));
  q.number(3, static_cast<sqlite3_int64>(st.st_ino));
  q.number(4, static_cast<sqlite3_int64>(bytes));
  const auto time = modified(st);
  q.number(5, time.first);
  q.number(6, time.second);
  q.number(7, active);
  q.number(8, readable);
  q.run();
}
// Capacity checks inspect catalogued active files, never enumerate the store.
void reconcile_active(sqlite3 *db, const fs::path &directory,
                      const std::function<void(const std::string &)> &warn) {
  struct Active {
    std::string name;
    sqlite3_int64 device, inode;
  };
  std::vector<Active> active;
  {
    Query rows(db, "SELECT name,device,inode FROM captures WHERE active=1");
    while (rows.step() == SQLITE_ROW) {
      const auto *name = sqlite3_column_text(rows.s, 0);
      active.push_back({name ? reinterpret_cast<const char *>(name) : "",
                        sqlite3_column_int64(rows.s, 1), sqlite3_column_int64(rows.s, 2)});
    }
  }
  for (const auto &row : active) {
    if (!eligible(row.name) || fs::path(row.name) != fs::path(row.name).filename())
      throw Error("Invalid active capture catalog entry");
    const auto path = directory / row.name;
    try {
      struct stat link{};
      if (::lstat(path.c_str(), &link)) {
        if (errno != ENOENT)
          throw Error("Cannot inspect active output capture");
        Query erase(db, "DELETE FROM captures WHERE name=?");
        erase.text(1, row.name);
        erase.run();
        continue;
      }
      File file(path);
      if (::flock(file.fd, LOCK_EX | LOCK_NB)) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          continue;
        throw Error("Cannot inspect active output ownership");
      }
      struct stat opened{}, current{};
      if (::fstat(file.fd, &opened) || ::lstat(path.c_str(), &current) ||
          opened.st_dev != static_cast<dev_t>(row.device) ||
          opened.st_ino != static_cast<ino_t>(row.inode) || opened.st_dev != current.st_dev ||
          opened.st_ino != current.st_ino || opened.st_size < 0)
        throw Error("Active capture identity changed");
      record(db, row.name, opened, static_cast<uint64_t>(opened.st_size), false, true);
    } catch (const CatalogError &) {
      throw;
    } catch (const Error &error) {
      warn(row.name + ": " + error.what());
    }
  }
}
} // namespace
struct CaptureStore::Catalog {
  sqlite3 *db = nullptr;
  std::pair<int64_t, int64_t> stamp{};
  dev_t device = 0;
  ino_t inode = 0;
  ~Catalog() {
    if (db)
      sqlite3_close(db);
  }
  void observe(const fs::path &directory) {
    struct stat st{};
    if (::lstat(directory.c_str(), &st) || !S_ISDIR(st.st_mode))
      throw Error("Cannot inspect capture directory");
    stamp = modified(st);
  }
};
CaptureStore::~CaptureStore() = default;
void CaptureStore::with_catalog(const std::function<void()> &action) {
  // Callers hold the store lock. Only catalog work is retried, never output writes.
  for (unsigned attempt = 0;; ++attempt) {
    try {
      prepare_catalog();
      action();
      return;
    } catch (const CatalogError &error) {
      if (attempt || (error.sqlite_code != SQLITE_CORRUPT && error.sqlite_code != SQLITE_NOTADB))
        throw;
      recover_catalog();
    }
  }
}
void CaptureStore::recover_catalog() {
  catalog_.reset();
  struct Checked {
    fs::path path;
    std::unique_ptr<File> file;
    struct stat identity{};
  };
  std::vector<Checked> checked;
  const auto path = directory_ / "catalog.sqlite3";
  // Validate every member before deleting any of them, including dangling symlinks.
  for (const auto *suffix : {"", "-wal", "-shm"}) {
    const fs::path member = path.string() + suffix;
    struct stat link{};
    if (::lstat(member.c_str(), &link)) {
      if (errno == ENOENT)
        continue;
      throw Error("Cannot inspect capture catalog recovery file");
    }
    auto file = std::make_unique<File>(member);
    struct stat identity{};
    if (::fstat(file->fd, &identity) || identity.st_dev != link.st_dev ||
        identity.st_ino != link.st_ino)
      throw Error("Capture catalog changed during recovery");
    checked.push_back({member, std::move(file), identity});
  }
  for (const auto &member : checked) {
    struct stat current{};
    if (::lstat(member.path.c_str(), &current) || current.st_dev != member.identity.st_dev ||
        current.st_ino != member.identity.st_ino)
      throw Error("Capture catalog changed during recovery");
    if (::unlink(member.path.c_str()))
      throw Error("Cannot remove corrupt capture catalog");
  }
}
void CaptureStore::prepare_catalog() {
  const auto path = directory_ / "catalog.sqlite3";
  if (catalog_) {
    struct stat link{};
    if (::lstat(path.c_str(), &link)) {
      if (errno != ENOENT)
        throw Error("Cannot inspect capture catalog identity");
      catalog_.reset();
    } else {
      if (!S_ISREG(link.st_mode) || link.st_uid != ::geteuid())
        throw Error("Capture catalog requires owned regular files: " + path.string());
      if (link.st_dev != catalog_->device || link.st_ino != catalog_->inode)
        catalog_.reset();
    }
  }
  if (!catalog_) {
    catalog_ = std::make_shared<Catalog>();
    for (const auto *suffix : {"", "-wal", "-shm"}) {
      const fs::path file = path.string() + suffix;
      if (fs::exists(file) || fs::is_symlink(file)) {
        File checked(file);
      }
    }
    struct stat identity{};
    {
      File checked(path, true);
      if (::fchmod(checked.fd, 0600))
        throw Error("Cannot protect capture catalog");
      if (::fstat(checked.fd, &identity))
        throw Error("Cannot inspect capture catalog identity");
    } // Raw descriptors must close before SQLite owns any locks on this inode.
    const auto status = sqlite3_open(path.c_str(), &catalog_->db);
    if (status != SQLITE_OK)
      throw CatalogError(catalog_->db, status);
    struct stat current{};
    if (::lstat(path.c_str(), &current) || !S_ISREG(current.st_mode) ||
        current.st_uid != ::geteuid() || current.st_dev != identity.st_dev ||
        current.st_ino != identity.st_ino) {
      catalog_.reset();
      throw Error("Capture catalog changed while opening");
    }
    sqlite3_busy_timeout(catalog_->db, 5000);
    execute(
        catalog_->db,
        "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;"
        "CREATE TABLE IF NOT EXISTS captures(name TEXT PRIMARY KEY,device INTEGER,inode INTEGER,"
        "bytes INTEGER NOT NULL CHECK(bytes>=0),seconds INTEGER,nanos INTEGER,active "
        "INTEGER,readable INTEGER);"
        "CREATE INDEX IF NOT EXISTS eviction ON captures(active,seconds,nanos,name)");
    catalog_->device = identity.st_dev;
    catalog_->inode = identity.st_ino;
    rebuild_catalog();
  } else {
    struct stat st{};
    if (::lstat(directory_.c_str(), &st) || !S_ISDIR(st.st_mode))
      throw Error("Capture directory changed");
    if (modified(st) != catalog_->stamp)
      rebuild_catalog();
  }
}
void CaptureStore::rebuild_catalog() {
  ++metrics.scans;
  CatalogTransaction transaction(catalog_->db);
  execute(catalog_->db, "DELETE FROM captures");
  for (const auto &entry : fs::directory_iterator(directory_)) {
    if (!eligible(entry.path()))
      continue;
    struct stat st{};
    if (::lstat(entry.path().c_str(), &st)) {
      warn("Cannot inspect " + entry.path().filename().string());
      continue;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
      warn("Ignoring nonregular entry " + entry.path().filename().string());
      continue;
    }
    bool active = false, readable = true;
    try {
      File file(entry.path());
      struct stat opened{};
      if (::fstat(file.fd, &opened) || opened.st_dev != st.st_dev || opened.st_ino != st.st_ino)
        throw Error("Capture changed during inspection");
      if (::fchmod(file.fd, 0600))
        throw Error("Cannot protect existing capture");
      if (::flock(file.fd, LOCK_EX | LOCK_NB)) {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
          throw Error("Cannot inspect output ownership");
        active = true;
      }
    } catch (const Error &e) {
      readable = false;
      warn(entry.path().filename().string() + ": " + e.what());
    }
    record(catalog_->db, entry.path().filename().string(), st, static_cast<uint64_t>(st.st_size),
           active, readable);
  }
  transaction.commit();
  catalog_->observe(directory_);
}
size_t CaptureStore::account_reservation(OutputSpool &spool, size_t requested) {
  auto *db = catalog_->db;
  struct stat own{};
  if (spool.fd_ >= 0) {
    if (::fstat(spool.fd_, &own))
      throw Error("Cannot inspect active capture");
    record(db, spool.path_.filename().string(), own, static_cast<uint64_t>(own.st_size), true,
           true);
  }
  bool pressure = false;
  {
    Query count(db, "SELECT count(*),coalesce(sum(bytes),0) FROM captures");
    if (count.step() != SQLITE_ROW)
      throw Error("Cannot inspect capture capacity");
    const auto total = static_cast<uint64_t>(sqlite3_column_int64(count.s, 1));
    pressure = sqlite3_column_int64(count.s, 0) > static_cast<sqlite3_int64>(records_) ||
               total > quota_ || requested > quota_ - std::min<uint64_t>(total, quota_);
  }
  if (pressure)
    reconcile_active(db, directory_, [&](const auto &message) { warn(message); });
  for (unsigned pass = 0; pass < 2; ++pass) {
    Query totals(db, "SELECT coalesce(sum(bytes),0),coalesce(sum(active=0),0) FROM captures");
    if (totals.step() != SQLITE_ROW)
      throw Error("Cannot total capture catalog");
    uint64_t total = static_cast<uint64_t>(sqlite3_column_int64(totals.s, 0));
    auto completed = sqlite3_column_int64(totals.s, 1);
    struct Candidate {
      std::string name;
      sqlite3_int64 device, inode, bytes;
    };
    std::vector<Candidate> candidates;
    const auto pressured = [&] {
      return completed > static_cast<int64_t>(records_) || total > quota_ ||
             requested > quota_ - total;
    };
    if (pressured()) {
      Query old(db, "SELECT name,device,inode,bytes FROM captures WHERE active=0 AND readable=1 "
                    "ORDER BY seconds,nanos,name");
      int status;
      while ((status = old.step()) == SQLITE_ROW) {
        const auto *name = sqlite3_column_text(old.s, 0);
        candidates.push_back({name ? reinterpret_cast<const char *>(name) : "",
                              sqlite3_column_int64(old.s, 1), sqlite3_column_int64(old.s, 2),
                              sqlite3_column_int64(old.s, 3)});
      }
      if (status != SQLITE_DONE)
        throw Error("Cannot read capture eviction catalog");
    }
    for (const auto &candidate : candidates) {
      if (!pressured())
        break;
      const auto &file_name = candidate.name;
      if (!eligible(file_name) || fs::path(file_name) != fs::path(file_name).filename())
        throw Error("Invalid capture catalog entry");
      const auto path = directory_ / file_name;
      try {
        File file(path);
        if (::flock(file.fd, LOCK_EX | LOCK_NB))
          continue;
        struct stat st{}, link{};
        if (::fstat(file.fd, &st) || ::lstat(path.c_str(), &link) ||
            st.st_dev != static_cast<dev_t>(candidate.device) ||
            st.st_ino != static_cast<ino_t>(candidate.inode) || st.st_dev != link.st_dev ||
            st.st_ino != link.st_ino || st.st_size != candidate.bytes)
          continue;
        if (::unlink(path.c_str()))
          throw Error("Cannot evict completed output capture");
        total -= static_cast<uint64_t>(st.st_size);
        --completed;
        Query erase(db, "DELETE FROM captures WHERE name=?");
        erase.text(1, file_name);
        erase.run();
      } catch (const CatalogError &) {
        throw;
      } catch (const Error &e) {
        warn(file_name + ": " + e.what());
      }
    }
    if (completed <= static_cast<int64_t>(records_) && total <= quota_) {
      const auto keep = static_cast<size_t>(std::min<uint64_t>(requested, quota_ - total));
      if (keep == requested || pass) {
        Query reserve(db, "UPDATE captures SET bytes=bytes+? WHERE name=?");
        reserve.number(1, static_cast<sqlite3_int64>(keep));
        reserve.text(2, spool.path_.filename().string());
        reserve.run();
        catalog_->observe(directory_);
        return keep;
      }
    }
    // Reclaim conservative reservations left by dead writers only when capacity is needed.
    rebuild_catalog();
  }
  catalog_->observe(directory_);
  return 0;
}
void CaptureStore::account_closed(OutputSpool &spool) {
  File lock(directory_ / ".capture.lock", true);
  if (::flock(lock.fd, LOCK_EX))
    throw Error("Cannot lock output store");
  with_catalog([&] {
    struct stat st{};
    if (spool.bytes_ && ::lstat(spool.path_.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      record(catalog_->db, spool.path_.filename().string(), st, static_cast<uint64_t>(st.st_size),
             false, true);
    else {
      Query erase(catalog_->db, "DELETE FROM captures WHERE name=?");
      erase.text(1, spool.path_.filename().string());
      erase.run();
    }
    (void)account_reservation(spool, 0);
    catalog_->observe(directory_);
  });
}
} // namespace codet
