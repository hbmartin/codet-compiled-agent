void rebuild() {
  // ruleid: codet-catalog-split-transaction-start, codet-catalog-guarded-transaction
  execute(db, "BEGIN IMMEDIATE; DELETE FROM captures");
  // ruleid: codet-catalog-guarded-transaction
  execute(db, "BEGIN IMMEDIATE");
  // ok: codet-catalog-split-transaction-start, codet-catalog-guarded-transaction
  execute(db, "DELETE FROM captures");
}
struct CatalogTransaction {
  explicit CatalogTransaction(sqlite3 *connection) : db(connection) {
    // ok: codet-catalog-split-transaction-start, codet-catalog-guarded-transaction
    execute(db, "BEGIN IMMEDIATE");
  }
};

void CaptureStore::prepare_catalog() {
  if (catalog_) {
    // ruleid: codet-catalog-raw-live-identity
    File file(path);
    inspect(file.fd);
  }
  // ok: codet-catalog-raw-live-identity
  if (catalog_)
    lstat(path.c_str(), &identity);
  // ok: codet-catalog-descriptor-before-sqlite
  {
    File checked(path, true);
    fstat(checked.fd, &identity);
  }
  sqlite3_open(path.c_str(), &db);
}
void bad_open() {
  {
    // ruleid: codet-catalog-descriptor-before-sqlite
    File checked(path, true);
    sqlite3_open(path.c_str(), &db);
  }
}
void output_file() {
  // ok: codet-catalog-descriptor-before-sqlite
  File capture(output_path);
  sqlite3_open(catalog_path.c_str(), &db);
}
