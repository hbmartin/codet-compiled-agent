#pragma once
#include "codet/core.hpp"
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#if defined(__APPLE__) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
#define CODET_TEST_DARWIN_ASAN_SPAWN 1
#include <spawn.h>
extern char **environ;
#endif
#endif
#endif

namespace codet::test {
extern std::string executable;
inline pid_t start_helper(const std::vector<std::string> &arguments,
                          const std::vector<int> &close_fds = {}) {
  std::vector<std::string> command{executable};
  command.insert(command.end(), arguments.begin(), arguments.end());
  std::vector<char *> argv;
  argv.reserve(command.size() + 1);
  for (auto &part : command)
    argv.push_back(part.data());
  argv.push_back(nullptr);
#ifdef CODET_TEST_DARWIN_ASAN_SPAWN
  // Apple's child-side atfork hooks can enter ASan's inherited StackStore lock. Spawn
  // keeps the fresh-process test while avoiding that host-only pre-exec runtime path.
  posix_spawn_file_actions_t actions;
  int error = ::posix_spawn_file_actions_init(&actions);
  if (error) {
    errno = error;
    return -1;
  }
  for (int fd : close_fds) {
    error = ::posix_spawn_file_actions_addclose(&actions, fd);
    if (error)
      break;
  }
  pid_t child = -1;
  if (!error)
    error = ::posix_spawn(&child, command.front().c_str(), &actions, nullptr, argv.data(), environ);
  (void)::posix_spawn_file_actions_destroy(&actions);
  if (error) {
    errno = error;
    return -1;
  }
  return child;
#else
  const auto child = ::fork();
  if (child == 0) {
    // Only async-signal-safe operations between fork and exec.
    for (int fd : close_fds)
      ::close(fd);
    ::execv(command.front().c_str(), argv.data());
    ::_exit(127);
  }
  return child;
#endif
}
inline bool read_token(int fd, char &token,
                       std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    struct pollfd descriptor {
      fd, POLLIN, 0
    };
    const auto ready = ::poll(&descriptor, 1, 100);
    if (ready > 0) {
      const auto count = ::read(fd, &token, 1);
      if (count < 0 && errno == EINTR)
        continue;
      return count == 1;
    }
    if (ready < 0 && errno != EINTR)
      return false;
  }
  return false;
}
struct HelperProcessOps {
  pid_t (*wait)(pid_t, int *, int) = &::waitpid;
  int (*signal)(pid_t, int) = &::kill;
};
inline bool wait_helper(pid_t child, int &status, const HelperProcessOps &ops = {}) {
  if (child <= 0)
    return false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto collected = ops.wait(child, &status, WNOHANG);
    if (collected == child)
      return true;
    if (collected < 0 && errno != EINTR)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ops.signal(child, SIGKILL);
  const auto cleanup = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < cleanup) {
    if (ops.wait(child, &status, WNOHANG) != 0)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}
} // namespace codet::test
