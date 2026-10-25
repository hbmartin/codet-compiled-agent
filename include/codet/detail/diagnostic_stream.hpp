#pragma once
#include "codet/core.hpp"
#include <cstdio>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <string_view>

namespace codet::detail {
struct IoContext;
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &);
// Internal trust boundary: the referenced text has already been rendered for humans.
struct RenderedTextView {
  const std::string &text;
};
struct RenderedText {
  std::string text;
  bool truncated = false;
};
struct RenderProgress {
  size_t consumed = 0, written = 0;
};
enum class RenderMode { prose, literal };
RenderProgress terminal_text_into(std::string_view, char *, size_t,
                                  RenderMode = RenderMode::prose) noexcept;
RenderedText render_terminal_text(std::string_view, size_t, RenderMode = RenderMode::prose);
size_t rendered_prefix(std::string_view, size_t) noexcept;
struct DiagnosticState {
  std::streambuf *buffer = nullptr;
  bool pending = false;
  void reset(std::streambuf *destination) noexcept {
    buffer = destination;
    pending = false;
  }
  void bind(std::streambuf *destination) noexcept {
    if (buffer != destination)
      reset(destination);
  }
};
class DiagnosticWriter {
  std::ostream &output_;
  DiagnosticState local_;
  DiagnosticState *state_;
  FILE *file_;
  IoContext *io_;

public:
  explicit DiagnosticWriter(std::ostream &, IoContext * = nullptr) noexcept;
  DiagnosticWriter(const DiagnosticWriter &) = delete;
  DiagnosticWriter &operator=(const DiagnosticWriter &) = delete;
  bool write(std::initializer_list<std::string_view>, bool newline = true, bool flush = false);
  void file(const std::string &);
};
std::string terminal_text(const std::string &);
std::string literal_text(const std::string &);
// Recovery never replaces an output exception. Ordinary callers may ignore write failure.
bool recover_diagnostic_stream(std::ostream &) noexcept;
void reset_standard_diagnostics() noexcept;
bool write_diagnostic(std::ostream &, std::initializer_list<std::string_view>, bool newline = true,
                      bool flush = false);
} // namespace codet::detail
