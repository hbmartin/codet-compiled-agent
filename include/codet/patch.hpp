#pragma once
#include "codet/core.hpp"
namespace codet {
std::vector<std::string> patch_lines(const std::string &);
// Consume update chunks from the Codex envelope, matching against the original file.
std::string patch_update(const std::string &, const std::vector<std::string> &, size_t &,
                         const Cancellation &);
} // namespace codet
