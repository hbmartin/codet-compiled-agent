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
