#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/interaction.hpp"
#include "codet/tool_registry.hpp"
#include <algorithm>
#include <curses.h>
#include <cwchar>
#include <future>
#include <histedit.h>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/stat.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace codet {
namespace {
const char *help = R"(codet — a C++17 terminal coding agent
Usage: codet [OPTIONS] [PROMPT]
       codet exec [OPTIONS] [PROMPT|-]
       codet [exec] resume [SESSION] [--last] [PROMPT]
       codet [exec] fork [SESSION] [--last] [PROMPT]
       codet login --import FILE | --with-api-key | status
       codet sessions | export SESSION | doctor
       codet memory consolidate

  -m, --model MODEL             Select model
      --provider NAME           codex, openai, anthropic, google, ollama
  -C, --cd DIR                  Working directory
  -p, --profile NAME            Overlay NAME.config.toml
  -c, --config KEY=VALUE        Repeatable TOML override
      --json                    JSONL thread/turn/item events
      --output-format FORMAT    text, json, jsonl
      --output-schema FILE      Draft 7 schema for final structured answer
  -o, --output-last-message FILE Save final answer independently
      --permission-mode MODE    default, plan, accept-edits, bypass
  -a, --ask-for-approval POLICY  on-request, never
      --allow RULE / --deny RULE Tool or Tool(target-glob) rule (repeatable)
      --add-dir DIR             Additional authorized root
      --ui MODE                 auto, curses, line
      --max-turns N             Model request limit
      --ephemeral               Temporary sessions and automatic output captures
      --last                    Select most recent session
      --color auto|always|never  Terminal color policy
      --version / --help

State: CODET_HOME or ~/.codet. Fresh state; no implicit Codex/Claurst imports.
Application permission rules are active; OS shell sandboxing is unavailable.
Native Tiger computer use and image inputs are capability-gated.
Native sign-in, multiagent, goals, MCP, ACP, LSP and plugins are deferred.
)";
struct Options {
  bool exec = false, last = false, json = false, ephemeral = false, key = false;
  bool help = false, version = false;
  std::string command, action, session, profile, provider, model, format = "text", ui, mode,
                                                                  approval, color = "auto";
  fs::path cwd = fs::current_path(), schema, output, import;
  std::vector<std::string> overrides, allow, deny;
  std::vector<fs::path> roots;
  std::vector<std::string> positionals;
};
std::string stdin_text() {
  std::ostringstream out;
  char buffer[8192];
  size_t n = 0;
  while (std::cin) {
    std::cin.read(buffer, sizeof buffer);
    const auto count = std::cin.gcount();
    n += static_cast<size_t>(count);
    if (n > size_t{1024} * 1024)
      throw Error("stdin exceeds 1 MiB", 2);
    out.write(buffer, count);
  }
  return out.str();
}
std::string combined(const std::vector<std::string> &words) {
  std::string s;
  for (const auto &word : words) {
    if (!s.empty())
      s += ' ';
    s += word;
  }
  return s;
}
Options parse(const std::vector<std::string> &args) {
  Options o;
  bool literal = false;
  std::exception_ptr argument_error;
  for (size_t i = 0; i < args.size(); ++i) {
    try {
      std::string a = args[i];
      if (!literal && a == "--") {
        literal = true;
        continue;
      }
      if (!literal && a.size() > 1 && a[0] == '-') {
        const auto eq = a.find('=');
        std::optional<std::string> attached;
        if (eq != std::string::npos) {
          attached = a.substr(eq + 1);
          a.resize(eq);
        }
        auto value = [&]() {
          if (attached)
            return *attached;
          if (++i >= args.size())
            throw Error("Missing value for " + a, 2);
          return args[i];
        };
        if (a == "--help" || a == "-h")
          o.help = true;
        else if (a == "--version")
          o.version = true;
        else if (a == "--json") {
          o.format = "jsonl";
          o.json = true;
        } else if (a == "--last")
          o.last = true;
        else if (a == "--ephemeral")
          o.ephemeral = true;
        else if (a == "--with-api-key")
          o.key = true;
        else if (a == "--model" || a == "-m")
          o.model = value();
        else if (a == "--provider")
          o.provider = value();
        else if (a == "--cd" || a == "-C")
          o.cwd = fs::absolute(value());
        else if (a == "--profile" || a == "-p")
          o.profile = value();
        else if (a == "--config" || a == "-c")
          o.overrides.push_back(value());
        else if (a == "--allow")
          o.allow.push_back(value());
        else if (a == "--deny")
          o.deny.push_back(value());
        else if (a == "--add-dir")
          o.roots.push_back(value());
        else if (a == "--output-format")
          o.format = value();
        else if (a == "--output-schema")
          o.schema = value();
        else if (a == "--output-last-message" || a == "-o")
          o.output = value();
        else if (a == "--import")
          o.import = value();
        else if (a == "--ui")
          o.ui = value();
        else if (a == "--permission-mode")
          o.mode = value();
        else if (a == "--ask-for-approval" || a == "-a")
          o.approval = value();
        else if (a == "--color")
          o.color = value();
        else if (a == "--max-turns")
          o.overrides.push_back("max_turns=" + value());
        else if (a == "--skip-git-repo-check") {
        } // Codet does not require a Git checkout.
        else if (a == "--sandbox" || a == "-s")
          throw Error("OS-enforced sandbox modes are unavailable; use documented application "
                      "permission rules",
                      2);
        else
          throw Error("Unknown option: " + a, 2);
        continue;
      }
      if (!literal && o.positionals.empty() && o.command.empty() && (a == "exec" || a == "e")) {
        o.exec = true;
        o.command = "exec";
        continue;
      }
      if (!literal && o.positionals.empty() &&
          (o.command.empty() || (o.command == "exec" && (a == "resume" || a == "fork"))) &&
          (a == "resume" || a == "fork" || a == "login" || a == "sessions" || a == "export" ||
           a == "doctor" || a == "memory")) {
        o.command = a;
        continue;
      }
      o.positionals.push_back(a);
    } catch (const std::exception &) {
      if (!argument_error)
        argument_error = std::current_exception();
    }
  }
  if (o.help || o.version)
    return o;
  if (argument_error)
    std::rethrow_exception(argument_error);
  if (o.format != "text" && o.format != "json" && o.format != "jsonl")
    throw Error("Invalid output format", 2);
  if (o.json && o.format != "jsonl")
    throw Error("--json conflicts with --output-format", 2);
  if (o.color != "auto" && o.color != "always" && o.color != "never")
    throw Error("Invalid color policy", 2);
  if (o.command == "resume" || o.command == "fork" || o.command == "export") {
    if (!o.last && !o.positionals.empty()) {
      o.session = o.positionals.front();
      o.positionals.erase(o.positionals.begin());
    }
  }
  if (o.command == "login" || o.command == "memory") {
    if (!o.positionals.empty()) {
      o.action = o.positionals.front();
      o.positionals.erase(o.positionals.begin());
    }
  }
  return o;
}
class Terminal {
  bool curses_ = false;
  EditLine *edit_ = nullptr;
  History *history_ = nullptr;
  HistEvent history_event_{};
  std::vector<std::string> transcript_;
  std::string input_, status_, progress_;
  std::string prompt_;
  std::map<std::string, int> bindings_{{"submit", 10}, {"quit", 4}, {"backspace", 127}};
  struct termios original_{};
  bool restore_ = false;
  const Cancellation *read_cancel_ = nullptr;
  static int read_character(EditLine *edit, wchar_t *character) {
    void *data = nullptr;
    el_get(edit, EL_CLIENTDATA, &data);
    auto *terminal = static_cast<Terminal *>(data);
    std::mbstate_t state{};
    for (;;) {
      if (terminal && terminal->read_cancel_ && terminal->read_cancel_->requested)
        return 0;
      struct pollfd descriptor{STDIN_FILENO, POLLIN, 0};
      const int ready = ::poll(&descriptor, 1, 100);
      if (ready < 0) {
        if (errno == EINTR)
          continue;
        return -1;
      }
      if (ready == 0)
        continue;
      char byte = 0;
      const auto n = ::read(STDIN_FILENO, &byte, 1);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        return static_cast<int>(n);
      const auto converted = std::mbrtowc(character, &byte, 1, &state);
      if (converted == static_cast<size_t>(-2))
        continue;
      if (converted == static_cast<size_t>(-1))
        *character = L'\ufffd';
      return 1;
    }
  }
  void render() {
    if (!curses_)
      return;
    erase();
    int rows = 0, cols = 0;
    getmaxyx(stdscr, rows, cols);
    if (rows < 4 || cols < 8) {
      refresh();
      return;
    }
    const auto heading = status_ + (progress_.empty() ? "" : " | " + progress_);
    mvaddnstr(0, 0, heading.c_str(), cols - 1);
    int y = rows - 3;
    for (auto it = transcript_.rbegin(); it != transcript_.rend() && y > 0; ++it, --y)
      mvaddnstr(y, 0, it->c_str(), cols - 1);
    mvaddnstr(rows - 1, 0, ("codet> " + input_).c_str(), cols - 1);
    refresh();
  }

public:
  Terminal(const Config &c, const std::string &color) {
    restore_ = tcgetattr(STDIN_FILENO, &original_) == 0;
    curses_ =
        (c.ui == "curses" || (c.ui == "auto" && env("TERM") != "dumb" && !env("TERM").empty()));
    for (auto it = c.keybindings.begin(); it != c.keybindings.end(); ++it) {
      if (!bindings_.count(it.key()) || !it.value().is_number_integer())
        throw Error("keybindings support integer submit/quit/backspace codes", 2);
      bindings_[it.key()] = it.value();
    }
    if (curses_) {
      if (!initscr())
        throw Error("Cannot initialize terminal");
      cbreak();
      noecho();
      keypad(stdscr, TRUE);
      timeout(100);
      if (has_colors() && color != "never") {
        start_color();
        use_default_colors();
        init_pair(1, c.theme == "light" ? COLOR_BLUE : COLOR_CYAN, -1);
        attron(COLOR_PAIR(1));
      }
      render();
    } else {
      edit_ = el_init("codet", stdin, stdout, stderr);
      if (!edit_)
        throw Error("Cannot initialize line editor");
      el_set(
          edit_, EL_PROMPT, +[](EditLine *edit) -> const char * {
            void *data = nullptr;
            el_get(edit, EL_CLIENTDATA, &data);
            return static_cast<Terminal *>(data)->prompt_.c_str();
          });
      el_set(edit_, EL_EDITOR, "emacs");
      el_set(edit_, EL_SIGNAL, 0);
      el_set(edit_, EL_CLIENTDATA, this);
      el_set(edit_, EL_GETCFN, &Terminal::read_character);
      history_ = history_init();
      if (history_) {
        history(history_, &history_event_, H_SETSIZE, 500);
        el_set(edit_, EL_HIST, history, history_);
      }
      set_mode(c.permission_mode);
      std::cerr << status_ << '\n';
    }
  }
  ~Terminal() {
    if (curses_)
      endwin();
    if (edit_)
      el_end(edit_);
    if (history_)
      history_end(history_);
    if (restore_)
      (void)tcsetattr(STDIN_FILENO, TCSANOW, &original_);
  }
  bool fullscreen() const {
    return curses_;
  }
  void diagnostic(const std::string &text) {
    if (curses_)
      say(text);
    else
      std::cerr << text << '\n';
  }
  void set_mode(const std::string &mode) {
    status_ = "Permissions: " + mode + "; no OS shell sandbox";
    prompt_ = "codet> [" + mode + "] ";
    if (curses_)
      render();
  }
  void say(const std::string &text) {
    if (curses_) {
      for (const auto &l : split(text))
        transcript_.push_back(l);
      if (transcript_.size() > 1000)
        transcript_.erase(transcript_.begin(),
                          transcript_.begin() + static_cast<long>(transcript_.size() - 1000));
      render();
    } else
      std::cout << text << '\n';
  }
  static std::vector<std::string> split(const std::string &text) {
    std::vector<std::string> ls;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
      ls.push_back(line);
    return ls;
  }
  std::optional<std::string> read(Cancellation &cancel) {
    if (!curses_) {
      read_cancel_ = &cancel;
      el_set(edit_, EL_PREP_TERM, 1);
      int count = 0;
      const char *text = el_gets(edit_, &count);
      if (!text || count <= 0)
        return std::nullopt;
      std::string line(text, static_cast<size_t>(count));
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
      if (history_)
        history(history_, &history_event_, H_ENTER, line.c_str());
      return line;
    }
    input_.clear();
    render();
    for (;;) {
      if (cancel.requested)
        return std::nullopt;
      const int ch = getch();
      if (ch == ERR)
        continue;
      if (ch == KEY_RESIZE) {
        render();
        continue;
      }
      if (ch == bindings_["quit"])
        return std::nullopt;
      if (ch == bindings_["submit"] || ch == KEY_ENTER) {
        auto out = input_;
        say("you> " + out);
        input_.clear();
        return out;
      }
      if (ch == bindings_["backspace"] || ch == KEY_BACKSPACE || ch == 8) {
        if (!input_.empty()) {
          do {
            const unsigned char b = input_.back();
            input_.pop_back();
            if ((b & 0xc0) != 0x80)
              break;
          } while (!input_.empty());
        }
        render();
      } else if (ch >= 32 && ch <= 255 && input_.size() < 65536) {
        input_ += static_cast<char>(ch);
        render();
      }
    }
  }
  bool approve(const PermissionRequest &r, Cancellation &cancel) {
    say(r.tool + ": " + r.target + " [y/N]");
    if (!curses_) {
      auto line = read(cancel);
      return line && (*line == "y" || *line == "yes");
    }
    for (;;) {
      cancel.check();
      int ch = getch();
      if (ch == 'y' || ch == 'Y')
        return true;
      if (ch == 'n' || ch == 'N' || ch == '\n' || ch == '\r')
        return false;
      if (ch == KEY_RESIZE)
        render();
    }
  }
  std::string ask(const std::string &question, const std::vector<std::string> &options,
                  Cancellation &cancel) {
    say(question);
    for (size_t i = 0; i < options.size(); ++i)
      say(std::to_string(i + 1) + ". " + options[i]);
    if (!options.empty())
      say("Enter a choice number or a text answer.");
    auto answer = read(cancel);
    cancel.check();
    if (!answer.has_value())
      throw Error("User dismissed the question");
    const auto &text = answer.value();
    for (size_t i = 0; i < options.size(); ++i)
      if (text == std::to_string(i + 1))
        return options[i];
    return text;
  }
  void begin_active() {
    input_.clear();
    if (!curses_ && restore_) {
      el_set(edit_, EL_PREP_TERM, 0);
      auto settings = original_;
      settings.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
      settings.c_cc[VMIN] = 0;
      settings.c_cc[VTIME] = 0;
      if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) != 0)
        throw Error("Cannot prepare active terminal input");
    }
  }
  void end_active() {
    if (!curses_ && restore_)
      (void)tcsetattr(STDIN_FILENO, TCSANOW, &original_);
    input_.clear();
    render();
  }
  std::optional<std::string> active_input(bool &closed) {
    int ch = ERR;
    if (curses_)
      ch = getch();
    else {
      pollfd fd{STDIN_FILENO, POLLIN, 0};
      const auto ready = ::poll(&fd, 1, 50);
      if (ready > 0) {
        unsigned char c = 0;
        const auto n = ::read(STDIN_FILENO, &c, 1);
        if (n == 1)
          ch = c;
        else if (n == 0)
          closed = true;
      }
    }
    if (ch == ERR)
      return {};
    if (ch == KEY_RESIZE) {
      render();
      return {};
    }
    if (ch == bindings_["quit"]) {
      closed = true;
      return {};
    }
    if (ch == bindings_["submit"] || ch == KEY_ENTER || ch == '\r') {
      auto text = input_;
      input_.clear();
      if (!curses_)
        std::cout << '\n';
      else
        say("you> " + text);
      render();
      return text;
    }
    if (ch == bindings_["backspace"] || ch == KEY_BACKSPACE || ch == 8) {
      if (!input_.empty()) {
        do {
          const auto b = static_cast<unsigned char>(input_.back());
          input_.pop_back();
          if ((b & 0xc0) != 0x80)
            break;
        } while (!input_.empty());
        if (!curses_)
          std::cout << "\b \b" << std::flush;
      }
    } else if (ch >= 32 && ch <= 255 && input_.size() < 65536) {
      input_ += static_cast<char>(ch);
      if (!curses_)
        std::cout << static_cast<char>(ch) << std::flush;
    }
    render();
    return {};
  }
  void progress(const Json &event) {
    if (event.value("type", "") == "item.started" && event.contains("item") &&
        event["item"].contains("tool"))
      say("[" + event["item"]["tool"].get<std::string>() + "]");
    const auto type = event.value("type", "");
    if (type == "turn.completed" || type == "turn.failed") {
      progress_.clear();
      render();
    }
    if (curses_ && type == "item.updated") {
      progress_ = "Responding: " + event["item"].value("text_delta", "");
      render();
    }
  }
};
} // namespace
int cli_main(const std::vector<std::string> &args, Cancellation &cancel) {
  const auto o = parse(args);
  if (o.help) {
    std::cout << help;
    return 0;
  }
  if (o.version) {
    std::cout << "codet " CODET_VERSION "\n";
    return 0;
  }
  NativeFileSystem files;
  SystemClock clock;
  CurlTransport http;
  std::shared_ptr<Interaction> interaction;
  const auto terminal_thread = std::this_thread::get_id();
  // Keep session ownership until background-process destruction has finished.
  std::shared_ptr<void> lifecycle_owner, current_owner;
  struct Diagnostics {
    std::weak_ptr<Terminal> screen;
    bool buffering = false;
    DiagnosticBuffer live{65536}, cleanup{65536};
    void emit(const std::string &text) {
      if (buffering)
        cleanup.add(text);
      else if (auto terminal = screen.lock()) {
        terminal->diagnostic(text);
        if (terminal->fullscreen())
          live.add(text);
      } else
        std::cerr << text << '\n';
    }
    void flush() {
      live.flush(std::cerr);
      cleanup.flush(std::cerr);
    }
  };
  auto diagnostics = std::make_shared<Diagnostics>();
  DiagnosticSink diagnostic = [&, diagnostics](const std::string &text) {
    if (interaction && std::this_thread::get_id() != terminal_thread)
      interaction->post({{"type", "codet.diagnostic"}, {"text", text}});
    else
      diagnostics->emit(text);
  };
  std::string home = env("CODET_HOME");
  if (home.empty()) {
    const auto h = env("HOME");
    if (h.empty())
      throw Error("Set CODET_HOME or HOME", 2);
    home = (fs::path(h) / ".codet").string();
  }
  if (!fs::is_directory(o.cwd))
    throw Error("Working directory does not exist", 2);
  if (o.ephemeral && (o.command == "resume" || o.command == "fork" || o.command == "export"))
    throw Error("--ephemeral cannot select a saved session", 2);
  auto overrides = o.overrides;
  auto setting = [&](const std::string &key, const std::string &value) {
    if (!value.empty())
      overrides.push_back(key + "=" + Json(value).dump());
  };
  setting("provider", o.provider);
  setting("model", o.model);
  setting("ui", o.ui);
  setting("permission_mode", o.mode);
  setting("approval_policy", o.approval);
  Config config = load_config(home, o.cwd, o.profile, overrides, files);
  for (const auto &rule : o.allow)
    config.add_allowed(rule);
  for (const auto &rule : o.deny)
    config.add_denied(rule);
  for (const auto &root : o.roots)
    config.roots.push_back(files.canonical(config.cwd / root));
  if (config.ui != "auto" && config.ui != "line" && config.ui != "curses")
    throw Error("Invalid UI mode", 2);
  validate_tool_rules(config.rules().allow);
  validate_tool_rules(config.rules().deny);
  CredentialStore credentials(config.home, files, clock);
  if (o.command == "login") {
    if (o.action == "status") {
      const auto auth = credentials.load();
      const bool present = auth.contains("codex") || auth.contains("api_keys");
      std::cout << Json({{"credentials_present", present},
                         {"subscription_imported", auth.contains("codex")}})
                       .dump()
                << '\n';
      return present ? 0 : 1;
    }
    if (!o.import.empty() && o.key)
      throw Error("Choose one credential input", 2);
    if (!o.import.empty())
      credentials.import(o.import);
    else if (o.key) {
      auto key = stdin_text();
      while (!key.empty() && (key.back() == '\n' || key.back() == '\r'))
        key.pop_back();
      credentials.save_key(config.provider == "codex" ? "openai" : config.provider, key);
    } else
      throw Error("Native sign-in is deferred. Use login --import FILE or --with-api-key", 2);
    std::cerr << "Credentials stored privately in Codet state\n";
    return 0;
  }
  if (o.command == "doctor") {
    NativeProcesses processes;
    std::cout << Json({{"version", CODET_VERSION},
                       {"pointer_bits", sizeof(void *) * 8},
                       {"provider", config.provider},
                       {"cwd", config.cwd.string()},
                       {"state", config.home.string()},
                       {"os_sandbox", false},
                       {"application_permissions", true},
                       {"target_runtime_verified", false},
                       {"capabilities",
                        Tools(config, files, processes, http, clock).capabilities()}})
                     .dump(2)
              << '\n';
    return 0;
  }
  fs::path state_home = config.home;
  struct Cleanup {
    fs::path p;
    DiagnosticSink diagnostics;
    ~Cleanup() {
      if (!p.empty()) {
        std::error_code e;
        fs::remove_all(p, e);
        if (e) {
          try {
            report_diagnostic(diagnostics, "Ephemeral cleanup: " + e.message());
          } catch (...) { // NOLINT(bugprone-empty-catch): Cleanup diagnostics must preserve the
                          // invocation result.
          }
        }
      }
    }
  } cleanup{{}, diagnostic};
  if (o.ephemeral) {
    std::string pattern = (fs::temp_directory_path() / "codet-ephemeral-XXXXXX").string();
    std::vector<char> buf(pattern.begin(), pattern.end());
    buf.push_back(0);
    char *p = mkdtemp(buf.data());
    if (!p)
      throw Error("Cannot create ephemeral state");
    state_home = p;
    cleanup.p = p;
    config.capture_root = fs::path(p) / "tool-output";
  }
  // Destroy processes/capture handles before removing the private invocation directory.
  NativeProcesses processes;
  processes.set_diagnostics(diagnostic);
  StateStore state(state_home);
  if (o.command == "sessions") {
    std::cout << state.list().dump(2) << '\n';
    return 0;
  }
  Session session;
  if (o.command == "resume" || o.command == "fork" || o.command == "export") {
    if (o.last)
      session = state.last();
    else if (!o.session.empty())
      session = state.load(o.session);
    else {
      if (o.exec || !isatty(STDIN_FILENO))
        throw Error("Select a session id or --last", 2);
      std::cerr << state.list().dump(2) << "\nSession id: ";
      std::string id;
      std::getline(std::cin, id);
      session = state.load(id);
    }
    if (o.command == "fork")
      session = state.fork(session);
    if (o.command == "export") {
      if (o.format == "text") {
        for (const auto &m : session.messages)
          std::cout << m.role << ": " << m.text << "\n\n";
      } else
        std::cout << Json({{"id", session.id},
                           {"name", session.name},
                           {"messages", session.messages}})
                         .dump(2)
                  << '\n';
      return 0;
    }
  } else
    session = state.create();
  lifecycle_owner = state.acquire(session.id);
  current_owner = lifecycle_owner;
  session = state.load_id(session.id, false);
  state.reconcile(session);
  const auto resolved =
      resolve_selection(config, o.command == "resume" || o.command == "fork"
                                    ? std::optional<Selection>{{session.provider, session.model}}
                                    : std::nullopt);
  config.provider = resolved.provider;
  config.model = resolved.model;
  session.model = config.model;
  session.provider = config.provider;
  state.save(session);
  const bool interactive = !o.exec && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
  std::shared_ptr<Terminal> terminal;
  diagnostics->live = DiagnosticBuffer(config.tool_output_limit);
  diagnostics->cleanup = DiagnosticBuffer(config.tool_output_limit);
  struct RestoreDiagnostics {
    std::shared_ptr<Terminal> &terminal;
    std::shared_ptr<Diagnostics> diagnostics;
    RestoreDiagnostics(std::shared_ptr<Terminal> &t, std::shared_ptr<Diagnostics> d)
        : terminal(t), diagnostics(std::move(d)) {}
    ~RestoreDiagnostics() {
      terminal.reset();
      diagnostics->buffering = false;
      diagnostics->flush();
    }
  } restore_diagnostics{terminal, diagnostics};
  if (interactive) {
    terminal = std::make_shared<Terminal>(config, o.color);
    terminal->set_mode(config.permission_mode);
    diagnostics->screen = terminal;
  }
  interaction = terminal ? std::make_shared<Interaction>() : std::shared_ptr<Interaction>{};
  Approver approver;
  Questioner questioner;
  if (terminal)
    approver = [&](const PermissionRequest &request) {
      const auto answer = interaction->request(
          "codet.approval", {{"tool", request.tool}, {"target", request.target}}, cancel);
      return answer == "y" || answer == "yes";
    };
  if (terminal)
    questioner = [&](const std::string &question, const std::vector<std::string> &options) {
      return interaction->request("codet.question", {{"question", question}, {"options", options}},
                                  cancel);
    };
  Tools tools(config, files, processes, http, clock, approver, questioner, diagnostic, interaction);
  ApiProvider provider(http, credentials, clock);
  Json schema;
  if (!o.schema.empty()) {
    try {
      schema = Json::parse(files.read(fs::absolute(o.schema), size_t{1024} * 1024), nullptr, false);
    } catch (const Error &e) {
      throw Error(e.what(), 2);
    }
    if (!schema.is_object())
      throw Error("Invalid output schema JSON", 2);
  }
  auto events = [&](const Json &event) {
    if (terminal)
      interaction->post(event);
    else if (o.format == "jsonl")
      std::cout << event.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n' << std::flush;
    else if (event.value("type", "") == "item.started" && event["item"].contains("tool"))
      std::cerr << "[" << event["item"]["tool"].get<std::string>() << "]\n";
  };
  Agent agent(config, provider, tools, state, events, diagnostic);
  struct SessionEnd {
    std::function<void()> end;
    ~SessionEnd() {
      try {
        end();
      } catch (const std::exception &error) {
        std::cerr << "SessionEnd hook cleanup failed: " << safe_text(error.what()) << '\n';
      } catch (...) {
        std::cerr << "SessionEnd hook cleanup failed: unknown exception\n";
      }
    }
  } session_end{[&, started_id = session.id] {
    diagnostics->buffering = true;
    struct RestoreTerminal {
      std::shared_ptr<Terminal> &terminal;
      Diagnostics &diagnostics;
      RestoreTerminal(std::shared_ptr<Terminal> &screen, Diagnostics &sink)
          : terminal(screen), diagnostics(sink) {}
      ~RestoreTerminal() {
        terminal.reset();
        diagnostics.buffering = false;
        diagnostics.flush();
      }
    } restore{terminal, *diagnostics};
    try {
      Cancellation cleanup_cancel;
      tools.run_hooks("SessionEnd", {{"hook_event_name", "SessionEnd"}, {"session_id", started_id}},
                      cleanup_cancel, 10000, &diagnostics->cleanup);
    } catch (const std::exception &error) {
      report_diagnostic(diagnostic, "SessionEnd hook cleanup failed: " + std::string(error.what()));
    } catch (...) {
      report_diagnostic(diagnostic, "SessionEnd hook cleanup failed: unknown exception");
    }
  }};
  const auto started = tools.run_hooks("SessionStart",
                                       {{"hook_event_name", "SessionStart"},
                                        {"session_id", session.id},
                                        {"source", o.command == "resume" ? "resume"
                                                   : o.command == "fork" ? "fork"
                                                                         : "startup"},
                                        {"cwd", config.cwd.string()}},
                                       cancel);
  if (started.blocked || started.stopped)
    throw Error("Session stopped by hook: " + started.reason);
  if (!started.additional_context.empty()) {
    Message context;
    context.role = "system";
    context.text = "SessionStart hook context:\n" + started.additional_context;
    session.messages.push_back(context);
    state.save(session);
  }
  bool quit_requested = false;
  auto execute_agent = [&](const std::string &text) -> RunResult {
    if (!terminal)
      return agent.run(session, text, cancel, schema);
    terminal->begin_active();
    auto work =
        std::async(std::launch::async, [&] { return agent.run(session, text, cancel, schema); });
    std::optional<Json> pending;
    const auto deliver = [&] {
      for (const auto &event : interaction->events()) {
        const auto type = event.value("type", "");
        if (type == "codet.diagnostic")
          diagnostics->emit(event.at("text"));
        else if (type == "codet.approval") {
          pending = event;
          terminal->say(event.at("tool").get<std::string>() + ": " +
                        event.at("target").get<std::string>() + " [y/N]");
        } else if (type == "codet.question") {
          pending = event;
          terminal->say(event.at("question"));
          const auto options = event.at("options").get<std::vector<std::string>>();
          for (size_t i = 0; i < options.size(); ++i)
            terminal->say(std::to_string(i + 1) + ". " + options[i]);
          terminal->say("Enter a choice number or a text answer.");
        } else if (type == "codet.user_input.requested") {
          for (const auto &q : event.at("questions")) {
            terminal->say(q.at("title"));
            size_t i = 0;
            for (const auto &option : q.value("options", Json::array()))
              terminal->say(std::to_string(++i) + ". " + option.get<std::string>());
          }
        } else if (type == "codet.user_message")
          terminal->say(event.at("message"));
        else if (type == "codet.artifact")
          terminal->say("[Image: " + event.at("artifact").at("path").get<std::string>() + "]");
        else
          terminal->progress(event);
      }
    };
    try {
      while (work.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        deliver();
        bool closed = false;
        const auto line = terminal->active_input(closed);
        if (closed)
          cancel.requested = true;
        if (line) {
          if (*line == "/quit" || *line == "/exit") {
            quit_requested = true;
            cancel.requested = true;
          } else if (pending) {
            interaction->answer(pending->at("request_id"), *line);
            pending.reset();
          } else if (line->rfind("/permissions", 0) == 0)
            terminal->say("Change permission modes after the active turn completes.");
          else
            interaction->submit(*line);
        }
      }
      deliver();
      terminal->end_active();
      return work.get();
    } catch (...) {
      cancel.requested = true;
      while (work.valid() &&
             work.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
      }
      deliver();
      terminal->end_active();
      throw;
    }
  };
  auto run = [&](const std::string &text) {
    try {
      auto result = execute_agent(text);
      const auto answer = result.structured.is_null() ? result.text : result.structured.dump();
      if (!o.output.empty())
        files.atomic_write(fs::absolute(o.output), answer + "\n", true);
      if (terminal)
        terminal->say(answer);
      else if (o.format == "text")
        std::cout << answer << '\n';
      else if (o.format == "json")
        std::cout << Json({{"session_id", session.id},
                           {"status", "success"},
                           {"result", result.text},
                           {"structured_output", result.structured},
                           {"usage",
                            {{"input_tokens", result.usage.input},
                             {"output_tokens", result.usage.output},
                             {"cached_input_tokens", result.usage.cached}}}})
                         .dump()
                  << '\n';
      return 0;
    } catch (const Error &e) {
      if (terminal) {
        terminal->say(std::string("Error: ") + safe_text(e.what()));
        cancel.requested = false;
        return e.code;
      }
      if (o.format == "json")
        std::cout
            << Json({{"session_id", session.id}, {"status", "error"}, {"error", e.what()}}).dump()
            << '\n';
      throw;
    } catch (const std::exception &e) {
      if (terminal) {
        terminal->say("Error: " + safe_text(e.what()));
        return 1;
      }
      throw Error(safe_text(e.what()));
    }
  };
  std::string prompt = combined(o.positionals);
  if (o.command == "memory") {
    if (o.action != "consolidate")
      throw Error("Use memory consolidate", 2);
    config.roots.push_back(config.home / "memory");
    prompt = "Explicit memory consolidation: use the memory preview and apply_patch to consolidate "
             "Markdown files in the Codet "
             "memory directory, consolidate accurate "
             "useful project facts and feedback, remove contradictions by updating files, preserve "
             "provenance, and report what changed. Do not perform unrelated work.";
  }
  if (!interactive) {
    if (!isatty(STDIN_FILENO)) {
      auto input = stdin_text();
      if (prompt.empty() || prompt == "-")
        prompt = input;
      else if (!input.empty())
        prompt += "\n\nAdditional stdin context:\n" + input;
    }
    if (prompt.empty() && o.command == "fork") {
      std::cout << Json({{"type", "thread.started"}, {"thread_id", session.id}}).dump() << '\n';
      return 0;
    }
    if (prompt.empty())
      throw Error("Headless mode requires a prompt or stdin", 2);
    return run(prompt);
  }
  if (!prompt.empty())
    run(prompt);
  if (quit_requested)
    return 0;
  while (!cancel.requested && !quit_requested) {
    auto input = terminal->read(cancel);
    if (!input)
      break;
    if (input->empty())
      continue;
    if (*input == "/quit" || *input == "/exit")
      break;
    if (*input == "/help") {
      terminal->say("/help /quit /permissions [MODE] /sessions /new /name TEXT /export /memory "
                    "consolidate; custom "
                    "commands from commands/*.md");
      continue;
    }
    if (*input == "/permissions" || input->rfind("/permissions ", 0) == 0) {
      if (*input != "/permissions") {
        const auto mode = input->substr(13);
        if (mode != "default" && mode != "plan" && mode != "accept-edits" && mode != "bypass") {
          terminal->say("Use /permissions default|plan|accept-edits|bypass");
          continue;
        }
        config.permission_mode = mode;
        terminal->set_mode(mode);
      }
      terminal->say("Permissions: " + config.permission_mode);
      continue;
    }
    if (*input == "/sessions") {
      terminal->say(state.list().dump(2));
      continue;
    }
    if (*input == "/new") {
      auto fresh = state.create();
      auto fresh_owner = state.acquire(fresh.id);
      session = std::move(fresh);
      current_owner = std::move(fresh_owner);
      terminal->say("Started " + session.id);
      continue;
    }
    if (input->rfind("/name ", 0) == 0) {
      session.name = input->substr(6);
      state.save(session);
      continue;
    }
    if (*input == "/export") {
      terminal->say(Json(session.messages).dump(2));
      continue;
    }
    if (*input == "/memory consolidate") {
      config.roots.push_back(config.home / "memory");
      *input = "Explicit memory consolidation: use the included memory preview and apply_patch to "
               "consolidate files in " +
               (config.home / "memory").string() + "; report changes.";
    }
    run(*input);
  }
  return quit_requested ? 0 : cancel.requested ? 130 : 0;
}
} // namespace codet
