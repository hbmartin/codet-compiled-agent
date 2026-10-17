#include "codet/output.hpp"
#include "temp.hpp"
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/stat.h>
using namespace codet;
using namespace testing;
namespace {
struct Database {
  sqlite3 *db = nullptr;
  explicit Database(const fs::path &path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
      throw Error("Cannot open test catalog");
  }
  ~Database() {
    sqlite3_close(db);
  }
  void exec(const char *sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
      throw Error(sqlite3_errmsg(db));
  }
  int integer(const char *sql) {
    sqlite3_stmt *query = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &query, nullptr) != SQLITE_OK)
      throw Error(sqlite3_errmsg(db));
    const int status = sqlite3_step(query);
    const int value = status == SQLITE_ROW ? sqlite3_column_int(query, 0) : -1;
    sqlite3_finalize(query);
    if (status != SQLITE_ROW)
      throw Error("Test catalog query failed");
    return value;
  }
};
struct CatalogRecovery : Test {
  TempDirectory temp;
  fs::path dir = temp.root / "captures";
  std::shared_ptr<CaptureStore> store = std::make_shared<CaptureStore>(dir);
  fs::path capture(const std::string &text) {
    OutputSpool spool(store);
    spool.append(text);
    spool.close();
    EXPECT_TRUE(spool.error().empty()) << spool.error();
    EXPECT_EQ(spool.bytes(), text.size());
    return spool.path();
  }
  void corrupt_data_page() {
    int page = 0, size = 0;
    {
      Database db(dir / "catalog.sqlite3");
      page = db.integer("SELECT rootpage FROM sqlite_master WHERE name='captures'");
      size = db.integer("PRAGMA page_size");
      db.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    }
    std::fstream bytes(dir / "catalog.sqlite3", std::ios::in | std::ios::out | std::ios::binary);
    bytes.seekp((page - 1) * size);
    bytes.put(char(0xff));
    ASSERT_TRUE(bytes.good());
  }
};
TEST_F(CatalogRecovery, MissingCatalogAndRemovedDirectoryRecoverInSameStore) {
  capture("first");
  ASSERT_TRUE(fs::remove(dir / "catalog.sqlite3"));
  capture("after catalog removal");
  fs::remove_all(dir);
  capture("after directory removal");
}
TEST_F(CatalogRecovery, ReplacedCatalogIsReconciled) {
  const auto retained = capture("retained");
  fs::rename(dir / "catalog.sqlite3", dir / "replaced.sqlite3");
  capture("after replacement");
  EXPECT_TRUE(fs::exists(retained));
  Database db(dir / "catalog.sqlite3");
  EXPECT_EQ(db.integer("SELECT count(*) FROM captures"), 2);
}
TEST_F(CatalogRecovery, DataPageCorruptionRebuildsWithoutLosingCaptureBytes) {
  const auto retained = capture("retained bytes");
  corrupt_data_page();
  store = std::make_shared<CaptureStore>(dir);
  capture("after corruption");
  NativeFileSystem files;
  EXPECT_EQ(files.read(retained, 1024), "retained bytes");
  EXPECT_EQ(store->metrics.scans, 2u); // Failed rebuild, then one clean rebuild.
  capture("later output");
}
TEST_F(CatalogRecovery, HeaderCorruptionRebuildsOnCloseAccounting) {
  OutputSpool active(store);
  active.append("one copy");
  active.flush();
  ASSERT_TRUE(active.error().empty());
  // Replace the catalog while retaining the active capture's descriptor and bytes.
  fs::rename(dir / "catalog.sqlite3", dir / "old.sqlite3");
  NativeFileSystem files;
  files.atomic_write(dir / "catalog.sqlite3", "not a database", true);
  active.close();
  EXPECT_TRUE(active.error().empty()) << active.error();
  EXPECT_EQ(files.read(active.path(), 1024), "one copy");
  Database db(dir / "catalog.sqlite3");
  EXPECT_EQ(db.integer("SELECT sum(bytes) FROM captures"), 8);
  EXPECT_EQ(db.integer("SELECT sum(active) FROM captures"), 0);
}
TEST_F(CatalogRecovery, FailedRebuildRollsBackAndNonCorruptionErrorsAreNotRecovered) {
  capture("retained");
  Database other(dir / "catalog.sqlite3");
  other.exec("CREATE TRIGGER fail_delete BEFORE DELETE ON captures "
             "BEGIN SELECT RAISE(ABORT,'injected delete failure'); END");
  // Reopening forces a rebuild even on shared filesystems with coarse directory timestamps.
  store = std::make_shared<CaptureStore>(dir);
  OutputSpool failed(store);
  failed.append("new");
  failed.flush();
  EXPECT_THAT(failed.error(), HasSubstr("injected delete failure"));
  EXPECT_EQ(sqlite3_exec(other.db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr), SQLITE_OK);
  other.exec("ROLLBACK; DROP TRIGGER fail_delete");
  EXPECT_EQ(other.integer("SELECT count(*) FROM captures"), 1);
  capture("after rollback");
}
TEST_F(CatalogRecovery, UnsafeCatalogAndSidecarsAreNeverRemoved) {
  capture("retained");
  for (const auto *suffix : {"", "-wal", "-shm"}) {
    store.reset();
    const auto member = fs::path((dir / "catalog.sqlite3").string() + suffix);
    std::error_code error;
    fs::remove(member, error);
    // A dangling link must fail the same ownership checks as a live link.
    fs::create_symlink(temp.root / "absent", member);
    store = std::make_shared<CaptureStore>(dir);
    OutputSpool failed(store);
    failed.append("new");
    failed.flush();
    EXPECT_FALSE(failed.error().empty());
    EXPECT_TRUE(fs::is_symlink(member));
    fs::remove(member);
    store.reset();
    store = std::make_shared<CaptureStore>(dir);
    capture("healthy");
  }
}
// Fail real SQLite opens through a temporary VFS; no production test hooks are needed.
sqlite3_vfs *underlying_vfs = nullptr;
int injected_status = SQLITE_OK, opened_catalogs = 0;
int failing_open(sqlite3_vfs *, const char *name, sqlite3_file *file, int flags, int *out) {
  if (flags & SQLITE_OPEN_MAIN_DB) {
    ++opened_catalogs;
    file->pMethods = nullptr;
    return injected_status;
  }
  return underlying_vfs->xOpen(underlying_vfs, name, file, flags, out);
}
struct FailingVfs {
  sqlite3_vfs replacement;
  explicit FailingVfs(int status) : replacement(*sqlite3_vfs_find(nullptr)) {
    underlying_vfs = sqlite3_vfs_find(nullptr);
    injected_status = status;
    opened_catalogs = 0;
    replacement.zName = "codet-catalog-failure";
    replacement.xOpen = failing_open;
    if (sqlite3_vfs_register(&replacement, 1) != SQLITE_OK)
      throw Error("Cannot install SQLite test VFS");
  }
  ~FailingVfs() {
    sqlite3_vfs_register(underlying_vfs, 1);
    sqlite3_vfs_unregister(&replacement);
  }
};
TEST_F(CatalogRecovery, RepeatedCorruptionHasOneRecoveryAttempt) {
  FailingVfs injection(SQLITE_CORRUPT);
  OutputSpool failed(store);
  failed.append("new");
  failed.flush();
  EXPECT_FALSE(failed.error().empty());
  EXPECT_EQ(failed.bytes(), 0u);
  EXPECT_EQ(opened_catalogs, 2);
  failed.close();
  EXPECT_EQ(opened_catalogs, 4); // Closing is a separate bounded accounting operation.
}
TEST_F(CatalogRecovery, IoErrorsDoNotDeleteOrRetryTheCatalog) {
  FailingVfs injection(SQLITE_IOERR);
  OutputSpool failed(store);
  failed.append("new");
  failed.flush();
  EXPECT_FALSE(failed.error().empty());
  EXPECT_EQ(opened_catalogs, 1);
  EXPECT_TRUE(fs::exists(dir / "catalog.sqlite3"));
}
} // namespace
