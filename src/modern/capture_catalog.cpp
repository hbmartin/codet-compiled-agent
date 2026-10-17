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
    struct stat st {};
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
};
void execute(sqlite3 *db, const char *sql) {
  if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
    throw Error("Capture catalog: " + std::string(sqlite3_errmsg(db)));
}
struct Query {
  sqlite3_stmt *s = nullptr;
  Query(sqlite3 *db, const char *sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK)
      throw Error("Capture catalog: " + std::string(sqlite3_errmsg(db)));
  }
  ~Query() {
    sqlite3_finalize(s);
  }
  void text(int i, const std::string &v) {
    // NOLINTNEXTLINE(performance-no-int-to-ptr): SQLite copies the bound bytes.
    if (sqlite3_bind_text(s, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK)
      throw Error("Capture catalog bind failed");
  }
  void number(int i, sqlite3_int64 v) {
    if (sqlite3_bind_int64(s, i, v) != SQLITE_OK)
      throw Error("Capture catalog bind failed");
  }
  void run() {
    if (sqlite3_step(s) != SQLITE_DONE)
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
    struct stat st {};
    if (::lstat(directory.c_str(), &st) || !S_ISDIR(st.st_mode))
      throw Error("Cannot inspect capture directory");
    stamp = modified(st);
  }
};
CaptureStore::~CaptureStore() = default;
void CaptureStore::prepare_catalog() {
  if (catalog_) {
    File file(directory_ / "catalog.sqlite3");
    struct stat st {};
    if (::fstat(file.fd, &st))
      throw Error("Cannot inspect capture catalog identity");
    if (st.st_dev != catalog_->device || st.st_ino != catalog_->inode)
      catalog_.reset();
  }
  if (!catalog_) {
    catalog_ = std::make_shared<Catalog>();
    const auto path = directory_ / "catalog.sqlite3";
    for (const auto *suffix : {"", "-wal", "-shm"}) {
      const fs::path file = path.string() + suffix;
      if (fs::exists(file) || fs::is_symlink(file)) {
        File checked(file);
      }
    }
    File checked(path, true);
    if (::fchmod(checked.fd, 0600))
      throw Error("Cannot protect capture catalog");
    if (sqlite3_open(path.c_str(), &catalog_->db) != SQLITE_OK)
      throw Error("Cannot open capture catalog");
    sqlite3_busy_timeout(catalog_->db, 5000);
    try {
      execute(
          catalog_->db,
          "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;"
          "CREATE TABLE IF NOT EXISTS captures(name TEXT PRIMARY KEY,device INTEGER,inode INTEGER,"
          "bytes INTEGER NOT NULL CHECK(bytes>=0),seconds INTEGER,nanos INTEGER,active "
          "INTEGER,readable INTEGER);"
          "CREATE INDEX IF NOT EXISTS eviction ON captures(active,seconds,nanos,name)");
    } catch (...) {
      const auto code = sqlite3_errcode(catalog_->db);
      catalog_.reset();
      if (code != SQLITE_CORRUPT && code != SQLITE_NOTADB)
        throw;
      for (const auto *suffix : {"", "-wal", "-shm"}) {
        const fs::path file = path.string() + suffix;
        if (fs::exists(file)) {
          File check(file);
          fs::remove(file);
        }
      }
      prepare_catalog();
      return;
    }
    struct stat identity {};
    if (::fstat(checked.fd, &identity))
      throw Error("Cannot inspect capture catalog identity");
    catalog_->device = identity.st_dev;
    catalog_->inode = identity.st_ino;
    rebuild_catalog();
  } else {
    struct stat st {};
    if (::lstat(directory_.c_str(), &st) || !S_ISDIR(st.st_mode))
      throw Error("Capture directory changed");
    if (modified(st) != catalog_->stamp)
      rebuild_catalog();
  }
}
void CaptureStore::rebuild_catalog() {
  ++metrics.scans;
  execute(catalog_->db, "BEGIN IMMEDIATE; DELETE FROM captures");
  try {
    for (const auto &entry : fs::directory_iterator(directory_)) {
      if (!eligible(entry.path()))
        continue;
      struct stat st {};
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
        struct stat opened {};
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
    execute(catalog_->db, "COMMIT");
    catalog_->observe(directory_);
  } catch (...) {
    (void)sqlite3_exec(catalog_->db, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}
size_t CaptureStore::reserve(OutputSpool &spool, size_t requested) {
  try {
    return account_reservation(spool, requested);
  } catch (const Error &) {
    const auto code = sqlite3_errcode(catalog_->db);
    if (code != SQLITE_CORRUPT && code != SQLITE_NOTADB)
      throw;
    catalog_.reset();
    prepare_catalog();
    return account_reservation(spool, requested);
  }
}
size_t CaptureStore::account_reservation(OutputSpool &spool, size_t requested) {
  auto *db = catalog_->db;
  struct stat own {};
  if (spool.fd_ >= 0) {
    if (::fstat(spool.fd_, &own))
      throw Error("Cannot inspect active capture");
    record(db, spool.path_.filename().string(), own, static_cast<uint64_t>(own.st_size), true,
           true);
  }
  for (unsigned pass = 0; pass < 2; ++pass) {
    Query totals(db, "SELECT coalesce(sum(bytes),0),coalesce(sum(active=0),0) FROM captures");
    if (sqlite3_step(totals.s) != SQLITE_ROW)
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
      while ((status = sqlite3_step(old.s)) == SQLITE_ROW) {
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
        struct stat st {
        }, link{};
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
  prepare_catalog();
  bool crowded = false;
  {
    Query count(catalog_->db, "SELECT count(*) FROM captures");
    if (sqlite3_step(count.s) != SQLITE_ROW)
      throw Error("Cannot count capture catalog");
    crowded = sqlite3_column_int64(count.s, 0) > static_cast<sqlite3_int64>(records_);
  }
  // An abandoned active flag must not bypass completed-file capacity after its owner dies.
  if (crowded)
    rebuild_catalog();

  struct stat st {};
  if (spool.bytes_ && ::lstat(spool.path_.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    record(catalog_->db, spool.path_.filename().string(), st, static_cast<uint64_t>(st.st_size),
           false, true);
  else {
    Query erase(catalog_->db, "DELETE FROM captures WHERE name=?");
    erase.text(1, spool.path_.filename().string());
    erase.run();
  }
  (void)reserve(spool, 0);
  catalog_->observe(directory_);
}
} // namespace codet
