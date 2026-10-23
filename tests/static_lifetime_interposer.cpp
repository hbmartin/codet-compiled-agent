// Host-only, fixed-storage observation of C++ static-destructor registration.
#include "static_lifetime_metrics.hpp"
#include <dlfcn.h>
namespace codet::test {
StaticLifetimeMetrics static_lifetime_metrics;
}
extern "C" int __cxa_atexit(void (*callback)(void *), void *argument, void *image) {
  using Register = int (*)(void (*)(void *), void *, void *);
  static const auto original = reinterpret_cast<Register>(::dlsym(RTLD_NEXT, "__cxa_atexit"));
  auto &metrics = codet::test::static_lifetime_metrics;
  if (metrics.observe) {
    if (metrics.count < sizeof(metrics.arguments) / sizeof(metrics.arguments[0]))
      metrics.arguments[metrics.count++] = argument;
    else
      metrics.overflow = true;
  }
  return original(callback, argument, image);
}
