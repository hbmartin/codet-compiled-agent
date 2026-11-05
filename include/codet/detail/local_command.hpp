#pragma once
#include <string_view>
namespace codet::detail {
inline bool is_quit_command(std::string_view text) noexcept {
  return text == "/quit" || text == "/exit";
}
enum class LocalCommand {
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
inline LocalCommand local_command(std::string_view text) noexcept {
  if (is_quit_command(text))
    return LocalCommand::quit;
  if (text == "/help")
    return LocalCommand::help;
  if (text == "/model" || text.substr(0, 7) == "/model ")
    return LocalCommand::model;
  if (text == "/status")
    return LocalCommand::status;
  if (text == "/compact")
    return LocalCommand::compact;
  if (text == "/permissions" || text.substr(0, 13) == "/permissions ")
    return LocalCommand::permissions;
  if (text == "/sessions")
    return LocalCommand::sessions;
  if (text == "/new")
    return LocalCommand::new_session;
  if (text.substr(0, 6) == "/name ")
    return LocalCommand::name;
  if (text == "/export")
    return LocalCommand::export_session;
  if (text == "/memory consolidate")
    return LocalCommand::memory;
  return LocalCommand::none;
}
} // namespace codet::detail
