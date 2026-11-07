#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace codet {
struct Cancellation;
class Interaction;
class ImageBackend;
namespace detail {
struct IoContext;
struct CursesDrawOps;
// Optional internal observation seam; callbacks never grant input ownership or throw.
struct CliLifecycleOps {
  void (*turn_completed)(void *) noexcept = nullptr;
  void *context = nullptr;
  void (*preparing)(void *, std::string_view, unsigned) noexcept = nullptr;
  void (*worker_returned)(void *) noexcept = nullptr;
};
struct CliServices {
  ImageBackend *images = nullptr;
  void (*turn_started)(void *, Interaction &) = nullptr;
  void *context = nullptr;
};
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &,
             const CliLifecycleOps &);
int cli_main(const std::vector<std::string> &, Cancellation &, IoContext &, const CursesDrawOps &,
             const CliLifecycleOps &, const CliServices &);
} // namespace detail
} // namespace codet
