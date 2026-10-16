#pragma once
#include "codet/core.hpp"
#include <cstdint>

namespace codet {
enum class ToolHandler : std::uint8_t {
  exec,
  stdin_write,
  patch,
  read,
  image,
  plan,
  question,
  async_question,
  message,
  time,
  sleep,
  computer
};
struct ToolDefinition {
  std::string name, description;
  Json schema;
  ToolHandler handler = ToolHandler::read;
  bool mutation = false;
  std::string availability, hook_name, event_type = "codet.tool";
};
const std::vector<ToolDefinition> &tool_registry();
const ToolDefinition *tool_definition(const std::string &);
std::string tool_wire_name(std::string);
std::string tool_logical_name(const std::string &);
void validate_tool_rules(const std::vector<std::string> &);
void validate_hook_matchers(const Json &);
} // namespace codet
