#pragma once
#include <string>
#include <vector>

namespace codet {
struct Cancellation;
namespace detail {
struct IoContext;
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &);
} // namespace detail
} // namespace codet
