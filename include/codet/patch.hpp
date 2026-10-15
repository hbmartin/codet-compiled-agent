#pragma once
#include "codet/core.hpp"
namespace codet {
// Consume update chunks from the Codex envelope, matching against the original file.
std::string patch_update(const std::string &, const std::vector<std::string> &, size_t &,
                         const Cancellation &);
} // namespace codet
