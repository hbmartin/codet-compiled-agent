#pragma once
// The wrapper name and resolver must use the same struct-stat ABI as sys/stat.h.
#include "native_symbol.hpp"
#include <cerrno>
#include <sys/stat.h>

namespace codet::test {
bool interrupt_stat(int) noexcept;
}
#if defined(__GLIBC__)
#if !__GLIBC_PREREQ(2, 33)
#define CODET_VERSIONED_STAT 1
#endif
#endif
#ifdef CODET_VERSIONED_STAT
extern "C" int __fxstat(int version, int descriptor, struct stat *status) noexcept {
  using Stat = int (*)(int, int, struct stat *);
#ifdef __USE_FILE_OFFSET64
  constexpr auto name = "__fxstat64";
#else
  constexpr auto name = "__fxstat";
#endif
  static const auto native = reinterpret_cast<Stat>(codet::test::native_symbol(name));
  if (codet::test::interrupt_stat(descriptor)) {
    errno = EINTR;
    return -1;
  }
  return native(version, descriptor, status);
}
#else
extern "C" int fstat(int descriptor, struct stat *status)
#ifdef __linux__
    noexcept
#endif
{
  using Stat = int (*)(int, struct stat *);
#ifdef __APPLE__
  constexpr auto name = "fstat" __DARWIN_SUF_64_BIT_INO_T;
#elif defined(__USE_TIME_BITS64)
  constexpr auto name = "__fstat64_time64";
#elif defined(__USE_FILE_OFFSET64)
  constexpr auto name = "fstat64";
#else
  constexpr auto name = "fstat";
#endif
  static const auto native = reinterpret_cast<Stat>(codet::test::native_symbol(name));
  if (codet::test::interrupt_stat(descriptor)) {
    errno = EINTR;
    return -1;
  }
  return native(descriptor, status);
}
#endif
#undef CODET_VERSIONED_STAT
