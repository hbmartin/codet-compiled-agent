#pragma once
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <string_view>

namespace codet::detail {
class DiagnosticWriter {
  std::ostream &output_;
  bool separator_ = false;
  bool &pending_;

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
bool write_diagnostic(std::ostream &, std::initializer_list<std::string_view>, bool newline = true,
                      bool flush = false);
} // namespace codet::detail
