#pragma once
#include <string>
#include <vector>

namespace codet {
struct Cancellation;
namespace detail {
struct IoContext;
struct CursesDrawOps;
// Optional internal observation seam; callbacks never grant input ownership or throw.
struct CliLifecycleOps {
  void (*turn_completed)(void *) noexcept = nullptr;
  void *context = nullptr;
};
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &,
             const CliLifecycleOps &);
} // namespace detail
} // namespace codet
