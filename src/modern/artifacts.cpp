#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/support.hpp"
#include <limits>
#include <sstream>
#include <sys/stat.h>

namespace codet {
fs::path artifact_index_path(const fs::path &root, const std::string &session) {
  if (session.empty() || session.size() > 128 ||
      session.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos)
    throw Error("Invalid artifact index session");
  return root / "indexes" / (session + ".jsonl");
}
fs::path archive_image_history(const fs::path &root, FileSystem &files, const std::string &session,
                               const std::vector<Message> &messages) {
  auto target = artifact_index_path(root, session);
  const auto directory = target.parent_path();
  if (fs::is_symlink(root) || fs::is_symlink(directory) || fs::is_symlink(target))
    throw Error("Artifact index storage must not be a symlink");
  std::map<std::pair<std::string, std::string>, ImageArtifact> entries;
  const auto load = [&](const fs::path &path) {
    if (fs::is_symlink(path) || files.canonical(path.parent_path()) != files.canonical(directory))
      throw Error("Artifact index is outside active private storage");
    std::istringstream input(files.read(path, std::numeric_limits<size_t>::max()));
    std::string line;
    while (std::getline(input, line)) {
      if (line.empty())
        continue;
      try {
        const auto image = Json::parse(line).get<ImageArtifact>();
        entries.emplace(std::make_pair(image.path, image.sha256), image);
      } catch (const Json::exception &error) {
        throw Error("Invalid artifact index: " + std::string(error.what()));
      }
    }
  };
  if (fs::exists(target))
    load(target);
  for (const auto &message : messages) {
    if (!message.artifact_index.empty() && fs::path(message.artifact_index) != target)
      load(message.artifact_index);
    for (const auto &image : message.images)
      entries.emplace(std::make_pair(image.path, image.sha256), image);
  }
  if (entries.empty())
    return {};
  fs::create_directories(directory);
  if (chmod(root.c_str(), 0700) || chmod(directory.c_str(), 0700))
    throw Error("Cannot protect artifact index directory");
  std::string text;
  for (const auto &entry : entries)
    text += safe_json(Json(entry.second)).dump() + '\n';
  if (!fs::exists(target) || files.read(target, std::numeric_limits<size_t>::max()) != text)
    files.atomic_write(target, text, true);
  return target;
}
bool generated_image(const Config &config, const FileSystem &files, const fs::path &path,
                     bool verify) {
  const auto root = artifact_directory(config);
  if (fs::is_symlink(root) || fs::is_symlink(path) || !fs::is_regular_file(path) ||
      files.canonical(path.parent_path()) != files.canonical(root))
    return false;
  const auto hash = path.stem().string();
  if (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos ||
      (path.extension() != ".png" && path.extension() != ".jpg") ||
      fs::file_size(path) > image_file_limit)
    return false;
  return !verify || digest(files.read(path, image_file_limit)) == hash;
}
} // namespace codet
