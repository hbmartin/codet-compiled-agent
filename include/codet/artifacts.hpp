#pragma once
#include "codet/core.hpp"

namespace codet {
fs::path artifact_index_path(const fs::path &root, const std::string &session);
// Publish the lossless index before replacing transcript history. Existing entries are immutable.
fs::path archive_image_history(const fs::path &root, FileSystem &, const std::string &session,
                               const std::vector<Message> &);
bool generated_image(const Config &, const FileSystem &, const fs::path &, bool verify = true);
} // namespace codet
