#pragma once
#include "codet/diagnostics.hpp"
#include <iostream>

namespace codet::detail {
class CliDiagnosticSurface {
public:
  virtual ~CliDiagnosticSurface() = default;
  virtual bool fullscreen() const = 0;
  virtual void diagnostic(const std::string &) = 0;
  virtual void diagnostic_rendered(RenderedTextView text) {
    diagnostic(text.text);
  }
};

class CliDiagnostics {
  std::shared_ptr<DiagnosticWriter> writer_;
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
      buffer.flush_to(*writer_);
    } catch (...) {
      // Each ordinary replay gets one independent attempt; notices must still be delivered.
      buffer.clear();
    }
  }

public:
  bool buffering = false;
  DiagnosticBuffer live{65536}, cleanup{65536};
  std::shared_ptr<FileDiagnosticQueue> files = std::make_shared<FileDiagnosticQueue>();
  explicit CliDiagnostics(std::ostream &output = std::cerr, IoContext *io = nullptr)
      : writer_(std::make_shared<DiagnosticWriter>(output, io)),
        immediate_(std::make_shared<Immediate>(
            [writer = writer_](const auto &text) { writer->file(text); })),
        file_sink_([this](const std::string &text) {
          if (auto terminal = screen_.lock(); terminal && terminal->fullscreen())
            terminal->diagnostic_rendered({text});
          else
            writer_->file(text);
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
    if (buffering) {
      cleanup.add(text);
      return;
    }
    const auto shown = terminal_text(text);
    emit_rendered({shown});
  }
  void emit_rendered(RenderedTextView text) {
    if (buffering)
      cleanup.add_rendered(text);
    else if (auto terminal = screen_.lock()) {
      terminal->diagnostic_rendered(text);
      if (terminal->fullscreen())
        live.add_rendered(text);
    } else
      (void)writer_->write({text.text});
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
