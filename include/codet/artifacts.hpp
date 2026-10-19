#pragma once
#include "codet/core.hpp"
#include <cstdint>
namespace codet {
fs::path artifact_index_path(const fs::path &, const std::string &);
std::string artifact_index_reference(const std::string &);
ImageArtifact portable_image(ImageArtifact);
fs::path resolve_image(const fs::path &, const ImageArtifact &);
void portable_messages(std::vector<Message> &, const std::string &, const std::string &tool = "");
bool result_name(const fs::path &);
// Callers retain distinct trust policies; path and identity checks share one implementation.
enum class ArtifactPolicy : std::uint8_t {
  regular,
  file_or_directory,
  owned_regular,
  private_owned_regular
};
struct ReferenceObservation {
  std::string original, identifier;
  fs::path root, path;
  ArtifactPolicy policy = ArtifactPolicy::owned_regular;
  std::optional<std::string> identity;
  std::string error;
};
struct ExecutionObservation {
  Json facts = Json::array();
  std::map<std::string, ReferenceObservation> archives;
};
struct RecoveryCandidate {
  ReferenceObservation file;
  std::string content_digest;
  size_t bytes = 0;
};
std::string checked_artifact_identity(const FileSystem &, const fs::path &root, const fs::path &,
                                      ArtifactPolicy);
std::set<std::string> message_references(const std::vector<Message> &);
fs::path archive_image_history(const fs::path &, FileSystem &, const std::string &,
                               const std::vector<Message> &);
void copy_image_history(const fs::path &, FileSystem &, const std::string &, const std::string &);
Json image_history_position(const fs::path &, const FileSystem &, const std::string &);
bool generated_image(const Config &, const FileSystem &, const fs::path &, bool verify = true);
} // namespace codet
