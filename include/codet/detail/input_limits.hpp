#pragma once
#include <cstddef>
#include <string>
#include <string_view>
namespace codet::detail {
inline constexpr size_t raw_input_limit = 65536;
inline constexpr size_t expanded_input_limit = 262144;
inline constexpr size_t queued_input_limit = 8388608;
inline constexpr size_t retained_input_limit = 8388608;
inline constexpr size_t queued_input_count = 100;
size_t normalized_input_size(std::string_view, size_t limit, bool *repaired = nullptr);
std::string normalized_input(const std::string &, size_t limit);
// The caller owns unchanged bytes and the size/repair result from its preflight.
std::string normalized_input_known(const std::string &, size_t bytes, bool repaired);
} // namespace codet::detail
