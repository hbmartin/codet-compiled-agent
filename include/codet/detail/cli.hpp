#pragma once
#include <string>
#include <vector>

namespace codet {
struct Cancellation;
namespace detail {
struct IoContext;
struct CursesDrawOps;
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &);
} // namespace detail
} // namespace codet
