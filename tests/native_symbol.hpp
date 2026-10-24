#pragma once
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

namespace codet::test {
inline void *native_symbol(const char *name) noexcept {
  if (auto *symbol = ::dlsym(RTLD_NEXT, name))
    return symbol;
  const auto emit = [](const char *text) noexcept {
    auto size = std::strlen(text);
    while (size) {
      const auto count = ::write(STDERR_FILENO, text, size);
      if (count < 0 && errno == EINTR)
        continue;
      if (count <= 0)
        return;
      text += count;
      size -= static_cast<size_t>(count);
    }
  };
  emit("Native test interposer cannot resolve ");
  emit(name);
  emit("\n");
  ::_exit(86);
}
} // namespace codet::test
