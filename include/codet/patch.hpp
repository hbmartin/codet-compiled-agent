#pragma once
#include "codet/core.hpp"
namespace codet {
// An empty source denotes a create; an empty destination denotes a delete.
// A distinct source and destination form one move throughout publication and rollback.
struct PatchChange {
  fs::path source, destination;
  std::string before_hash, after;
};
struct PatchFileOps {
  virtual ~PatchFileOps() = default;
  virtual void link_exclusive(const fs::path &from, const fs::path &to) const;
  virtual void replace(const fs::path &from, const fs::path &to) const;
  virtual void retire(const fs::path &from, const fs::path &private_name) const;
  virtual void sync_directory(const fs::path &) const;
  virtual void preserve_metadata(const fs::path &source, int source_fd, const fs::path &staged,
                                 int staged_fd, const Cancellation &) const;
};
void publish_native_patch(const std::vector<PatchChange> &, const Cancellation &,
                          const PatchFileOps &);
std::vector<std::string> patch_lines(const std::string &);
// Consume update chunks from the Codex envelope, matching against the original file.
std::string patch_update(const std::string &, const std::vector<std::string> &, size_t &,
                         const Cancellation &);
} // namespace codet
