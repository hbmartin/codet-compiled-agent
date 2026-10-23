#pragma once
#include <initializer_list>
#include <iosfwd>
#include <string_view>

namespace codet::detail {
// Recovery never replaces an output exception. Ordinary callers may ignore write failure.
bool recover_diagnostic_stream(std::ostream &) noexcept;
bool write_diagnostic(std::ostream &, std::initializer_list<std::string_view>, bool newline = true,
                      bool flush = false);
} // namespace codet::detail
