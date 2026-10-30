#pragma once
#include <array>
#include <cstdint>
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
    bool known = false, row = false, pending = false;
    uint64_t generation = 0;
  };
  std::array<Channel, 2> channels_{};
  bool presented_ = false;
  uint64_t generation_ = 0;
  static size_t index(int descriptor) noexcept {
    return descriptor == STDOUT_FILENO ? 0u : 1u;
  }
  template <class Apply> void related(int descriptor, Apply apply) noexcept {
    auto &selected = channels_[index(descriptor)];
    for (auto &channel : channels_)
      if (&channel == &selected || (selected.known && channel.known &&
                                    same_destination(selected.identity, channel.identity)))
        apply(channel);
  }

public:
  void bind(int descriptor, const struct stat &identity) noexcept {
    auto &channel = channels_[index(descriptor)];
    if (channel.known && same_destination(channel.identity, identity))
      return;
    Channel next{identity, true, presented_ && S_ISCHR(identity.st_mode), false, ++generation_};
    for (const auto &other : channels_)
      if (&other != &channel && other.known && same_destination(other.identity, identity))
        next = other;
    channel = next;
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
  bool separate(int descriptor) noexcept {
    bool result = false;
    related(descriptor, [&](auto &channel) {
      result = result || channel.row;
      channel.row = false;
    });
    return result;
  }
  bool pending(int descriptor) const noexcept {
    return channels_[index(descriptor)].pending;
  }
  void pending(int descriptor, bool value) noexcept {
    related(descriptor, [&](auto &channel) { channel.pending = value; });
  }
  void interrupt(int descriptor) noexcept {
    const auto next = ++generation_;
    related(descriptor, [&](auto &channel) {
      channel.row = false;
      channel.generation = next;
    });
  }
  uint64_t generation(int descriptor) const noexcept {
    return channels_[index(descriptor)].generation;
  }
};
// Fixed, trivially destructible native owners; standalone writers and active UI share them.
LineDestinations &native_line_destinations() noexcept;
} // namespace codet::detail
