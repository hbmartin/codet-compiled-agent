#pragma once
#include <string>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace codet {
std::string digest(const std::string &);
#ifdef __APPLE__
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
