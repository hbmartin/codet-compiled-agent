#pragma once
#include "codet/diagnostics.hpp"
#include <iostream>

namespace codet::detail {
class CliDiagnosticSurface {
public:
  virtual ~CliDiagnosticSurface() = default;
  virtual bool fullscreen() const = 0;
  virtual void diagnostic(const std::string &) = 0;
};

class CliDiagnostics {
  std::ostream &output_;
  std::weak_ptr<CliDiagnosticSurface> screen_;
  class Immediate final : public FileDiagnosticReceiver {
    DiagnosticSink sink_;

  public:
    explicit Immediate(DiagnosticSink sink) : sink_(std::move(sink)) {}
    void submit(std::unique_ptr<FileNotice> notice) noexcept override {
      deliver_file_diagnostic(*notice, sink_);
    }
  };
  std::shared_ptr<FileDiagnosticReceiver> immediate_;
  DiagnosticSink file_sink_;
  void flush(DiagnosticBuffer &buffer) noexcept {
    try {
      if (!recover_diagnostic_stream(output_)) {
        buffer.clear();
        return;
      }
      buffer.flush(output_);
    } catch (...) {
      // Each ordinary replay gets one independent attempt; notices must still be delivered.
      buffer.clear();
    }
  }

public:
  bool buffering = false;
  DiagnosticBuffer live{65536}, cleanup{65536};
  std::shared_ptr<FileDiagnosticQueue> files = std::make_shared<FileDiagnosticQueue>();
  explicit CliDiagnostics(std::ostream &output = std::cerr)
      : output_(output),
        immediate_(std::make_shared<Immediate>(
            [stream = &output](const auto &text) { write_file_diagnostic(*stream, text); })),
        file_sink_([this](const std::string &text) {
          if (auto terminal = screen_.lock(); terminal && terminal->fullscreen())
            terminal->diagnostic(text);
          else
            write_file_diagnostic(output_, text);
        }) {}
  CliDiagnostics(const CliDiagnostics &) = delete;
  CliDiagnostics &operator=(const CliDiagnostics &) = delete;
  void install(StateStore &state) noexcept {
    state.set_file_diagnostics(immediate_);
  }
  void activate(StateStore &state, const std::shared_ptr<CliDiagnosticSurface> &screen) noexcept {
    screen_ = screen;
    state.set_file_diagnostics(files);
  }
  void emit(const std::string &text) {
    if (buffering)
      cleanup.add(text);
    else if (auto terminal = screen_.lock()) {
      terminal->diagnostic(text);
      if (terminal->fullscreen())
        live.add(text);
    } else
      (void)write_diagnostic(output_, {text});
  }
  void deliver_files() noexcept {
    if (auto terminal = screen_.lock(); terminal && terminal->fullscreen())
      files->preview(file_sink_);
    else
      files->deliver(file_sink_);
  }
  void restored(StateStore &state) noexcept {
    screen_.reset();
    install(state);
    buffering = false;
    flush(live);
    flush(cleanup);
    files->finish(file_sink_);
  }
};

// Keep both scopes: early setup failures and SessionEnd hook cleanup have different lifetimes.
template <class Surface> class TerminalDiagnosticsScope {
  std::shared_ptr<Surface> &terminal_;
  std::shared_ptr<CliDiagnostics> diagnostics_;
  StateStore &state_;

public:
  TerminalDiagnosticsScope(std::shared_ptr<Surface> &terminal,
                           std::shared_ptr<CliDiagnostics> diagnostics, StateStore &state) noexcept
      : terminal_(terminal), diagnostics_(std::move(diagnostics)), state_(state) {}
  TerminalDiagnosticsScope(const TerminalDiagnosticsScope &) = delete;
  TerminalDiagnosticsScope &operator=(const TerminalDiagnosticsScope &) = delete;
  void activate(std::shared_ptr<Surface> terminal) noexcept {
    terminal_ = std::move(terminal);
    diagnostics_->activate(state_, terminal_);
  }
  ~TerminalDiagnosticsScope() {
    terminal_.reset();
    diagnostics_->restored(state_);
  }
};
} // namespace codet::detail
