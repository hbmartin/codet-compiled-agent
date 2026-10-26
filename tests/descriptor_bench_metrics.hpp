#pragma once
// Host benchmark counters. No observation state is compiled into production.
#include "native_symbol.hpp"
#include <atomic>
#include <cstdarg>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace codet::test::descriptor_bench {
inline bool observing = false;
inline std::atomic<size_t> stats{0}, flags{0}, bounds{0};
} // namespace codet::test::descriptor_bench
extern "C" int fstat(int descriptor, struct stat *status)
#ifdef __linux__
    noexcept
#endif
{
  using Stat = int (*)(int, struct stat *);
  static const auto native = reinterpret_cast<Stat>(codet::test::native_symbol("fstat"));
  if (codet::test::descriptor_bench::observing)
    ++codet::test::descriptor_bench::stats;
  return native(descriptor, status);
}
extern "C" long fpathconf(int descriptor, int name)
#ifdef __linux__
    noexcept
#endif
{
  using Bound = long (*)(int, int);
  static const auto native = reinterpret_cast<Bound>(codet::test::native_symbol("fpathconf"));
  if (codet::test::descriptor_bench::observing)
    ++codet::test::descriptor_bench::bounds;
  return native(descriptor, name);
}
extern "C" int fcntl(int descriptor, int command, ...)
#ifdef __linux__
    noexcept
#endif
{
  using Flags = int (*)(int, int, ...);
  static const auto native = reinterpret_cast<Flags>(codet::test::native_symbol("fcntl"));
  if (codet::test::descriptor_bench::observing)
    ++codet::test::descriptor_bench::flags;
  switch (command) {
  case F_GETFD:
  case F_GETFL:
  case F_GETOWN:
#ifdef F_GETPIPE_SZ
  case F_GETPIPE_SZ:
#endif
#ifdef F_FULLFSYNC
  case F_FULLFSYNC:
#endif
    return native(descriptor, command);
  default:
    break;
  }
  va_list arguments;
  va_start(arguments, command);
  int result;
  switch (command) {
  case F_DUPFD:
  case F_SETFD:
  case F_SETFL:
  case F_SETOWN:
#ifdef F_DUPFD_CLOEXEC
  case F_DUPFD_CLOEXEC:
#endif
#ifdef F_SETPIPE_SZ
  case F_SETPIPE_SZ:
#endif
#ifdef F_NOCACHE
  case F_NOCACHE:
#endif
    result = native(descriptor, command, va_arg(arguments, int));
    break;
  default:
    // Locking and platform commands used by these harnesses take pointer arguments.
    result = native(descriptor, command, va_arg(arguments, void *));
    break;
  }
  va_end(arguments);
  return result;
}
