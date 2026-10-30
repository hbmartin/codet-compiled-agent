#pragma once
#include <array>
#include <sys/stat.h>
#include <unistd.h>

namespace codet::detail {
inline bool same_destination(const struct stat &a, const struct stat &b) noexcept {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
         (a.st_mode & S_IFMT) == (b.st_mode & S_IFMT) && a.st_rdev == b.st_rdev;
}
// Exactly the two currently reachable channels, never a registry of historical terminals.
// Aliased channels share their boundary even when one of them is subsequently rebound.
class LineDestinations {
  struct Channel {
    struct stat identity {};
    bool known = false, row = false;
  };
  std::array<Channel, 2> channels_{};
  bool presented_ = false;

public:
  void bind(int descriptor, const struct stat &identity) noexcept {
    auto &channel = channels_[descriptor == STDOUT_FILENO ? 0u : 1u];
    if (channel.known && same_destination(channel.identity, identity))
      return;
    bool row = presented_ && S_ISCHR(identity.st_mode);
    for (const auto &other : channels_)
      if (&other != &channel && other.known && same_destination(other.identity, identity))
        row = other.row;
    channel = {identity, true, row};
  }
  bool row(const struct stat &identity) const noexcept {
    for (const auto &channel : channels_)
      if (channel.known && same_destination(channel.identity, identity) && channel.row)
        return true;
    return false;
  }
  void open(const struct stat &identity) noexcept {
    presented_ = true;
    for (auto &channel : channels_)
      if (channel.known && same_destination(channel.identity, identity))
        channel.row = true;
  }
  bool separate(const struct stat &identity) noexcept {
    const bool pending = row(identity);
    for (auto &channel : channels_)
      if (channel.known && same_destination(channel.identity, identity))
        channel.row = false;
    return pending;
  }
};
} // namespace codet::detail
