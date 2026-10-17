#pragma once
#include "codet/core.hpp"
namespace codet {
fs::path artifact_index_path(const fs::path &, const std::string &);
std::string artifact_index_reference(const std::string &);
ImageArtifact portable_image(ImageArtifact);
fs::path resolve_image(const fs::path &, const ImageArtifact &);
void portable_messages(std::vector<Message> &, const std::string &);
fs::path archive_image_history(const fs::path &, FileSystem &, const std::string &,
                               const std::vector<Message> &);
void copy_image_history(const fs::path &, FileSystem &, const std::string &, const std::string &);
Json image_history_position(const fs::path &, const FileSystem &, const std::string &);
bool generated_image(const Config &, const FileSystem &, const fs::path &, bool verify = true);
} // namespace codet
