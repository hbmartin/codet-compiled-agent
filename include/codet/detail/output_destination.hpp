#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>

namespace codet::detail {
enum class DiagnosticResult : uint8_t;
struct PresentationToken {
  uint64_t binding = 0, generation = 0;
  bool valid = false;
};
struct PresentationReceipt {
  DiagnosticResult result;
  PresentationToken token{};
};
inline bool same_destination(const struct stat &a, const struct stat &b) noexcept {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
         (a.st_mode & S_IFMT) == (b.st_mode & S_IFMT) && a.st_rdev == b.st_rdev;
}
// Exactly the two currently reachable channels, never a registry of historical terminals.
// Aliased channels share their boundary even when one of them is subsequently rebound.
class LineDestinations {
  struct Channel {
    struct stat identity {};
    bool known = false, row = false, pending = false, fragment = false;
    uint64_t generation = 0;
    uint64_t binding = 0, failure = 0;
    size_t active = 0;
  };
  std::array<Channel, 2> channels_{};
  bool presented_ = false;
  uint64_t generation_ = 0;
  uint64_t binding_ = 0;
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
  struct Reservation {
    uint64_t binding = 0, failure = 0;
    bool admitted = false, separator = false, presentation = false;
    PresentationToken token{};
  };
  void bind(int descriptor, const struct stat &identity) noexcept {
    auto &channel = channels_[index(descriptor)];
    if (channel.known && same_destination(channel.identity, identity))
      return;
    Channel next;
    next.identity = identity;
    next.known = true;
    next.row = presented_ && S_ISCHR(identity.st_mode);
    next.generation = ++generation_;
    next.binding = ++binding_;
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
    for (size_t i = 0; i != channels_.size(); ++i)
      if (channels_[i].known && same_destination(channels_[i].identity, identity))
        return separate(i == 0 ? STDOUT_FILENO : STDERR_FILENO);
    return false;
  }
  bool separate(int descriptor) noexcept {
    bool result = false;
    related(descriptor, [&](auto &channel) {
      result = result || channel.row || channel.fragment;
      channel.row = channel.fragment = false;
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
  Reservation admit(int descriptor, bool presentation,
                    const PresentationToken *expected = nullptr) noexcept {
    auto &selected = channels_[index(descriptor)];
    if (!selected.binding)
      selected.binding = ++binding_;
    if (presentation && (selected.active ||
                         (expected && (!expected->valid || expected->binding != selected.binding ||
                                       expected->generation != selected.generation))))
      return {};
    Reservation result{selected.binding,
                       selected.failure,
                       true,
                       selected.pending || (presentation ? selected.fragment : selected.row),
                       presentation,
                       {selected.binding, selected.generation, selected.known}};
    const auto next = presentation ? selected.generation : ++generation_;
    related(descriptor, [&](auto &channel) {
      channel.pending = true;
      channel.row = presentation;
      channel.fragment = false;
      channel.generation = next;
      if (!presentation)
        ++channel.active;
    });
    return result;
  }
  void finish(const Reservation &record, bool success, bool newline) noexcept {
    if (!record.admitted)
      return;
    const auto next = record.presentation ? generation_ : ++generation_;
    for (auto &channel : channels_) {
      if (channel.binding != record.binding)
        continue;
      if (!record.presentation) {
        if (channel.active)
          --channel.active;
        channel.generation = next;
        channel.row = false;
      }
      if (!success) {
        channel.pending = true;
        channel.row = false;
        ++channel.failure;
      } else if (!channel.active && channel.failure == record.failure) {
        channel.pending = false;
        channel.fragment = !record.presentation && !newline;
        if (record.presentation)
          channel.row = !newline;
      }
    }
  }
  PresentationToken token(int descriptor, const struct stat &identity) const noexcept {
    const auto &channel = channels_[index(descriptor)];
    return {channel.binding, channel.generation,
            channel.known && same_destination(channel.identity, identity)};
  }
  size_t active(int descriptor) const noexcept {
    return channels_[index(descriptor)].active;
  }
  uint64_t generation(int descriptor) const noexcept {
    return channels_[index(descriptor)].generation;
  }
  void reset() noexcept {
    channels_ = {};
    presented_ = false;
    // Keep reset distinguishable from a completion belonging to an earlier invocation.
    const auto next = ++generation_;
    for (auto &channel : channels_)
      channel.generation = next;
  }
};
struct BoundarySnapshot {
  bool available = false, row = false;
  uint64_t generation = 0;
  size_t active = 0;
  PresentationToken token{};
};
// Null selects the synchronized native owner. No mutable native references escape its guard.
class BoundaryAccess {
  LineDestinations *local_;

public:
  explicit BoundaryAccess(LineDestinations *local = nullptr) noexcept : local_(local) {}
  void bind(int, const struct stat &) const noexcept;
  bool row(const struct stat &) const noexcept;
  void open(const struct stat &) const noexcept;
  bool separate(const struct stat &) const noexcept;
  BoundarySnapshot snapshot(int, const struct stat &) const noexcept;
};
BoundaryAccess native_line_destinations() noexcept;
// Stack-owned inspection uses the logical delivery's EINTR policy and never holds a state lock.
class OutputAttempt;
bool inspect_output_destination(int, struct stat &, OutputAttempt &) noexcept;
} // namespace codet::detail
