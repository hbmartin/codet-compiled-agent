#include "codet/detail/cli.hpp"
#include "codet/computer.hpp"
#include "codet/core.hpp"
#include "codet/detail/approval_identity.hpp"
#include "codet/detail/approval_review.hpp"
#include "codet/detail/cleanup_diagnostics.hpp"
#include "codet/detail/cli_diagnostics.hpp"
#include "codet/detail/cli_output.hpp"
#include "codet/detail/curses_input.hpp"
#include "codet/detail/curses_render.hpp"
#include "codet/detail/line_prompt.hpp"
#include "codet/detail/native_terminal.hpp"
#include "codet/detail/request_input_controller.hpp"
#include "codet/detail/terminal_input.hpp"
#include "codet/detail/ui_history.hpp"
#include "codet/diagnostics.hpp"
#include "codet/interaction.hpp"
#include "codet/prompt.hpp"
#include "codet/tool_registry.hpp"
#include <algorithm>
#include <curses.h>
#include <cwchar>
#include <future>
#include <histedit.h>
#include <iostream>
#include <nlohmann/json-schema.hpp>
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
       codet repair SESSION [--last] [--provider-cache] [--recovery] [--session-token]
       codet memory consolidate

  -i, --image PATH              Attach an image (repeatable, up to four)
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
      --durability MODE         durable (default) or fast
      --ephemeral               Temporary automatic session state
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
  bool help = false, version = false, repair_provider = false, repair_recovery = false;
  bool repair_token = false;
  std::string command, action, session, profile, provider, model,
      format = "text", ui, mode, approval, durability, color = "auto";
  fs::path cwd = fs::current_path(), schema, output, import;
  std::vector<std::string> overrides, allow, deny;
  std::vector<fs::path> roots, images;
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
        else if (a == "--provider-cache")
          o.repair_provider = true;
        else if (a == "--recovery")
          o.repair_recovery = true;
        else if (a == "--session-token")
          o.repair_token = true;
        else if (a == "--ephemeral")
          o.ephemeral = true;
        else if (a == "--durability")
          o.durability = value();
        else if (a == "--with-api-key")
          o.key = true;
        else if (a == "--model" || a == "-m")
          o.model = value();
        else if (a == "--image" || a == "-i")
          o.images.push_back(fs::absolute(value()));
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
           a == "doctor" || a == "memory" || a == "repair")) {
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
  if (o.command == "resume" || o.command == "fork" || o.command == "export" ||
      o.command == "repair") {
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
  if ((o.repair_provider || o.repair_recovery || o.repair_token) && o.command != "repair")
    throw Error("--provider-cache, --recovery and --session-token require repair", 2);
  if (o.repair_token && (o.repair_provider || o.repair_recovery))
    throw Error("--session-token cannot be combined with --provider-cache or --recovery", 2);
  if (o.command == "repair" &&
      ((!o.last && o.session.empty()) || !o.positionals.empty() || o.ephemeral))
    throw Error("Use repair SESSION or repair --last with optional --provider-cache / --recovery / "
                "--session-token",
                2);
  return o;
}
class Terminal final : public detail::CliDiagnosticSurface {
  detail::CliOutput &output_;
  detail::DiagnosticWriter &errors_;
  detail::NativeTerminalResourceOps native_resources_;
  bool curses_ = false, display_dirty_ = true;
  detail::CursesDrawOps drawing_;
  HistEvent history_event_{};
  detail::UiHistory transcript_;
  detail::RequestInputController input_;
  std::optional<detail::ApprovalReview> approval_;
  bool curses_unicode_ = true;
  std::string status_, progress_;
  std::string prompt_;
  std::optional<detail::LinePrompt> line_;
  std::map<std::string, int> bindings_{{"submit", 10}, {"quit", 4}, {"backspace", 127}};
  const Cancellation *read_cancel_ = nullptr;
  // Release native callbacks before destroying the prompt/history data they can reference.
  detail::TerminalResources resources_{native_resources_};
  static int read_character(EditLine *edit, wchar_t *character) {
    void *data = nullptr;
    el_get(edit, EL_CLIENTDATA, &data);
    auto *terminal = static_cast<Terminal *>(data);
    std::mbstate_t state{};
    for (;;) {
      if (terminal && terminal->read_cancel_ && terminal->read_cancel_->requested)
        return 0;
      const int ready = detail::wait_terminal_input(STDIN_FILENO, 100);
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
  void clear_review_input() {
    input_.presentation_clear();
  }
  std::string input_view(std::string_view prompt, size_t columns) const {
    return input_.view(prompt, columns);
  }

public:
  void flush_prompt() {
    if (line_)
      line_->flush();
  }
  void discarded_line(detail::InputDiscard reason = detail::InputDiscard::unfinished) {
    say(std::string(detail::discard_message(reason)));
    flush_prompt();
  }

private:
  void render() {
    if (!curses_)
      return;
    detail::CursesFrame frame(display_dirty_, drawing_);
    int rows = 0, cols = 0;
    getmaxyx(stdscr, rows, cols);
    if (approval_) {
      if (approval_->reshape(rows, cols))
        clear_review_input();
      const auto draw = [&](int row, std::string_view text) {
        if (!frame.row(row, text, cols, true))
          throw Error("Cannot render approval review");
      };
      if (!approval_->usable()) {
        if (rows > 1 && cols > 1)
          draw(0, std::string_view((input_.stale() ? "Enter discards unfinished input"
                                                   : "Resize to review; approval disabled"))
                      .substr(0, static_cast<size_t>(cols - 1)));
        if (rows > 0 && cols > 1)
          draw(rows - 1, input_view(rows == 1 ? "!> " : "codet> ", static_cast<size_t>(cols - 1)));
      } else {
        const auto heading = "Approval review " + std::to_string(approval_->page() + 1) + "/" +
                             std::to_string(approval_->pages());
        draw(0, std::string_view(heading).substr(0, static_cast<size_t>(cols - 1)));
        for (size_t i = 0; i < approval_->capacity(); ++i)
          draw(static_cast<int>(i) + 1, approval_->line(i));
        const std::string_view navigation = "Space/PgDn: next  PgUp: back  /quit: exit";
        draw(rows - 3, navigation.substr(0, static_cast<size_t>(cols - 1)));
        const std::string_view prompt = input_.stale() ? "Enter discards unfinished input"
                                        : approval_->approved_review()
                                            ? detail::RequestInputController::approval_prompt
                                            : "Read all pages; n denies";
        draw(rows - 2, prompt.substr(0, static_cast<size_t>(cols - 1)));
        draw(rows - 1, input_view("codet> ", static_cast<size_t>(cols - 1)));
      }
      frame.require_finish();
      const bool previously_reviewed = approval_->approved_review();
      approval_->displayed();
      if (!input_.stale() && !previously_reviewed && approval_->approved_review()) {
        draw(rows - 2, detail::RequestInputController::approval_prompt);
        frame.require_finish();
      }
      return;
    }
    if (rows < 4 || cols < 8) {
      if (rows > 0 && cols > 1)
        frame.row(rows - 1, input_view("codet> ", static_cast<size_t>(cols - 1)), cols);
      (void)frame.finish();
      return;
    }
    const auto guidance = input_.guidance();
    const auto heading = guidance.empty() ? status_ + (progress_.empty() ? "" : " | " + progress_)
                                          : std::string(guidance);
    frame.row(0, heading, cols);
    int y = rows - 3;
    for (auto it = transcript_.lines().rbegin(); it != transcript_.lines().rend() && y > 0;
         ++it, --y)
      frame.row(y, *it, cols);
    frame.row(rows - 1, input_view("codet> ", static_cast<size_t>(cols - 1)), cols);
    (void)frame.finish();
  }

public:
  void begin_request(const Json &event) {
    input_.open(detail::ApprovalIdentity::from(event), detail::RequestKind::question);
  }
  void begin_approval(const Json &event) {
    const auto identity = detail::ApprovalIdentity::from(event);
    input_.open(identity, detail::RequestKind::approval);
    const auto target =
        event.at("tool").get<std::string>() + ": " + event.at("target").get<std::string>();
    if (!curses_) {
      say_rendered({detail::CursesText(detail::literal_text(target)).text()});
      if (event.contains("context") && !event.at("context").empty())
        say_rendered(
            {detail::CursesText(detail::terminal_text(safe_json(event.at("context")).dump()))
                 .text()});
      return;
    }
    auto document = detail::literal_text(target);
    if (event.contains("context") && !event.at("context").empty())
      document += "\nContext:\n" + detail::terminal_text(safe_json(event.at("context")).dump());
    approval_.emplace(identity.session, identity.turn, identity.request, document, curses_unicode_);
    const auto label = "[Approval " + detail::literal_text(identity.request) + "]";
    remember(label);
    transcript_.add(approval_->document(),
                    detail::UiHistory::approval_limit -
                        std::min(detail::UiHistory::approval_limit, label.size() + 1));
    clear_review_input();
    render();
  }
  bool approval_reviewed(const Json &event) const {
    return !curses_ ||
           (approval_ && approval_->identity.matches(event) && approval_->approved_review());
  }
  void end_approval(std::optional<std::string_view> answer = {}) {
    input_.close();
    auto finished = std::move(approval_);
    approval_.reset();

    if (finished)
      remember(std::string("[Approval ") +
               (!answer                     ? "cancelled"
                : detail::approves(*answer) ? "approved"
                                            : "denied") +
               "] " + detail::literal_text(finished->identity.request));
    render();
  }
  Terminal(const Config &c, const std::string &color, detail::CliOutput &output,
           detail::DiagnosticWriter &errors, const detail::CursesDrawOps &drawing,
           detail::IoContext &io)
      : output_(output), errors_(errors), drawing_(drawing) {
    curses_ =
        (c.ui == "curses" || (c.ui == "auto" && env("TERM") != "dumb" && !env("TERM").empty()));
    for (auto it = c.keybindings.begin(); it != c.keybindings.end(); ++it) {
      if (!bindings_.count(it.key()) || !it.value().is_number_integer())
        throw Error("keybindings support integer submit/quit/backspace codes", 2);
      bindings_[it.key()] = it.value();
    }
    if (curses_) {
      resources_.start_screen();
      curses_unicode_ = detail::curses_unicode_supported();
      input_.unicode(curses_unicode_);
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
      resources_.start_editor();
      struct stat destination {};
      detail::OutputAttempt inspection(io.cancel, detail::OutputMode::diagnostic);
      if (inspection.inspect([&] { return ::fstat(STDOUT_FILENO, &destination); }) < 0)
        throw Error("Cannot inspect prompt terminal");
      line_.emplace(input_, output_, io, prompt_, destination);
      el_set(
          resources_.editor(), EL_PROMPT, +[](EditLine *edit) -> const char * {
            void *data = nullptr;
            el_get(edit, EL_CLIENTDATA, &data);
            return static_cast<Terminal *>(data)->prompt_.c_str();
          });
      el_set(resources_.editor(), EL_EDITOR, "emacs");
      el_set(resources_.editor(), EL_SIGNAL, 0);
      el_set(resources_.editor(), EL_CLIENTDATA, this);
      el_set(resources_.editor(), EL_GETCFN, &Terminal::read_character);
      resources_.start_history();
      if (resources_.entries()) {
        history(resources_.entries(), &history_event_, H_SETSIZE, 500);
        el_set(resources_.editor(), EL_HIST, history, resources_.entries());
      }
      set_mode(c.permission_mode);
      (void)errors_.write({status_});
    }
  }
  ~Terminal() override = default;
  bool fullscreen() const override {
    return curses_;
  }
  void diagnostic(const std::string &text) override {
    if (curses_) {
      say(text);
      return;
    }
    const auto shown = detail::terminal_text(text);
    diagnostic_rendered({shown});
  }
  void diagnostic_rendered(detail::RenderedTextView text) override {
    if (curses_)
      say_rendered(text);
    else {
      (void)errors_.write({text.text});
    }
  }
  void set_mode(const std::string &mode) {
    status_ = "Permissions: " + mode + "; no OS shell sandbox";
    prompt_ = "codet> [" + mode + "] ";
    if (curses_)
      render();
  }
  void remember_bounded(std::string_view text, std::optional<detail::RenderMode> raw = {}) {
    remember(detail::CursesText::bounded(text, detail::UiHistory::byte_limit,
                                         detail::UiHistory::omission, curses_unicode_, raw)
                 .text);
    render();
  }
  void say_literal(const std::string &text) {
    if (curses_) {
      remember_bounded(text, detail::RenderMode::literal);
      return;
    }
    const auto shown = detail::literal_text(text);
    say_rendered({shown});
  }
  void say(const std::string &text) {
    if (curses_) {
      remember_bounded(text, detail::RenderMode::prose);
      return;
    }
    const auto shown = detail::terminal_text(text);
    say_rendered({shown});
  }
  void say_rendered(detail::RenderedTextView text) {
    if (curses_)
      remember_bounded(text.text);
    else {
      (void)output_.display({text.text});
    }
  }
  void remember(const std::string &text) {
    transcript_.add(text);
  }
  std::optional<std::string> read(Cancellation &cancel) {
    if (!curses_ && (input_.stale() || (input_.ordinary_draft() && !input_.input().empty()))) {
      begin_active();
      flush_prompt();
      struct Restore {
        Terminal &terminal;
        ~Restore() {
          try {
            terminal.end_active();
          } catch (...) {
            detail::secondary_diagnostic([&] {
              (void)terminal.errors_.write({"Terminal active-mode restoration also failed"});
            });
          }
        }
      } restore{*this};
      for (;;) {
        if (cancel.requested)
          return std::nullopt;
        flush_prompt();
        bool closed = false;
        auto line = active_input(closed);
        if (closed)
          return std::nullopt;
        if (line) {
          if (const auto submission = line->classify(); submission.rejection) {
            discarded_line(*submission.rejection);
            break; // The stale idle line ended; hand the empty prompt back to libedit.
          }
          if (resources_.entries())
            history(resources_.entries(), &history_event_, H_ENTER, line->text.c_str());
          return std::move(line->text);
        }
      }
    }
    if (!curses_) {
      if (line_)
        line_->editor();
      read_cancel_ = &cancel;
      el_set(resources_.editor(), EL_PREP_TERM, 1);
      int count = 0;
      const char *text = el_gets(resources_.editor(), &count);
      if (!text || count <= 0)
        return std::nullopt;
      std::string line(text, static_cast<size_t>(count));
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
      if (resources_.entries())
        history(resources_.entries(), &history_event_, H_ENTER, line.c_str());
      const auto submitted = input_.take(std::move(line));
      if (submitted.action() == detail::InputAction::discard) {
        discarded_line();
        return read(cancel);
      }
      return submitted.text;
    }
    if (!input_.ordinary_draft())
      clear_review_input();
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
        auto submitted = input_.take(input_.input().raw());
        if (submitted.action() == detail::InputAction::discard) {
          discarded_line();
          continue;
        }
        say("you> " + submitted.text);
        return submitted.text;
      }
      if (ch == bindings_["backspace"] || ch == KEY_BACKSPACE || ch == 8) {
        if (!input_.input().empty()) {
          input_.erase_character();
        }
        render();
      } else if (ch >= 32 && ch <= 255) {
        input_.append(static_cast<char>(ch));
        render();
      }
    }
  }
  void begin_active() {
    if (line_)
      line_->acquire();
    if (!input_.ordinary_draft())
      clear_review_input();
    if (!curses_ && resources_.saved()) {
      el_set(resources_.editor(), EL_PREP_TERM, 0);
      resources_.begin_active();
    }
  }
  void end_active() {
    if (!curses_ && resources_.saved())
      resources_.end_active();
    end_approval();
    if (line_)
      line_->release();
  }
  std::optional<detail::InputLine> active_input(bool &closed) {
    int ch = ERR;
    if (curses_)
      ch = getch();
    else {
      const auto ready = detail::wait_terminal_input(STDIN_FILENO, 50);
      if (ready > 0) {
        unsigned char c = 0;
        const auto n = ::read(STDIN_FILENO, &c, 1);
        if (n == 1)
          ch = c;
        else if (n == 0)
          closed = true;
      } else if (ready < 0 && errno != EINTR)
        closed = true;
    }
    if (ch == ERR)
      return {};
    if (ch == KEY_RESIZE) {
      render();
      return {};
    }
    if (approval_ &&
        (ch == KEY_NPAGE || ch == KEY_PPAGE || (ch == ' ' && input_.input().empty()))) {
      clear_review_input();
      if (ch == KEY_PPAGE)
        approval_->previous();
      else
        approval_->next();
      render();
      return {};
    }
    if (ch == bindings_["quit"]) {
      closed = true;
      return {};
    }
    if (ch == bindings_["submit"] || ch == KEY_ENTER || ch == '\r') {
      if (line_)
        line_->submitted();
      auto submitted = input_.take(input_.input().raw());
      const auto &text = submitted.text;
      if (!submitted.discarded && approval_ && detail::approves(text) &&
          !approval_->approved_review()) {
        render();
        return {};
      }
      if (submitted.discarded)
        return submitted;
      if (curses_ && approval_) {
        const auto shown =
            detail::CursesText(detail::literal_text("you> " + text), curses_unicode_).text();
        say_rendered({shown});
      } else if (curses_)
        say("you> " + text);
      render();
      return submitted;
    }
    if (ch == bindings_["backspace"] || ch == KEY_BACKSPACE || ch == 8) {
      if (!input_.input().empty()) {
        if (line_)
          line_->erase_character();
        else
          input_.erase_character();
      }
    } else if (ch >= 32 && ch <= 255) {
      if (line_)
        line_->append(static_cast<char>(ch));
      else
        input_.append(static_cast<char>(ch));
    }
    render();
    return {};
  }
  void progress(const Json &event) {
    if (event.value("type", "") == "item.started" && event.contains("item") &&
        event["item"].contains("tool"))
      say("[" + event["item"]["tool"].get<std::string>() + "]");
    const auto type = event.value("type", "");
    if (type == "item.completed" && event.contains("item")) {
      const auto &item = event.at("item");
      const auto phase = item.value("phase", "");
      if ((phase == "commentary" || phase == "partial_answer") &&
          !item.value("final_result", false) && !item.value("text", "").empty())
        say(item.at("text").get<std::string>());
    }
    if (type == "turn.completed" || type == "turn.failed") {
      progress_.clear();
      render();
    }
    if (curses_ && type == "item.updated") {
      progress_ = "Responding: " + detail::terminal_text(event["item"].value("text_delta", ""));
      progress_ = detail::CursesText(progress_, curses_unicode_, true, false).text();
      render();
    }
  }
};
} // namespace
int cli_main(const std::vector<std::string> &args, Cancellation &cancel) {
  detail::IoContext io(cancel);
  return detail::cli_main(args, cancel, io);
}
int detail::cli_main(const std::vector<std::string> &args, Cancellation &cancel, IoContext &io) {
  return detail::cli_main(args, cancel, io, CursesDrawOps{});
}
int detail::cli_main(const std::vector<std::string> &args, Cancellation &cancel, IoContext &io,
                     const CursesDrawOps &drawing) {
  detail::DiagnosticWriter errors(std::cerr, &io);
  detail::reset_standard_diagnostics();
  const auto o = parse(args);
  detail::CliOutput output(io);
  if (o.help) {
    output.required({help}, false);
    return 0;
  }
  if (o.version) {
    output.required({"codet ", CODET_VERSION});
    return 0;
  }
  if (!o.images.empty() && !o.command.empty() && o.command != "exec" && o.command != "resume" &&
      o.command != "fork")
    throw Error("--image requires a conversation invocation", 2);
  NativeFileSystem files;
  SystemClock clock;
  CurlTransport http;
  std::shared_ptr<Interaction> interaction;
  const auto terminal_thread = std::this_thread::get_id();
  // Keep session ownership until background-process destruction has finished.
  std::shared_ptr<void> lifecycle_owner, current_owner;
  auto diagnostics = std::make_shared<detail::CliDiagnostics>(std::cerr, &io);
  const DiagnosticSink main_diagnostic = [diagnostics](const std::string &text) {
    diagnostics->emit_rendered({text});
  };
  DiagnosticSink diagnostic = [&](const std::string &text) {
    if (interaction && std::this_thread::get_id() != terminal_thread) {
      try {
        interaction->post({{"type", "codet.diagnostic"}, {"text", text}});
      } catch (const std::bad_alloc &) {
        throw;
      } catch (...) {
        interaction->warn(Interaction::Warning::event_render);
      }
    } else
      detail::context_diagnostic(main_diagnostic, errors, {text});
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
  setting("durability", o.durability);
  Config config = load_config(home, o.cwd, o.profile, overrides, files);
  auto allowed = config.rules().allow, denied = config.rules().deny;
  allowed.insert(allowed.end(), o.allow.begin(), o.allow.end());
  denied.insert(denied.end(), o.deny.begin(), o.deny.end());
  config.set_rules(std::move(allowed), std::move(denied));
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
      output.required({Json({{"credentials_present", present},
                             {"subscription_imported", auth.contains("codex")}})
                           .dump()});
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
    (void)errors.write({"Credentials stored privately in Codet state"});
    return 0;
  }
  if (o.command == "doctor") {
    NativeProcesses processes;
    output.required(
        {Json({{"version", CODET_VERSION},
               {"pointer_bits", sizeof(void *) * 8},
               {"provider", config.provider},
               {"durability", config.durability},
               {"cwd", config.cwd.string()},
               {"state", config.home.string()},
               {"os_sandbox", false},
               {"application_permissions", true},
               {"target_runtime_verified", false},
               {"capabilities", Tools(config, files, processes, http, clock).capabilities()}})
             .dump(2)});
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
    config.artifact_root = fs::path(p) / "artifacts";
  }
  // Destroy processes/capture handles before removing the private invocation directory.
  NativeProcesses processes;
  processes.set_diagnostics(diagnostic);
  config.state_root = state_home;
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
  if (!schema.is_null()) {
    nlohmann::json_schema::schema_loader loader = [](const nlohmann::json_uri &, Json &) {
      throw Error("External schema references are disabled", 2);
    };
    try {
      if (schema.value("type", "") != "object")
        throw Error("Output schema must describe an object", 2);
      const auto draft = schema.value("$schema", "http://json-schema.org/draft-07/schema#");
      if (draft != "http://json-schema.org/draft-07/schema#" &&
          draft != "https://json-schema.org/draft-07/schema")
        throw Error("Only JSON Schema Draft 7 is supported", 2);
      nlohmann::json_schema::json_validator validator(loader);
      validator.set_root_schema(schema);
    } catch (const std::exception &e) {
      throw Error(std::string("Invalid output schema: ") + e.what(), 2);
    }
  }
  std::string prompt = combined(o.positionals);
  const bool will_run = o.command != "sessions" && o.command != "export" && o.command != "repair";
  const bool headless = o.exec || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO);
  if (will_run && headless && !isatty(STDIN_FILENO)) {
    auto input = stdin_text();
    if (prompt.empty() || prompt == "-")
      prompt = input;
    else if (!input.empty())
      prompt += "\n\nAdditional stdin context:\n" + input;
  }
  if (o.command == "memory" && o.action != "consolidate")
    throw Error("Use memory consolidate", 2);
  if (will_run && headless && prompt.empty() && o.images.empty() && o.command != "fork" &&
      o.command != "memory")
    throw Error("Headless mode requires a prompt, stdin or an image", 2);
  const auto provider_preflight = [&] {
    Tools preflight(config, files, processes, http, clock);
    if (!prompt.empty())
      (void)preflight.expand_command(prompt);
    ApiProvider check(http, credentials, clock);
    Json auth = Json::object();
    if (config.provider == "codex") {
      const auto saved = credentials.load();
      if (!saved.contains("codex"))
        throw Error("No imported ChatGPT credentials. Use codet login --import FILE");
      auth = saved.at("codex");
    }
    (void)check.request(config, "", {}, preflight.schemas(), auth);
  };
  if (will_run && o.command != "resume" && o.command != "fork")
    provider_preflight();
  StateStore state(files, state_home, artifact_directory(config), config.durability,
                   {&cancel, diagnostic});
  state.set_diagnostics(diagnostic);
  diagnostics->install(state);
  if (o.command == "sessions") {
    output.required({state.list().dump(2)});
    return 0;
  }
  if (o.command == "repair") {
    if (o.repair_token) {
      output.required({state.repair_session_token(o.session, o.last).dump(2)});
      return 0;
    }
    Tools inspection(config, files, processes, http, clock);
    output.required(
        {state.repair(o.session, o.last, o.repair_provider, o.repair_recovery, inspection)
             .dump(2)});
    return 0;
  }
  Session session;
  if (o.command == "resume" || o.command == "fork" || o.command == "export") {
    if (o.last)
      session = state.last(o.command == "export");
    else if (!o.session.empty())
      session = state.load(o.session, o.command == "export");
    else {
      if (o.exec || !isatty(STDIN_FILENO))
        throw Error("Select a session id or --last", 2);
      (void)errors.write({state.list().dump(2), "\nSession id: "}, false);
      std::string id;
      std::getline(std::cin, id);
      session = state.load(id, o.command == "export");
    }
    if (o.command == "export") {
      if (o.format == "text") {
        detail::TextExport exported(output);
        const bool human = isatty(STDOUT_FILENO);
        const auto append = [&](const std::string &text) {
          if (human)
            exported.append_human(text);
          else
            exported.append(text);
        };
        for (const auto &m : session.messages) {
          append(m.role);
          exported.append(": ");
          append(m.text);
          exported.append("\n\n");
        }
        exported.finish();
      } else
        output.required(
            {Json({{"id", session.id}, {"name", session.name}, {"messages", session.messages}})
                 .dump(2)});
      return 0;
    }
  }
  if (o.command == "resume") {
    lifecycle_owner = state.acquire(session.id);
    session = state.load_id(session.id, false);
  }
  // Validate candidate selection and explicit attachments before publishing a new session/fork.
  const auto candidate = resolve_selection(
      config, session.id.empty() ? std::nullopt
                                 : std::optional<Selection>{{session.provider, session.model}});
  config.provider = candidate.provider;
  config.model = candidate.model;
  const auto inherited_effort = config.effort;
  config.effort = resolve_effort(config, session.id.empty() ? nullptr : &session);
  validate_effort(config);
  const auto selected_effort = saved_effort(config, session.id.empty() ? nullptr : &session);
  auto pending_images = input_images(config, files, *native_images(), o.images, cancel);
  if (o.command == "resume" || o.command == "fork")
    provider_preflight();
  if (o.command == "fork")
    session = state.fork(session);
  else if (session.id.empty())
    session = state.create();
  lifecycle_owner = state.acquire(session.id);
  current_owner = lifecycle_owner;
  session = state.load_id(session.id, false);
  state.reconcile(session);
  session.effort = selected_effort;
  session.model = config.model;
  session.provider = config.provider;
  state.update_metadata(session);
  const bool interactive = !o.exec && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
  std::shared_ptr<Terminal> terminal;
  diagnostics->live = DiagnosticBuffer(config.tool_output_limit);
  diagnostics->cleanup = DiagnosticBuffer(config.tool_output_limit);
  detail::TerminalDiagnosticsScope<Terminal> restore_diagnostics{terminal, diagnostics, state};
  if (interactive) {
    restore_diagnostics.activate(
        std::make_shared<Terminal>(config, o.color, output, errors, drawing, io));
    terminal->set_mode(config.permission_mode);
  }
  interaction = terminal ? std::make_shared<Interaction>() : std::shared_ptr<Interaction>{};
  Approver approver;
  Questioner questioner;
  if (terminal)
    approver = [&](const PermissionRequest &request) {
      const auto answer = interaction->request(
          "codet.approval",
          {{"tool", request.tool}, {"target", request.target}, {"context", request.context}},
          cancel);
      return detail::approves(answer);
    };
  if (terminal)
    questioner = [&](const std::string &question, const std::vector<std::string> &options) {
      return interaction->request("codet.question", {{"question", question}, {"options", options}},
                                  cancel);
    };
  Tools tools(config, files, processes, http, clock, approver, questioner, diagnostic, interaction);
  ApiProvider provider(http, credentials, clock);
  auto events = [&](const Json &event) {
    if (terminal)
      interaction->post(event);
    else if (o.format == "jsonl") {
      if (event.value("type", "") == "turn.failed") {
        output.secondary_record(
            [&] { return event.dump(-1, ' ', false, Json::error_handler_t::replace); });
      } else
        output.required({event.dump(-1, ' ', false, Json::error_handler_t::replace)});
    } else if (event.value("type", "") == "item.completed" &&
               !event["item"].value("final_result", false) &&
               (event["item"].value("phase", "") == "commentary" ||
                event["item"].value("phase", "") == "partial_answer"))
      (void)errors.write({detail::terminal_text(event["item"].value("text", ""))});
    else if (event.value("type", "") == "item.started" && event["item"].contains("tool"))
      (void)errors.write({"[", detail::terminal_text(event["item"]["tool"]), "]"});
  };
  Agent agent(config, provider, tools, state, events, diagnostic);
  struct SessionEnd {
    detail::DiagnosticWriter &errors;
    std::function<void()> end;
    ~SessionEnd() {
      try {
        end();
      } catch (const std::exception &error) {
        detail::secondary_diagnostic([&] {
          (void)errors.write(
              {"SessionEnd hook cleanup failed: ", detail::terminal_text(error.what())});
        });
      } catch (...) {
        detail::secondary_diagnostic(
            [&] { (void)errors.write({"SessionEnd hook cleanup failed: unknown exception"}); });
      }
    }
  } session_end{errors, [&, started_id = session.id] {
                  diagnostics->buffering = true;
                  detail::TerminalDiagnosticsScope<Terminal> restore{terminal, diagnostics, state};
                  try {
                    Cancellation cleanup_cancel;
                    tools.run_hooks("SessionEnd",
                                    {{"hook_event_name", "SessionEnd"}, {"session_id", started_id}},
                                    cleanup_cancel, 10000, &diagnostics->cleanup);
                  } catch (const std::exception &error) {
                    report_diagnostic(diagnostic, "SessionEnd hook cleanup failed: " +
                                                      std::string(error.what()));
                  } catch (...) {
                    report_diagnostic(diagnostic,
                                      "SessionEnd hook cleanup failed: unknown exception");
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
    state.append_messages(session, session.messages.size() - 1);
  }
  bool quit_requested = false;
  bool new_requested = false, compacting = false;
  std::optional<Usage> last_usage;
  auto execute_agent = [&](const std::string &text, bool steering) -> RunResult {
    if (!terminal)
      return compacting ? agent.compact(session, cancel)
                        : agent.run(session, text, cancel, schema, !steering, pending_images);
    interaction->begin_turn(session.id);
    std::future<RunResult> work;
    std::optional<Json> pending;
    const auto drain_warnings = [&] {
      interaction->drain_warnings([&](std::string_view text) {
        const std::string owned(text);
        diagnostics->emit_rendered({owned});
        return true;
      });
    };
    const auto deliver = [&](bool recovering = false) {
      struct DrainWarnings {
        const decltype(drain_warnings) &drain;
        ~DrainWarnings() {
          drain();
        }
      } warnings{drain_warnings};
      diagnostics->deliver_files();
      interaction->deliver(
          [&](const Json &event) {
            const auto type = event.value("type", "");
            if (type == "codet.diagnostic")
              diagnostics->emit_rendered({event.at("text").get_ref<const std::string &>()});
            else if (type == "codet.approval") {
              terminal->begin_approval(event);
              pending = event;
            } else if (type == "codet.question") {
              terminal->begin_request(event);
              terminal->say(event.at("question"));
              const auto options = event.at("options").get<std::vector<std::string>>();
              for (size_t i = 0; i < options.size(); ++i)
                terminal->say(std::to_string(i + 1) + ". " + options[i]);
              pending = event;
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
              terminal->say_literal(
                  "[Image: " + event.at("artifact").at("path").get<std::string>() + "]");
            else
              terminal->progress(event);
          },
          diagnostic, recovering);
    };
    try {
      terminal->begin_active();
      work = std::async(std::launch::async, [&] {
        return compacting ? agent.compact(session, cancel)
                          : agent.run(session, text, cancel, schema, !steering, pending_images);
      });
      while (work.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        deliver();
        if (pending && !interaction->request_active(pending->at("request_id"))) {
          pending.reset();
          terminal->end_approval();
          terminal->say("Request expired; any unfinished answer is discarded at Enter.");
        }
        terminal->flush_prompt();
        bool closed = false;
        const auto line = terminal->active_input(closed);
        if (closed) {
          if (pending && pending->value("type", "") == "codet.approval")
            quit_requested = true;
          cancel.requested = true;
        }
        if (line) {
          const auto &text = line->text;
          const auto submission = line->classify(pending ? &*pending : nullptr);
          const auto action = submission.action;
          if (action == detail::InputAction::quit) {
            quit_requested = true;
            cancel.requested = true;
          } else if (action == detail::InputAction::discard) {
            terminal->discarded_line(*submission.rejection);
          } else if (text == "/model" || text.rfind("/model ", 0) == 0 || text == "/status" ||
                     text == "/compact") {
            terminal->say("Use /model, /status and /compact after the active turn completes.");
          } else if (action == detail::InputAction::answer) {
            if (pending->value("type", "") == "codet.approval" && detail::approves(text) &&
                !terminal->approval_reviewed(*pending))
              continue;
            const bool answered = interaction->answer(pending->at("request_id"), text);
            pending.reset();
            terminal->end_approval(answered ? std::optional<std::string_view>(text) : std::nullopt);
            if (!answered)
              terminal->discarded_line(detail::InputDiscard::expired);
          } else if (text.rfind("/permissions", 0) == 0)
            terminal->say("Change permission modes after the active turn completes.");
          else if (text == "/new") {
            new_requested = true;
            terminal->say("Starting a new session after pending messages are handled.");
          } else if (!text.empty()) {
            try {
              interaction->submit(text);
            } catch (const Error &error) {
              terminal->say(error.what());
            }
          }
        }
      }
      deliver();
      terminal->end_active();
      return work.get();
    } catch (...) {
      detail::recover_turn(
          std::current_exception(), cancel, quit_requested, work,
          [&] {
            pending.reset();
            terminal->end_active();
          },
          [&] { deliver(true); }, [&](std::string_view message) { (void)errors.write({message}); });
    }
  };
  std::deque<std::string> followups;
  auto pending_input = [&](bool resume) {
    if (!interaction)
      return;
    // Agent closes admission before its future becomes ready; close also covers early errors.
    interaction->end_turn();
    if (!resume) {
      for (const auto &text : followups)
        terminal->say("[Pending message not sent: " + text + "]");
      followups.clear();
    }
    for (const auto &text : interaction->take_pending(session.id)) {
      if (resume && !quit_requested)
        followups.push_back(text);
      else
        terminal->say("[Pending message not sent: " + text + "]");
    }
  };
  auto run = [&](const std::string &text, bool steering = false) {
    struct DeliverFiles {
      detail::CliDiagnostics &diagnostics;
      ~DeliverFiles() {
        diagnostics.deliver_files();
      }
    } deliver_files{*diagnostics};
    struct ClearImages {
      std::vector<ImageArtifact> &images;
      bool clear;
      ~ClearImages() {
        if (clear)
          images.clear();
      }
    } clear_images{pending_images, !compacting};
    try {
      auto result = execute_agent(text, steering);
      pending_input(true);
      if (!compacting)
        last_usage = result.usage;
      const auto answer = result.structured.is_null() ? result.text : result.structured.dump();
      if (!o.output.empty() && !compacting)
        files.atomic_write(fs::absolute(o.output), answer + "\n", true);
      if (terminal)
        terminal->say(answer);
      else if (o.format == "text") {
        output.answer(answer, isatty(STDOUT_FILENO));
      } else if (o.format == "json")
        output.required({Json({{"session_id", session.id},
                               {"status", "success"},
                               {"result", result.text},
                               {"structured_output", result.structured},
                               {"usage",
                                {{"input_tokens", result.usage.input},
                                 {"output_tokens", result.usage.output},
                                 {"cached_input_tokens", result.usage.cached}}}})
                             .dump()});
      return 0;
    } catch (const Error &e) {
      if (terminal) {
        pending_input(false);
        terminal->say(std::string("Error: ") + e.what());
        if (const auto *raw = dynamic_cast<const ProviderError *>(&e))
          terminal->say(raw->body);
        cancel.requested = false;
        return e.code;
      }
      if (o.format == "json") {
        output.secondary_record([&] {
          Json failed = {{"session_id", session.id}, {"status", "error"}, {"error", e.what()}};
          if (const auto *raw = dynamic_cast<const ProviderError *>(&e))
            failed["provider_error"] = raw->details();
          if (dynamic_cast<const HookFailure *>(&e))
            failed["hook_failure"] = true;
          return failed.dump();
        });
      }
      throw;
    } catch (const std::exception &e) {
      if (terminal) {
        pending_input(false);
        terminal->say(std::string("Error: ") + e.what());
        if (!quit_requested)
          cancel.requested = false;
        return 1;
      }
      throw Error(safe_text(e.what()));
    }
  };
  if (o.command == "memory") {
    if (o.action != "consolidate")
      throw Error("Use memory consolidate", 2);
    config.roots.push_back(config.home / "memory");
    const auto consolidation_input = prompt;
    prompt = "Explicit memory consolidation: use the memory preview and apply_patch to consolidate "
             "Markdown files in the Codet "
             "memory directory, consolidate accurate "
             "useful project facts and feedback, remove contradictions by updating files, preserve "
             "provenance, and report what changed. Do not perform unrelated work.";
    if (!consolidation_input.empty())
      prompt += "\n\nAdditional consolidation instructions:\n" + consolidation_input;
  }
  if (!interactive) {

    if (prompt.empty() && pending_images.empty() && o.command == "fork") {
      output.required({Json({{"type", "thread.started"}, {"thread_id", session.id}}).dump()});
      return 0;
    }
    if (prompt.empty() && pending_images.empty())
      throw Error("Headless mode requires a prompt, stdin or an image", 2);
    return run(prompt);
  }
  if (!prompt.empty() || !pending_images.empty())
    run(prompt);
  if (quit_requested)
    return output.status(0);
  while (!cancel.requested && !quit_requested) {
    std::optional<std::string> input;
    if (!followups.empty()) {
      input = followups.front();
      followups.pop_front();
      run(*input, true);
      continue;
    } else if (new_requested) {
      input = "/new";
      new_requested = false;
    } else
      input = terminal->read(cancel);
    if (!input)
      break;
    if (input->empty())
      continue;
    if (*input == "/quit" || *input == "/exit")
      break;
    if (*input == "/help") {
      terminal->say("/help /quit /model [MODEL [EFFORT]] /status /compact /permissions [MODE] "
                    "/sessions /new /name TEXT /export /memory "
                    "consolidate; custom "
                    "commands from commands/*.md");
      continue;
    }
    if (*input == "/model" || input->rfind("/model ", 0) == 0) {
      if (*input == "/model") {
        terminal->say("Current: " + config.provider + "/" + config.model +
                      "; effort: " + (config.effort.empty() ? "provider default" : config.effort));
        if (config.provider == "codex")
          terminal->say(model_catalog().dump(2));
        terminal->say("Use /model MODEL [EFFORT]. Bundled models are reference entries; account "
                      "availability varies.");
      } else {
        try {
          std::istringstream words(input->substr(7));
          std::string model, effort, extra;
          words >> model;
          const bool explicit_effort = bool(words >> effort);
          if (model.empty() || (words >> extra))
            throw Error("Use /model MODEL [EFFORT]", 2);
          Config next = config;
          next.model = model;
          next.selected_effort =
              explicit_effort ? std::optional<std::string>{effort} : std::nullopt;
          next.effort = explicit_effort ? effort : inherited_effort;
          if (!explicit_effort && next.effort.empty() && next.provider == "codex")
            for (const auto &entry : model_catalog())
              if (entry.at("model") == model)
                next.effort = entry.at("default_effort").get<std::string>();
          validate_effort(next);
          const auto previous_model = session.model;
          const auto previous_effort = session.effort;
          session.model = next.model;
          session.effort = saved_effort(next);
          try {
            state.update_metadata(session);
          } catch (...) {
            session.model = previous_model;
            session.effort = previous_effort;
            throw;
          }
          config = std::move(next);
          terminal->say("Model: " + config.model + "; effort: " +
                        (config.effort.empty() ? "provider default" : config.effort));
        } catch (const std::exception &error) {
          terminal->say(std::string("Error: ") + error.what());
        }
      }
      continue;
    }
    if (*input == "/status") {
      Json status = {{"session_id", session.id},
                     {"provider", config.provider},
                     {"durability", config.durability},
                     {"model", config.model},
                     {"reasoning_effort", config.effort},
                     {"permission_mode", config.permission_mode},
                     {"context_tokens", config.context_tokens}};
      if (last_usage)
        status["last_turn_usage_this_invocation"] = {
            {"input_tokens", last_usage->input},
            {"output_tokens", last_usage->output},
            {"cached_input_tokens", last_usage->cached},
            {"reasoning_output_tokens", last_usage->reasoning}};
      else
        status["last_turn_usage_this_invocation"] = nullptr;
      status["durability"] = config.durability;
      terminal->say(status.dump(2));
      continue;
    }
    if (*input == "/compact") {
      compacting = true;
      run("");
      compacting = false;
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
      fresh.provider = config.provider;
      fresh.model = config.model;
      fresh.effort = saved_effort(config, &session);
      state.update_metadata(fresh);
      session = std::move(fresh);
      last_usage.reset();
      current_owner = std::move(fresh_owner);
      terminal->say_literal("Started " + session.id);
      continue;
    }
    if (input->rfind("/name ", 0) == 0) {
      session.name = input->substr(6);
      state.update_metadata(session);
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
  return output.status(quit_requested ? 0 : cancel.requested ? 130 : 0);
}
} // namespace codet
