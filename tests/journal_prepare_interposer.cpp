// Linked only into the host benchmark and deterministic publication-fault executable.
#include "journal_metrics.hpp"
#include <dlfcn.h>
extern "C" int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int bytes, sqlite3_stmt **statement,
                                  const char **tail) {
  using Prepare = int (*)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);
  static const auto prepare = reinterpret_cast<Prepare>(dlsym(RTLD_NEXT, "sqlite3_prepare_v2"));
  const auto status = prepare(db, sql, bytes, statement, tail);
  if (status == SQLITE_OK)
    codet::test::journal_metrics.prepared(*statement);
  return status;
}
