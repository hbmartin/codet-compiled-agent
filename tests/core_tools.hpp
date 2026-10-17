#pragma once
#include "codet/core.hpp"
namespace codet {
inline ToolResult execute_core(Tools &tools, Config &config, const ToolCall &call,
                               const Cancellation &cancel) {
  struct Restore {
    Config &config;
    std::string mode;
    ~Restore() {
      config.permission_mode = mode;
    }
  } restore{config, config.permission_mode};
  if (call.name == "read_file")
    config.permission_mode = "plan";
  auto controlled = call;
  if (controlled.name == "exec_command") {
    if (!controlled.arguments.contains("shell"))
      controlled.arguments["shell"] = "/bin/sh";
    if (!controlled.arguments.contains("login"))
      controlled.arguments["login"] = false;
  }
  auto result = tools.execute(controlled, cancel);
  if ((call.name == "exec_command" || call.name == "write_stdin") && !result.error)
    result.content = result.metadata.at("output");
  return result;
}
} // namespace codet
