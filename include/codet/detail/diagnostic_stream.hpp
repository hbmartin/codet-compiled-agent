#pragma once
#include <cstdio>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <string_view>

namespace codet::detail {
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
  DiagnosticState &state_;
  FILE *file_;

public:
  explicit DiagnosticWriter(std::ostream &) noexcept;
  DiagnosticWriter(const DiagnosticWriter &) = delete;
  DiagnosticWriter &operator=(const DiagnosticWriter &) = delete;
  bool write(std::initializer_list<std::string_view>, bool newline = true, bool flush = false);
  void file(const std::string &);
};
std::string terminal_text(const std::string &);
// Recovery never replaces an output exception. Ordinary callers may ignore write failure.
bool recover_diagnostic_stream(std::ostream &) noexcept;
void reset_standard_diagnostics() noexcept;
bool write_diagnostic(std::ostream &, std::initializer_list<std::string_view>, bool newline = true,
                      bool flush = false);
} // namespace codet::detail
