#include "codet/core.hpp"
#include "codet/tool_registry.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <toml++/toml.hpp>
#include <unistd.h>

namespace codet {
namespace {
Json parse_toml(const std::string &text) {
  try {
    auto table = toml::parse(text);
    std::ostringstream s;
    s << toml::json_formatter{table};
    return Json::parse(s.str());
  } catch (const std::exception &e) {
    throw Error(std::string("Invalid TOML: ") + e.what(), 2);
  }
}
void merge(Json &into, const Json &from) {
  for (auto it = from.begin(); it != from.end(); ++it) {
    if (it.value().is_object() && into.contains(it.key()) && into[it.key()].is_object())
      merge(into[it.key()], it.value());
    else
      into[it.key()] = it.value();
  }
}
Json layer(Json value) {
  if (value.contains("model_provider") && !value.contains("provider"))
    value["provider"] = value["model_provider"];
  value.erase("model_provider");
  return value;
}
template <class T> T setting(const Json &data, const char *name, const T &fallback = {}) {
  try {
    return data.value(name, fallback);
  } catch (const Json::exception &error) {
    throw Error(std::string("Invalid configuration type for ") + name + ": " + error.what(), 2);
  }
}
template <class T>
T limit(const Json &data, const char *name, T fallback, uint64_t minimum, uint64_t maximum) {
  const auto value = data.find(name);
  if (value == data.end())
    return fallback;
  if (!value->is_number_integer())
    throw Error(std::string("Expected an integer for ") + name, 2);
  if (!value->is_number_unsigned() && value->get<int64_t>() < 0)
    throw Error(std::string("Configuration limit outside supported range: ") + name, 2);
  const auto number = value->get<uint64_t>();
  if (number < minimum || number > maximum)
    throw Error(std::string("Configuration limit outside supported range: ") + name, 2);
  return static_cast<T>(number);
}
std::string expand_env(std::string text) {
  size_t pos = 0;
  while ((pos = text.find("{env:", pos)) != std::string::npos) {
    const auto end = text.find('}', pos);
    if (end == std::string::npos)
      throw Error("Unclosed environment substitution", 2);
    std::string replacement = env(text.substr(pos + 5, end - pos - 5).c_str());
    text.replace(pos, end - pos + 1, replacement);
    pos += replacement.size();
  }
  return text;
}
Json jwt_payload(const std::string &token) {
  const auto a = token.find('.');
  if (a == std::string::npos)
    return Json::object();
  const auto b = token.find('.', a + 1);
  if (b == std::string::npos)
    return Json::object();
  std::string raw;
  unsigned bits = 0;
  int count = 0;
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  for (char ch : token.substr(a + 1, b - a - 1)) {
    if (ch == '=')
      break;
    const auto at = alphabet.find(ch);
    if (at == std::string::npos)
      return Json::object();
    bits = (bits << 6) | static_cast<unsigned>(at);
    count += 6;
    if (count >= 8) {
      count -= 8;
      raw += static_cast<char>((bits >> count) & 255);
    }
  }
  Json j = Json::parse(raw, nullptr, false);
  return j.is_object() ? j : Json::object();
}
bool valid_secret(const std::string &s) {
  return !s.empty() && s.size() < 65536 && s.find_first_of("\r\n") == std::string::npos;
}
void secure_home(const fs::path &home) {
  if (fs::is_symlink(home))
    throw Error("Credential directory must not be a symlink");
  fs::create_directories(home);
  if (::chmod(home.c_str(), 0700) != 0)
    throw Error("Cannot protect credential directory");
}
class CredentialLock {
  int fd_;

public:
  explicit CredentialLock(const fs::path &home) {
    secure_home(home);
    fd_ = ::open((home / "auth.lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd_ < 0)
      throw Error("Cannot open credential lock");
  }
  CredentialLock(const CredentialLock &) = delete;
  CredentialLock &operator=(const CredentialLock &) = delete;
  ~CredentialLock() {
    ::close(fd_);
  }
  void acquire(Clock &clock, const Cancellation &cancel) const {
    struct stat info {};
    if (::fstat(fd_, &info) || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() ||
        ::fcntl(fd_, F_SETFD, FD_CLOEXEC) < 0 || ::fchmod(fd_, 0600))
      throw Error("Cannot protect private credential lock");
    for (;;) {
      cancel.check();
      if (::flock(fd_, LOCK_EX | LOCK_NB) == 0)
        return;
      const int error = errno;
      if (error == EINTR)
        continue;
      if (error != EWOULDBLOCK && error != EAGAIN)
        throw Error("Cannot acquire credential lock: " + std::string(std::strerror(error)));
      clock.sleep_ms(50, cancel);
    }
  }
};
} // namespace
Config load_config(const fs::path &home, const fs::path &cwd, const std::string &profile,
                   const std::vector<std::string> &overrides, FileSystem &files) {
  Config c;
  c.home = files.canonical(home);
  c.cwd = files.canonical(cwd);
  c.roots = {c.cwd};
  Json data = Json::object();
  Json selection = Json::object();
  const auto config_file = c.home / "config.toml";
  if (fs::exists(config_file))
    data = layer(parse_toml(files.read(config_file, size_t{1024} * 1024)));
  Json defaults = data;
  if (!profile.empty()) {
    if (profile.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
        std::string::npos)
      throw Error("Invalid profile name", 2);
    selection =
        layer(parse_toml(files.read(c.home / (profile + ".config.toml"), size_t{1024} * 1024)));
    merge(data, selection);
  }
  fs::path trusted_root;
  for (const auto &p : setting<std::vector<std::string>>(data, "trusted_projects")) {
    const auto root = files.canonical(p);
    if (within(c.cwd, root)) {
      c.trust_project = true;
      if (root.string().size() > trusted_root.string().size())
        trusted_root = root;
    }
  }
  if (c.trust_project) {
    std::vector<fs::path> parents;
    for (auto p = c.cwd; !p.empty(); p = p.parent_path()) {
      parents.push_back(p);
      if (p == trusted_root || p == p.root_path() || fs::exists(p / ".git"))
        break;
    }
    for (auto it = parents.rbegin(); it != parents.rend(); ++it)
      if (fs::exists(*it / ".codet/config.toml")) {
        const auto project =
            layer(parse_toml(files.read(*it / ".codet/config.toml", size_t{1024} * 1024)));
        merge(data, project);
        merge(defaults, project);
      }
  }
  for (const auto &s : overrides) {
    const auto eq = s.find('=');
    if (eq == std::string::npos || eq == 0)
      throw Error("Configuration overrides require key=value", 2);
    Json value;
    try {
      value = parse_toml(s);
    } catch (const Error &) {
      value = parse_toml(s.substr(0, eq) + "=" + Json(s.substr(eq + 1)).dump());
    }
    value = layer(std::move(value));
    if (value.contains("trusted_projects"))
      (void)setting<std::vector<std::string>>(value, "trusted_projects");
    merge(data, value);
    merge(selection, value);
  }
  if (selection.contains("provider"))
    c.selected_provider = setting<std::string>(selection, "provider");
  if (selection.contains("model"))
    c.selected_model = setting<std::string>(selection, "model");
  c.configured = {setting(defaults, "provider", c.provider),
                  setting<std::string>(defaults, "model")};
  c.provider = c.configured.provider;
  c.permission_mode = setting(data, "permission_mode", c.permission_mode);
  c.approval = setting(data, "approval_policy", c.approval);
  c.ui = setting(data, "ui", c.ui);
  c.theme = setting(data, "theme", c.theme);
  c.durability = setting(data, "durability", c.durability);
  c.effort = setting<std::string>(data, "model_reasoning_effort");
  if (selection.contains("model_reasoning_effort"))
    c.selected_effort = c.effort;
  c.max_turns = limit(data, "max_turns", c.max_turns, 1, 1000);
  c.max_output_tokens = limit(data, "max_output_tokens", c.max_output_tokens, 1, 1000000);
  c.context_tokens = limit(data, "context_tokens", c.context_tokens, 512, 2000000);
  c.tool_output_limit =
      limit(data, "tool_output_limit", c.tool_output_limit, 256, 16 * 1024 * 1024);
  c.request_timeout = limit(data, "request_timeout_seconds", c.request_timeout, 0, 3600);
  c.connect_timeout = limit(data, "connect_timeout_seconds", c.connect_timeout, 1, 3600);
  c.stream_idle_timeout =
      limit(data, "stream_idle_timeout_seconds", c.stream_idle_timeout, 1, 3600);
  c.project_doc_max_bytes =
      limit(data, "project_doc_max_bytes", c.project_doc_max_bytes, 0, 1024 * 1024);
  c.memory_context_max_bytes =
      limit(data, "memory_context_max_bytes", c.memory_context_max_bytes, 0, 1024 * 1024);
  c.set_allow(setting(data, "allow", c.rules().allow));
  c.set_deny(setting(data, "deny", c.rules().deny));
  c.hooks = data.value("hooks", Json::object());
  validate_hook_matchers(c.hooks);
  c.keybindings = data.value("keybindings", Json::object());
  if (!c.keybindings.is_object())
    throw Error("keybindings must be a table", 2);
  if (data.contains("providers") && !data.at("providers").is_object())
    throw Error("providers must be a table", 2);
  if (data.contains("providers"))
    for (auto it = data["providers"].begin(); it != data["providers"].end(); ++it) {
      if (!it.value().is_object())
        throw Error("providers." + it.key() + " must be a table", 2);
      ProviderConfig p;
      p.api_key = expand_env(setting<std::string>(it.value(), "api_key"));
      p.base_url = setting<std::string>(it.value(), "base_url");
      p.model = setting<std::string>(it.value(), "model");
      p.wire_api = setting<std::string>(it.value(), "wire_api", "auto");
      if (p.wire_api != "auto" && p.wire_api != "responses" && p.wire_api != "chat_completions")
        throw Error("wire_api must be auto, responses, or chat_completions", 2);
      if (it.value().contains("image_input"))
        p.image_input = setting<bool>(it.value(), "image_input");
      c.providers[it.key()] = p;
    }
  for (const auto &p : setting<std::vector<std::string>>(data, "add_dirs"))
    c.roots.push_back(files.canonical(c.cwd / p));
  if (c.permission_mode != "default" && c.permission_mode != "plan" &&
      c.permission_mode != "accept-edits" && c.permission_mode != "bypass")
    throw Error("Invalid permission_mode", 2);
  if (c.approval != "on-request" && c.approval != "never")
    throw Error("Invalid approval_policy", 2);
  if (c.durability != "durable" && c.durability != "fast")
    throw Error("durability must be durable or fast", 2);
  for (const auto &provider : {c.provider, c.selected_provider.value_or(c.provider)})
    if (provider != "codex" && provider != "openai" && provider != "anthropic" &&
        provider != "google" && provider != "ollama")
      throw Error("Unsupported provider: " + provider, 2);
  const auto resolved = resolve_selection(c);
  c.provider = resolved.provider;
  c.model = resolved.model;
  validate_tool_rules(c.rules().allow);
  validate_tool_rules(c.rules().deny);
  return c;
}
bool image_input_enabled(const Config &c) {
  const auto p = c.providers.find(c.provider);
  if (p != c.providers.end())
    return p->second.image_input.value_or(c.provider != "ollama");
  return c.provider != "ollama";
}
std::string default_model(const Config &c, const std::string &provider) {
  if (c.configured.provider == provider && !c.configured.model.empty())
    return c.configured.model;
  const auto found = c.providers.find(provider);
  if (found != c.providers.end() && !found->second.model.empty())
    return found->second.model;
  return provider == "anthropic" ? "claude-sonnet-5-5"
         : provider == "google"  ? "gemini-3.8-flash"
         : provider == "ollama"  ? "qwen3-coder"
         : provider == "codex"   ? "gpt-5.5"
                                 : "gpt-6.1-sol";
}
Selection resolve_selection(const Config &c, const std::optional<Selection> &saved) {
  const auto previous = saved && !saved->provider.empty() ? saved->provider : c.configured.provider;
  Selection resolved;
  resolved.provider = c.selected_provider.value_or(previous);
  if (c.selected_model)
    resolved.model =
        c.selected_model->empty() ? default_model(c, resolved.provider) : *c.selected_model;
  else if (saved && resolved.provider == previous && !saved->model.empty())
    resolved.model = saved->model;
  else
    resolved.model = default_model(c, resolved.provider);
  return resolved;
}
Json CredentialStore::load() const {
  if (!fs::exists(home_ / "auth.json"))
    return Json::object();
  const Json j = Json::parse(files_.read(home_ / "auth.json", size_t{1024} * 1024), nullptr, false);
  if (!j.is_object())
    throw Error("Invalid Codet credential store");
  return j;
}
void CredentialStore::import(const fs::path &source, const Cancellation &cancel) {
  cancel.check();
  const Json input = Json::parse(files_.read(source, size_t{1024} * 1024), nullptr, false);
  if (!input.is_object())
    throw Error("Credential import requires a JSON object", 2);
  Json tokens = input.contains("tokens") ? input["tokens"] : input;
  if (!tokens.is_object() || !tokens.contains("access_token") ||
      !tokens["access_token"].is_string() ||
      !valid_secret(tokens["access_token"].get<std::string>()))
    throw Error("Import requires a valid access_token", 2);
  Json clean = {{"access_token", tokens["access_token"]}};
  for (const auto *key : {"refresh_token", "account_id"})
    if (tokens.contains(key) && !tokens[key].is_null()) {
      if (!tokens[key].is_string() || !valid_secret(tokens[key].get<std::string>()))
        throw Error(std::string("Invalid ") + key, 2);
      clean[key] = tokens[key];
    }
  const auto claims = jwt_payload(clean["access_token"].get<std::string>());
  if (claims.contains("exp") && claims["exp"].is_number_integer())
    clean["expires_at"] = claims["exp"];
  else if (tokens.contains("expires_at") && tokens["expires_at"].is_number_integer())
    clean["expires_at"] = tokens["expires_at"];
  if (!clean.contains("account_id") && claims.contains("https://api.openai.com/auth")) {
    const auto &identity = claims["https://api.openai.com/auth"];
    if (identity.is_object() && identity.contains("chatgpt_account_id") &&
        identity["chatgpt_account_id"].is_string())
      clean["account_id"] = identity["chatgpt_account_id"];
  }
  CredentialLock lock(home_);
  lock.acquire(clock_, cancel);
  Json store = load();
  store["codex"] = clean;
  cancel.check();
  files_.atomic_write(home_ / "auth.json", store.dump(2) + "\n", true);
}
void CredentialStore::save_key(const std::string &provider, const std::string &key,
                               const Cancellation &cancel) {
  cancel.check();
  if (!valid_secret(key))
    throw Error("API key must be a nonempty single line", 2);
  CredentialLock lock(home_);
  lock.acquire(clock_, cancel);
  Json store = load();
  store["api_keys"][provider] = key;
  cancel.check();
  files_.atomic_write(home_ / "auth.json", store.dump(2) + "\n", true);
}
std::string CredentialStore::key(const std::string &p) const {
  auto j = load();
  if (j.contains("api_keys") && j["api_keys"].contains(p))
    return j["api_keys"][p].get<std::string>();
  return "";
}
Json CredentialStore::codex(HttpTransport &http, const Cancellation &cancel) {
  CredentialLock lock(home_);
  lock.acquire(clock_, cancel);
  auto store = load();
  if (!store.contains("codex"))
    throw Error("No imported ChatGPT credentials. Use codet login --import FILE, or select an "
                "API-key provider.");
  Json tokens = store["codex"];
  if (tokens.value("expires_at", int64_t{0}) != 0 &&
      tokens.value("expires_at", int64_t{0}) <= clock_.seconds() + 300) {
    const auto refresh = tokens.value("refresh_token", "");
    if (refresh.empty())
      throw Error("Imported credential expired; import fresh credentials");
    HttpRequest r;
    r.url = "https://auth.openai.com/oauth/token";
    r.headers = {{"Content-Type", "application/json"}};
    r.body = Json({{"grant_type", "refresh_token"},
                   {"refresh_token", refresh},
                   {"client_id", "app_EMoamEEZ73f0CkXaXp7hrann"}})
                 .dump();
    const auto response = http.perform(r, cancel);
    if (response.status != 200)
      throw Error("Credential refresh failed (HTTP " + std::to_string(response.status) +
                  "); import fresh credentials");
    auto fresh = Json::parse(response.body, nullptr, false);
    if (!fresh.is_object() || !fresh.contains("access_token") ||
        !fresh["access_token"].is_string() ||
        !valid_secret(fresh["access_token"].get<std::string>()))
      throw Error("Malformed credential refresh response");
    tokens["access_token"] = fresh["access_token"];
    if (fresh.contains("refresh_token") && fresh["refresh_token"].is_string() &&
        valid_secret(fresh["refresh_token"].get<std::string>()))
      tokens["refresh_token"] = fresh["refresh_token"];
    auto claims = jwt_payload(tokens["access_token"].get<std::string>());
    tokens["expires_at"] =
        claims.value("exp", clock_.seconds() + fresh.value("expires_in", int64_t{3600}));
    store["codex"] = tokens;
    files_.atomic_write(home_ / "auth.json", store.dump(2) + "\n", true);
  }
  return tokens;
}
} // namespace codet
