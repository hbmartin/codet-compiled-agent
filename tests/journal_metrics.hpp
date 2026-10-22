#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <sqlite3.h>

namespace codet::test {
// Host-only SQLite interception; no instrumentation is linked into the application.
struct JournalMetrics {
  bool enabled = false;
  size_t queries = 0, rows = 0, vm_steps = 0, fullscan_steps = 0, max_bindings = 0;
  static bool candidate(sqlite3_stmt *statement) {
    const auto *sql = sqlite3_sql(statement);
    constexpr char single[] = "SELECT status,name,args FROM tools WHERE session=? AND call_id=?";
    constexpr char batch[] =
        "SELECT call_id,status,name,args FROM tools WHERE session=? AND call_id IN (";
    return std::strncmp(sql, single, sizeof(single) - 1) == 0 ||
           std::strncmp(sql, batch, sizeof(batch) - 1) == 0;
  }
  void before(sqlite3_stmt *statement) {
    if (enabled && candidate(statement) && !sqlite3_stmt_busy(statement)) {
      ++queries;
      max_bindings =
          std::max(max_bindings, static_cast<size_t>(sqlite3_bind_parameter_count(statement)));
    }
  }
  void after(sqlite3_stmt *statement, int result) {
    if (!enabled || !candidate(statement))
      return;
    rows += result == SQLITE_ROW ? 1u : 0u;
    vm_steps += static_cast<size_t>(sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 1));
    fullscan_steps +=
        static_cast<size_t>(sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_FULLSCAN_STEP, 1));
  }
};
inline JournalMetrics journal_metrics;
} // namespace codet::test
