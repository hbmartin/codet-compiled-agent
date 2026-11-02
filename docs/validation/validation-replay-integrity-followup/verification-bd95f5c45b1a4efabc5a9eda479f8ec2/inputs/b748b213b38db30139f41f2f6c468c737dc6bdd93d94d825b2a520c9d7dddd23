#pragma once
#include "codet/core.hpp"
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

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
  const auto child = ::fork();
  if (child == 0) {
    // Only async-signal-safe operations between fork and exec.
    for (int fd : close_fds)
      ::close(fd);
    ::execv(command.front().c_str(), argv.data());
    ::_exit(127);
  }
  return child;
}
inline bool read_token(int fd, char &token) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    struct pollfd descriptor {
      fd, POLLIN, 0
    };
    const auto ready = ::poll(&descriptor, 1, 100);
    if (ready > 0)
      return ::read(fd, &token, 1) == 1;
    if (ready < 0 && errno != EINTR)
      return false;
  }
  return false;
}
inline bool wait_helper(pid_t child, int &status) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto collected = ::waitpid(child, &status, WNOHANG);
    if (collected == child)
      return true;
    if (collected < 0 && errno != EINTR)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ::kill(child, SIGKILL);
  const auto cleanup = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < cleanup) {
    if (::waitpid(child, &status, WNOHANG) != 0)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}
} // namespace codet::test
