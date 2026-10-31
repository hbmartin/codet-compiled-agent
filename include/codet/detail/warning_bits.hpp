#pragma once
#include <limits>

namespace codet::detail {
// One contiguous bank for new warnings, followed by an equally wide retry bank.
template <unsigned Kinds> struct WarningBits {
  static constexpr unsigned width() noexcept {
    unsigned result = 0;
    for (auto bits = Kinds; bits; bits >>= 1u)
      ++result;
    return result;
  }
  static constexpr unsigned kinds = Kinds, shift = width();
  static_assert(Kinds && (Kinds & (Kinds + 1u)) == 0, "Warning kinds must fill one bit bank");
  static_assert(shift <= std::numeric_limits<unsigned>::digits / 2,
                "Warning and retry banks must fit without overlap");
  static constexpr unsigned retries =
      shift <= std::numeric_limits<unsigned>::digits / 2 ? kinds << shift : 0;
  static_assert((kinds & retries) == 0, "Warning and retry bits overlap");
};
} // namespace codet::detail
