#include "codet/artifacts.hpp"
#include "codet/computer.hpp"
#include "codet/output.hpp"
#include "codet/support.hpp"
#include <sys/stat.h>
#include <unistd.h>
namespace codet {
namespace {
bool blob_name(const ImageArtifact &a) {
  return a.sha256.size() == 64 &&
         a.sha256.find_first_not_of("0123456789abcdef") == std::string::npos &&
         fs::path(a.path).filename() == a.sha256 + (a.mime == "image/png" ? ".png" : ".jpg") &&
         (a.mime == "image/png" || a.mime == "image/jpeg");
}
void lines(ReadHandle &input, const std::function<void(const std::string &)> &visit) {
  std::string line;
  char buffer[8192];
  for (;;) {
    const auto n = input.read(buffer, sizeof buffer);
    if (!n)
      break;
    for (size_t i = 0; i < n; ++i) {
      if (buffer[i] == '\n') {
        if (!line.empty())
          visit(line);
        line.clear();
      } else {
        line += buffer[i];
        if (line.size() > 1048576)
          throw Error("Artifact index record exceeds 1 MiB");
      }
    }
  }
  if (!line.empty())
    visit(line);
}
void check_index(const fs::path &root, const fs::path &target) {
  if (fs::is_symlink(root) || fs::is_symlink(target.parent_path()) || fs::is_symlink(target))
    throw Error("Artifact index storage must not be a symlink");
}
bool current_index(const FileSystem &files, const fs::path &target) {
  auto input = files.open_read(target);
  if (input->path() != files.canonical(target))
    throw Error("Artifact index changed while opening");
  char buffer[512];
  const auto count = input->read(buffer, sizeof buffer);
  const std::string prefix(buffer, count);
  const auto first = Json::parse(prefix.substr(0, prefix.find('\n')), nullptr, false);
  if (!first.is_object() || first.value("kind", "") != "header")
    return false;
  if (first.value("version", 0) != 2)
    throw Error("Unsupported artifact index version; use a compatible Codet version");
  return true;
}
} // namespace
std::string artifact_index_reference(const std::string &session) {
  if (session.empty() || session.size() > 128 ||
      session.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos)
    throw Error("Invalid artifact index session");
  return "indexes/" + session + ".jsonl";
}
fs::path artifact_index_path(const fs::path &root, const std::string &session) {
  return root / artifact_index_reference(session);
}
ImageArtifact portable_image(ImageArtifact image) {
  if (blob_name(image))
    image.path = fs::path(image.path).filename().string();
  return image;
}
fs::path resolve_image(const fs::path &root, const ImageArtifact &image) {
  if (!blob_name(image) || (!fs::path(image.path).is_absolute() &&
                            fs::path(image.path) != fs::path(image.path).filename()))
    throw Error("Invalid generated image reference");
  return root / fs::path(image.path).filename();
}
bool result_name(const fs::path &ref) {
  const auto parts = std::vector<fs::path>(ref.begin(), ref.end());
  const auto identity = [](const std::string &value) {
    return !value.empty() && value.size() <= 128 &&
           value.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") ==
               std::string::npos;
  };
  return !ref.is_absolute() && parts.size() == 3 && parts[0] == "results" &&
         identity(parts[1].string()) && parts[2].extension() == ".json" &&
         identity(parts[2].stem().string());
}
std::string checked_artifact_identity(const FileSystem &files, const fs::path &root,
                                      const fs::path &path, ArtifactPolicy policy) {
  const auto base = fs::absolute(root).lexically_normal();
  const auto value = fs::absolute(path).lexically_normal();
  if (!within(value, base) || !within(files.canonical(value), files.canonical(base)) ||
      fs::is_symlink(base))
    throw Error("Artifact reference escapes trusted storage");
  auto current = base;
  for (const auto &part : value.lexically_relative(base)) {
    if (part == ".")
      continue;
    current /= part;
    if (fs::is_symlink(current))
      throw Error("Unsafe artifact symlink");
  }
  struct stat status{};
  if (::lstat(value.c_str(), &status) ||
      (!S_ISREG(status.st_mode) &&
       !(policy == ArtifactPolicy::file_or_directory && S_ISDIR(status.st_mode))))
    throw Error("Artifact is unavailable or not a regular file");
  if ((policy == ArtifactPolicy::owned_regular ||
       policy == ArtifactPolicy::private_owned_regular) &&
      status.st_uid != ::geteuid())
    throw Error("Artifact must be an owned regular file");
  if (policy == ArtifactPolicy::private_owned_regular && (status.st_mode & 0077))
    throw Error("Checkpoint recovery archive must be private (mode 0600)");
#if defined(__APPLE__)
  const auto modified = status.st_mtimespec, changed = status.st_ctimespec;
#else
  const auto modified = status.st_mtim, changed = status.st_ctim;
#endif
  return std::to_string(status.st_dev) + ":" + std::to_string(status.st_ino) + ":" +
         std::to_string(status.st_size) + ":" + std::to_string(status.st_uid) + ":" +
         std::to_string(status.st_mode) + ":" + std::to_string(modified.tv_sec) + ":" +
         std::to_string(modified.tv_nsec) + ":" + std::to_string(changed.tv_sec) + ":" +
         std::to_string(changed.tv_nsec);
}
fs::path remap_artifact_alias(const fs::path &value, const fs::path &root, const fs::path &anchor) {
  // Remap the trusted anchor only; subordinate symlinks must remain visible to validation.
  return value.is_absolute() && within(value, anchor) && !within(value, root)
             ? root / value.lexically_relative(anchor)
             : value;
}
bool normalize_checkpoint_reference(Message &message, const FileSystem &files, const fs::path &root,
                                    const fs::path &anchor) {
  if (!message.checkpoint_state.is_object())
    return false;
  auto ref = message.checkpoint_state.find("recovery_record");
  if (ref == message.checkpoint_state.end() || !ref->is_string())
    return false;
  const fs::path original(ref->get_ref<const std::string &>());
  if (!original.is_absolute())
    return false;
  try {
    const auto base = fs::absolute(root).lexically_normal();
    if (files.canonical(base) != anchor)
      return false;
    const auto path = remap_artifact_alias(original, base, anchor);
    const auto relative = path.lexically_relative(base);
    if (!result_name(relative))
      return false;
    const auto identity =
        checked_artifact_identity(files, base, path, ArtifactPolicy::private_owned_regular);
    auto input = files.open_read(path);
    if (input->path() != files.canonical(path) ||
        identity !=
            checked_artifact_identity(files, base, path, ArtifactPolicy::private_owned_regular))
      return false;
    *ref = relative.string();
    return true;
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    return false; // Preserve the original pointer for explicit uncertainty and repair.
  }
}
void cleanup_recovery_candidates(StateStore &state, Tools &tools,
                                 const std::vector<RecoveryCandidate> &candidates,
                                 const DiagnosticSink &diagnostics) noexcept {
  for (const auto &candidate : candidates) {
    const auto diagnose = [&](const char *reason) noexcept {
      try {
        report_diagnostic(diagnostics, "Cannot remove recovery candidate " +
                                           candidate.file.identifier + ": " + reason);
      } catch (...) {
        // A diagnostic failure must not stop the remaining candidates.
        return;
      }
    };
    try {
      state.discard_recovery_record(candidate, tools);
    } catch (const std::exception &error) {
      diagnose(error.what());
    } catch (...) {
      diagnose("Unknown cleanup failure");
    }
  }
}
std::set<std::string> message_references(const std::vector<Message> &messages) {
  std::set<std::string> out;
  const auto accept = [&](const std::string &value) {
    if (result_name(value))
      out.insert(value);
    else if (capture_name(value))
      out.insert(fs::path(value).filename().string());
  };
  for (const auto &m : messages) {
    accept(m.result_file);
    if (m.checkpoint_state.is_object() &&
        m.checkpoint_state.value("recovery_record", Json()).is_string())
      accept(m.checkpoint_state.at("recovery_record").get<std::string>());
    if (m.role != "tool")
      continue;
    const auto envelope = Json::parse(m.text, nullptr, false);
    if (!envelope.is_object())
      continue;
    const auto metadata = envelope.value("metadata", Json::object());
    if (metadata.is_object() && metadata.value("spill_path", Json()).is_string())
      accept(metadata.at("spill_path").get<std::string>());
  }
  return out;
}
void portable_messages(std::vector<Message> &messages, const std::string &session,
                       const std::string &tool) {
  std::map<std::string, std::string> names;
  for (const auto &message : messages)
    for (const auto &call : message.calls)
      names[call.id] = call.name;
  for (auto &message : messages) {
    for (auto &image : message.images)
      image = portable_image(std::move(image));
    if (!message.result_file.empty() && capture_name(message.result_file))
      message.result_file = fs::path(message.result_file).filename().string();
    if (message.role == "tool") {
      auto envelope = Json::parse(message.text, nullptr, false);
      const auto normalize = [](Json &value) {
        if (!value.is_object())
          return;
        if (value.value("spill_path", Json()).is_string() &&
            capture_name(value.at("spill_path").get<std::string>()))
          value["spill_path"] =
              fs::path(value.at("spill_path").get<std::string>()).filename().string();
        if (value.value("artifact", Json()).is_object()) {
          try {
            value["artifact"] = portable_image(value.at("artifact").get<ImageArtifact>());
          } catch (const Json::exception &) {
            return; // Preserve unrelated tool metadata without interpreting it as an image.
          }
        }
      };
      if (envelope.is_object()) {
        if (envelope.contains("metadata")) {
          normalize(envelope["metadata"]);
          if (result_name(message.result_file) && envelope["metadata"].is_object() &&
              envelope["metadata"].contains("result_path"))
            envelope["metadata"]["result_path"] = message.result_file;
        }
        const auto name = tool.empty() ? names[message.tool_call_id] : tool;
        const bool owned = name == "exec_command" || name == "write_stdin" ||
                           name == "view_image" || name.rfind("computer.", 0) == 0;
        if (owned && envelope.value("content", Json()).is_string()) {
          auto content = Json::parse(envelope.at("content").get<std::string>(), nullptr, false);
          if (content.is_object()) {
            normalize(content);
            if (result_name(message.result_file) && content.contains("result_path"))
              content["result_path"] = message.result_file;
            envelope["content"] = content.dump();
          }
        }
        message.text = envelope.dump();
      }
    }
    if (!message.artifact_index.empty()) {
      if (fs::path(message.artifact_index).filename() != session + ".jsonl")
        throw Error("Artifact index belongs to another session");
      message.artifact_index = artifact_index_reference(session);
    }
  }
}
fs::path archive_image_history(const fs::path &root, FileSystem &files, const std::string &session,
                               const std::vector<Message> &messages) {
  auto target = artifact_index_path(root, session);
  check_index(root, target);
  std::vector<Json> pending;
  std::set<std::string> ids;
  bool referenced = false;
  for (const auto &message : messages) {
    referenced = referenced || !message.artifact_index.empty();
    if (!message.artifact_index.empty() &&
        fs::path(message.artifact_index).filename() != target.filename())
      throw Error("Artifact index belongs to another session");
    for (size_t i = 0; i < message.images.size(); ++i) {
      if (message.message_id.empty())
        throw Error("Persist message IDs before archiving images");
      const auto id = message.message_id + ":" + std::to_string(i);
      if (!ids.insert(id).second)
        continue;
      pending.push_back({{"version", 2},
                         {"kind", "observation"},
                         {"observation_id", id},
                         {"message_id", message.message_id},
                         {"tool_call_id", message.tool_call_id},
                         {"artifact", portable_image(message.images[i])}});
    }
  }
  if (!fs::exists(target) && referenced)
    throw Error("Historical image index is missing under the active artifact root; restore its "
                "indexes directory");
  if (pending.empty() && (!fs::exists(target) || current_index(files, target)))
    return fs::exists(target) ? target : fs::path{};
  fs::create_directories(target.parent_path());
  if (::chmod(root.c_str(), 0700) || ::chmod(target.parent_path().c_str(), 0700))
    throw Error("Cannot protect artifact index directory");
  // Only IDs in this pending batch are retained in memory. The old ledger streams through.
  files.atomic_stream(
      target,
      [&](const FileSystem::Writer &write) {
        uint64_t sequence = 0, observations = 0, legacy = 0, line_number = 1, latest_line = 0;
        write(Json({{"version", 2}, {"kind", "header"}, {"session_id", session}}).dump() + '\n');
        if (fs::exists(target)) {
          auto input = files.open_read(target);
          if (input->path() != files.canonical(target))
            throw Error("Artifact index changed while opening");
          lines(*input, [&](const std::string &line) {
            auto record = Json::parse(line);
            const auto kind = record.value("kind", "");
            if (kind == "header" || kind == "position")
              return;
            if (kind == "observation") {
              ids.erase(record.at("observation_id").get<std::string>());
              sequence = std::max(sequence, record.at("sequence").get<uint64_t>());
              record["artifact"] = portable_image(record.at("artifact").get<ImageArtifact>());
              ++observations;
              latest_line = line_number + 1;
            } else {
              if (kind != "legacy")
                record = {{"version", 2},
                          {"kind", "legacy"},
                          {"chronology", "unknown"},
                          {"artifact", portable_image(record.get<ImageArtifact>())}};
              ++legacy;
            }
            write(safe_json(record).dump() + '\n');
            ++line_number;
          });
        }
        for (auto &record : pending)
          if (ids.count(record.at("observation_id"))) {
            record["sequence"] = ++sequence;
            write(safe_json(record).dump() + '\n');
            ++observations;
            latest_line = ++line_number;
          }
        write(Json({{"version", 2},
                    {"kind", "position"},
                    {"observations", observations},
                    {"legacy_unknown", legacy},
                    {"last_sequence", sequence},
                    {"latest_line", latest_line}})
                  .dump() +
              '\n');
      },
      true);
  return target;
}
void copy_image_history(const fs::path &root, FileSystem &files, const std::string &parent,
                        const std::string &child) {
  const auto source = artifact_index_path(root, parent), target = artifact_index_path(root, child);
  check_index(root, source);
  check_index(root, target);
  if (!fs::exists(source))
    return;
  fs::create_directories(target.parent_path());
  files.atomic_stream(
      target,
      [&](const FileSystem::Writer &write) {
        auto input = files.open_read(source);
        if (input->path() != files.canonical(source))
          throw Error("Artifact index changed while opening");
        lines(*input, [&](const std::string &line) {
          auto record = Json::parse(line);
          if (record.value("kind", "") == "header")
            record["session_id"] = child;
          write(safe_json(record).dump() + '\n');
        });
      },
      true);
  // Migrate legacy rows in the child only; a fork must not rewrite its live parent.
  archive_image_history(root, files, child, {});
}
Json image_history_position(const fs::path &root, const FileSystem &files,
                            const std::string &session) {
  const auto target = artifact_index_path(root, session);
  check_index(root, target);
  auto input = files.open_read(target);
  if (input->path() != files.canonical(target))
    throw Error("Artifact index changed while opening");
  const auto size = input->size();
  input->seek(size > 4096 ? size - 4096 : 0);
  std::string tail;
  char buffer[4096];
  const auto n = input->read(buffer, sizeof buffer);
  tail.assign(buffer, n);
  if (!tail.empty() && tail.back() == '\n')
    tail.pop_back();
  const auto pos = tail.rfind('\n');
  auto value = Json::parse(tail.substr(pos == std::string::npos ? 0 : pos + 1), nullptr, false);
  return value.is_object() && value.value("kind", "") == "position"
             ? value
             : Json({{"chronology", "unknown"}});
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
