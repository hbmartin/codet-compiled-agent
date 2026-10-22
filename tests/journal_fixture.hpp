#pragma once
#include "codet/core.hpp"
#include <sqlite3.h>

namespace codet::test {
inline void journal_history(StateStore &store, Session &session, sqlite3 *db, size_t calls,
                            const std::string &mode, size_t argument_bytes = 0,
                            bool invalid_arguments = false) {
  const auto first = session.messages.size();
  for (size_t i = 0; i < calls; ++i) {
    const auto id = "call-" + std::to_string(i);
    Message assistant;
    assistant.role = "assistant";
    Json arguments = {{"index", i}, {"payload", std::string(argument_bytes, 'x')}};
    if (invalid_arguments)
      arguments["invalid"] = std::string(1, static_cast<char>(0xff));
    assistant.calls = {{id, "exec_command", std::move(arguments)}};
    Message result;
    result.role = "tool";
    result.tool_call_id = id;
    result.text = "old-" + std::to_string(i);
    session.messages.push_back(std::move(assistant));
    session.messages.push_back(std::move(result));
  }
  store.save(session);
  if (mode == "none")
    return;
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(db,
                         "INSERT INTO tools(session,call_id,name,args,result,status) "
                         "VALUES(?,?,?,?,?,?)",
                         -1, &raw, nullptr) != SQLITE_OK)
    throw Error(sqlite3_errmsg(db));
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
  if (sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK)
    throw Error(sqlite3_errmsg(db));
  try {
    for (size_t i = 0; i < calls; ++i) {
      const auto &call = session.messages[first + i * 2].calls.front();
      const auto args = safe_json(call.arguments).dump();
      const auto result = safe_json(Json(session.messages[first + i * 2 + 1])).dump();
      const std::string status = mode == "complete" || (mode == "mixed" && i % 4 == 0) ? "complete"
                                 : i % 2                                               ? "uncertain"
                                         : "not_started";
      sqlite3_reset(raw);
      int binding = 1;
      for (const auto *value : {static_cast<const std::string *>(&session.id), &call.id, &call.name,
                                &args, &result, &status})
        if (sqlite3_bind_text(raw, binding++, value->data(), static_cast<int>(value->size()),
                              SQLITE_TRANSIENT) != SQLITE_OK)
          throw Error(sqlite3_errmsg(db));
      if (sqlite3_step(raw) != SQLITE_DONE)
        throw Error(sqlite3_errmsg(db));
    }
    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
      throw Error(sqlite3_errmsg(db));
  } catch (...) {
    (void)sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}
} // namespace codet::test
