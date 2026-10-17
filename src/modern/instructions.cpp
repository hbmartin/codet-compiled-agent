#include "codet/core.hpp"
#include <algorithm>
#include <sstream>

namespace codet {
namespace {
std::vector<std::string> lines(const std::string &text) {
  std::vector<std::string> out;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    out.push_back(line);
  }
  return out;
}
std::string trim(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
std::string single_line(const std::string &text) {
  std::istringstream input(safe_text(text));
  std::string word, out;
  while (input >> word) {
    if (!out.empty())
      out += ' ';
    out += word;
  }
  return out;
}
struct SkillMetadata {
  std::string name, description;
};
SkillMetadata skill_metadata(const std::string &text, const std::string &fallback) {
  auto input = lines(text);
  if (input.empty() || input.front() != "---")
    throw Error("missing frontmatter");
  auto end = std::find(input.begin() + 1, input.end(), "---");
  if (end == input.end())
    throw Error("frontmatter exceeds 8 KiB or is unterminated");
  SkillMetadata result{fallback, ""};
  std::set<std::string> seen;
  for (auto it = input.begin() + 1; it != end; ++it) {
    const auto colon = it->find(':');
    if (colon == std::string::npos || it->empty() || (*it)[0] == ' ' || (*it)[0] == '\t')
      continue;
    const auto key = trim(it->substr(0, colon));
    if (key != "name" && key != "description")
      continue;
    if (!seen.insert(key).second)
      throw Error("duplicate metadata field");
    auto value = trim(it->substr(colon + 1));
    if (value.empty())
      throw Error("empty metadata field");
    if (value[0] == '|' || value[0] == '>') {
      if (value != "|" && value != ">" && value != "|-" && value != ">-" && value != "|+" &&
          value != ">+")
        throw Error("unsupported block scalar");
      value.clear();
      while (it + 1 != end &&
             ((it + 1)->empty() || (it + 1)->front() == ' ' || (it + 1)->front() == '\t')) {
        ++it;
        value += trim(*it) + " ";
      }
    } else if (value.front() == '"') {
      const auto decoded = Json::parse(value, nullptr, false);
      if (!decoded.is_string())
        throw Error("invalid quoted scalar");
      value = decoded.get<std::string>();
    } else if (value.front() == '\'') {
      if (value.size() < 2 || value.back() != '\'')
        throw Error("invalid quoted scalar");
      value = value.substr(1, value.size() - 2);
      std::string decoded;
      for (size_t at = 0; at < value.size(); ++at) {
        if (value[at] == '\'') {
          if (at + 1 == value.size() || value[at + 1] != '\'')
            throw Error("invalid quoted scalar");
          ++at;
        }
        decoded += value[at];
      }
      value = std::move(decoded);
    } else {
      if (std::string("&*[]{}").find(value.front()) != std::string::npos)
        throw Error("unsupported scalar");
      const auto comment = value.find(" #");
      if (comment != std::string::npos)
        value.resize(comment);
    }
    value = single_line(value);
    if (value.empty() || value.size() > (key == "name" ? 64u : 4096u))
      throw Error("metadata field exceeds bounds or is empty");
    (key == "name" ? result.name : result.description) = std::move(value);
  }
  if (result.description.empty())
    throw Error("missing description");
  return result;
}
} // namespace
std::string Tools::instructions() const {
  const std::string inspection = "read_file";
  const auto cwd = files_.canonical(config_.cwd), home = files_.canonical(config_.home);
  fs::path root = cwd;
  for (auto p = cwd; !p.empty(); p = p.parent_path()) {
    if (fs::exists(p / ".git")) {
      root = p;
      break;
    }
    if (p == p.root_path())
      break;
  }
  std::vector<fs::path> directories;
  for (auto p = cwd;; p = p.parent_path()) {
    directories.push_back(p);
    if (p == root)
      break;
  }
  directories.push_back(home);
  std::string out;
  const auto warn = [&](const std::string &message) {
    if (warnings_.insert(message).second)
      report_diagnostic(diagnostics_, message);
  };
  std::set<fs::path> seen;
  const auto append = [&](const std::string &text, size_t cap) {
    if (out.size() >= cap)
      return false;
    const auto clean = safe_text(text);
    const auto portion = bounded_text(clean, cap - out.size());
    out += portion;
    return portion.size() == clean.size();
  };
  std::function<void(const fs::path &, int)> include = [&](const fs::path &input, int depth) {
    const auto p = files_.canonical(input);
    if (seen.count(p) || !fs::exists(p) || (!within(p, root) && !within(p, home)))
      return;
    if (depth > 16) {
      warn("Instruction include depth exceeded: " + p.string());
      return;
    }
    seen.insert(p);
    if (!config_.project_doc_max_bytes)
      return;
    if (out.size() >= config_.project_doc_max_bytes) {
      warn("Instruction budget exhausted; omitted: " + p.string());
      return;
    }
    bool truncated = false;
    std::string text;
    try {
      text = files_.read_prefix(p, config_.project_doc_max_bytes - out.size(), truncated);
    } catch (const std::exception &e) {
      warn("Instruction read failed: " + std::string(e.what()));
      return;
    }
    append("\n# Instructions: " + p.string() + "\n", config_.project_doc_max_bytes);
    const auto parts = lines(text);
    for (size_t i = 0; i < parts.size(); ++i) {
      const auto &line = parts[i];
      const bool complete_line =
          !truncated || i + 1 < parts.size() || (!text.empty() && text.back() == '\n');
      if (complete_line && line.rfind("@include ", 0) == 0)
        include(p.parent_path() / line.substr(9), depth + 1);
      else if (complete_line && !line.empty() && line[0] == '@' &&
               line.find(' ') == std::string::npos)
        include(p.parent_path() / line.substr(1), depth + 1);
      else if (!append(line + "\n", config_.project_doc_max_bytes)) {
        truncated = true;
        break;
      }
    }
    if (truncated)
      warn("Instruction content truncated to project_doc_max_bytes: " + p.string());
  };
  for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
    for (const auto *name : {"AGENTS.override.md", "AGENTS.md", "CLAUDE.md"})
      if (fs::is_regular_file(*it / name)) {
        include(*it / name, 0);
        break;
      }
  }
  const auto memory = config_.home / "memory";
  size_t count = 0;
  if (config_.memory_context_max_bytes && fs::exists(memory)) {
    std::set<fs::path> paths;
    bool omitted = false;
    for (const auto &entry : fs::directory_iterator(memory))
      if (!entry.is_symlink() && entry.is_regular_file() && entry.path().extension() == ".md") {
        paths.insert(entry.path());
        if (paths.size() > 200) {
          paths.erase(std::prev(paths.end()));
          omitted = true;
        }
      }
    if (omitted)
      warn("Memory file count exceeds 200; omitted later paths");
    const auto cap = out.size() + config_.memory_context_max_bytes;
    for (const auto &p : paths) {
      if (++count > 200 || out.size() >= cap) {
        warn("Memory context budget exhausted; use " + inspection + " to read additional files");
        break;
      }
      if (!append("\n# Memory: " + p.string() + " (read through " + inspection + ")\n", cap))
        break;
      bool truncated = false;
      try {
        const auto text = files_.read_prefix(p, cap - out.size(), truncated);
        append(text, cap);
        if (truncated)
          warn("Memory context truncated: " + p.string());
      } catch (const std::exception &e) {
        warn("Memory read failed: " + std::string(e.what()));
      }
    }
  }
  out += "\n# Available skills (read the exact SKILL.md path through read_file)\n";
  skill_directories_.clear();
  skill_anchors_.clear();
  // Higher-priority roots come first; never recursively crawl a workspace.
  std::vector<fs::path> skill_roots;
  for (const auto &dir : directories) {
    if (dir == home)
      continue;
    for (const auto *name : {".codet/skills", ".agents/skills", ".claude/skills"})
      skill_roots.push_back(dir / name);
  }
  skill_roots.push_back(config_.home / "skills");
  const auto user_home = env("HOME");
  if (!user_home.empty())
    skill_roots.push_back(fs::path(user_home) / ".agents/skills");
  std::set<fs::path> visited;
  std::set<std::string> names;
  size_t catalog_bytes = 0, admitted = 0;
  bool exhausted = false;
  for (const auto &dir : skill_roots) {
    if (exhausted)
      break;
    if (fs::is_symlink(dir) || !fs::is_directory(dir))
      continue;
    std::set<fs::path> candidates;
    // Retain only the first bounded set, independent of directory iteration order.
    for (const auto &entry : fs::directory_iterator(dir)) {
      if (entry.is_symlink() || !entry.is_directory())
        continue;
      candidates.insert(entry.path());
      if (candidates.size() > 101)
        candidates.erase(std::prev(candidates.end()));
    }
    for (const auto &candidate : candidates) {
      const auto path = candidate / "SKILL.md";
      if (fs::is_symlink(path) || !fs::is_regular_file(path))
        continue;
      const auto canonical = files_.canonical(candidate);
      if (!within(canonical, files_.canonical(dir)) || !visited.insert(canonical).second)
        continue;
      if (++admitted > 100) {
        exhausted = true;
        break;
      }
      SkillMetadata metadata{candidate.filename().string(), ""};
      try {
        bool cut = false;
        const auto prefix = files_.read_prefix(path, 8192, cut);
        metadata = skill_metadata(prefix, metadata.name);
      } catch (const std::exception &error) {
        warn("Skill metadata unavailable: " + path.string() + ": " + error.what());
      }
      if (!names.insert(metadata.name).second)
        continue;
      const auto line = "- " + metadata.name + ": " + metadata.description +
                        " (file: " + (canonical / "SKILL.md").string() + ")\n";
      if (line.size() > 16384 - catalog_bytes) {
        exhausted = true;
        break;
      }
      catalog_bytes += line.size();
      out += line;
      auto advertised = canonical;
      auto anchor = fs::absolute(dir).lexically_normal();
      for (const auto &base : {config_.home, config_.cwd})
        if (within(canonical, files_.canonical(base))) {
          anchor = fs::absolute(base).lexically_normal();
          advertised = anchor / canonical.lexically_relative(files_.canonical(base));
          break;
        }
      private_anchors_.emplace(anchor, files_.canonical(anchor));
      skill_anchors_[advertised] = anchor;
      skill_directories_.push_back(advertised);
    }
  }
  if (exhausted)
    warn("Skill catalog budget exhausted; later skills omitted");
  return out;
}
} // namespace codet
