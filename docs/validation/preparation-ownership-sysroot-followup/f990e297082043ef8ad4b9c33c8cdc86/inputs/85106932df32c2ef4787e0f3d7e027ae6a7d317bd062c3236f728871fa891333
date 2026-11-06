#pragma once
#include <cstddef>
namespace codet::test {
struct StaticLifetimeMetrics {
  bool observe = false, overflow = false;
  const void *arguments[64]{};
  size_t count = 0;
  volatile unsigned destructions = 0;
  bool registered(const void *object) const noexcept {
    for (size_t i = 0; i < count; ++i)
      if (arguments[i] == object)
        return true;
    return false;
  }
};
extern StaticLifetimeMetrics static_lifetime_metrics;
} // namespace codet::test
