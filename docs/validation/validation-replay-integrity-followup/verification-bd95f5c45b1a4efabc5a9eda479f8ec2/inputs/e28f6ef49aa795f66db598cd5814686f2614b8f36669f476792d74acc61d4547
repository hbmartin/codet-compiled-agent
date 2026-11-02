#pragma once
#include <string>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <limits>
#include <stdexcept>
#endif

namespace codet {
std::string digest(const std::string &);
#ifdef __APPLE__
inline CFIndex cf_count(size_t count) {
  if (count > static_cast<size_t>(std::numeric_limits<CFIndex>::max()))
    throw std::length_error("Core Foundation count exceeds CFIndex");
  return static_cast<CFIndex>(count);
}
template <class T> struct CF {
  T value;
  explicit CF(T v = nullptr) : value(v) {}
  ~CF() {
    if (value)
      CFRelease(value);
  }
  operator T() const {
    return value;
  }
  CF(const CF &) = delete;
  CF &operator=(const CF &) = delete;
};
#endif
} // namespace codet
