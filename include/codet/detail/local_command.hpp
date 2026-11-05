#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>
namespace codet::detail {
inline bool is_quit_command(std::string_view text) noexcept {
  return text == "/quit" || text == "/exit";
}
enum class LocalCommand : uint8_t {
  none,
  quit,
  help,
  model,
  status,
  compact,
  permissions,
  sessions,
  new_session,
  name,
  export_session,
  memory
};
inline bool command_prefix(std::string_view text, std::string_view prefix) noexcept {
  return text.size() >= prefix.size() &&
         std::memcmp(text.data(), prefix.data(), prefix.size()) == 0;
}
// A slash alone or a path is prose, not a syntactic custom command.
inline std::string_view custom_command_name(std::string_view text) noexcept {
  if (text.empty() || text.front() != '/')
    return {};
  const auto end = text.find(' ');
  const std::string_view name(text.data() + 1,
                              end == std::string_view::npos ? text.size() - 1 : end - 1);
  for (char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_'))
      return {};
  return name;
}
inline LocalCommand local_command(std::string_view text) noexcept {
  if (is_quit_command(text))
    return LocalCommand::quit;
  if (text == "/help")
    return LocalCommand::help;
  if (text == "/model" || command_prefix(text, "/model "))
    return LocalCommand::model;
  if (text == "/status")
    return LocalCommand::status;
  if (text == "/compact")
    return LocalCommand::compact;
  if (text == "/permissions" || command_prefix(text, "/permissions "))
    return LocalCommand::permissions;
  if (text == "/sessions")
    return LocalCommand::sessions;
  if (text == "/new")
    return LocalCommand::new_session;
  if (command_prefix(text, "/name "))
    return LocalCommand::name;
  if (text == "/export")
    return LocalCommand::export_session;
  if (text == "/memory consolidate")
    return LocalCommand::memory;
  return LocalCommand::none;
}
} // namespace codet::detail
