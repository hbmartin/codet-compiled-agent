#pragma once
#include "codet/core.hpp"
#include <unistd.h>

struct TempDirectory {
  codet::fs::path root;
  TempDirectory() {
    auto pattern = (codet::fs::temp_directory_path() / "codet-test-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    const auto *path = ::mkdtemp(name.data());
    if (!path)
      throw std::runtime_error("Cannot create test temporary directory");
    root = path;
  }
  ~TempDirectory() {
    std::error_code error;
    codet::fs::remove_all(root, error);
  }
  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;
};
