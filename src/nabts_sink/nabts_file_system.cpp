/*
 * File:        nabts_file_system.cpp
 * Module:      orc-stage-plugin-nabts_sink
 * Purpose:     The machine's own file system behind INabtsFileSystem
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */
#include "nabts_file_system.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace orc {

namespace {

namespace fs = std::filesystem;

class LocalFileSystem final : public INabtsFileSystem {
 public:
  Kind kind(const std::string& path) const override {
    std::error_code error;
    if (fs::is_directory(path, error)) {
      return Kind::kDirectory;
    }
    if (fs::is_regular_file(path, error)) {
      return Kind::kFile;
    }
    return Kind::kMissing;
  }

  std::vector<std::string> list(const std::string& directory) const override {
    std::vector<std::string> paths;
    std::error_code error;
    for (fs::directory_iterator it(directory, error), end;
         !error && it != end; it.increment(error)) {
      paths.push_back(it->path().string());
    }
    return paths;
  }

  std::optional<std::vector<uint8_t>> read(
      const std::string& path) const override {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      return std::nullopt;
    }
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>()};
    if (in.bad()) {
      return std::nullopt;
    }
    return bytes;
  }

  bool make_directories(const std::string& directory) const override {
    std::error_code error;
    fs::create_directories(directory, error);
    return !error && fs::is_directory(directory, error);
  }

  std::string canonical(const std::string& path) const override {
    std::error_code error;
    const fs::path resolved = fs::weakly_canonical(path, error);
    return error ? std::string() : resolved.string();
  }
};

}  // namespace

std::shared_ptr<const INabtsFileSystem> nabts_local_file_system() {
  static const std::shared_ptr<const INabtsFileSystem> local =
      std::make_shared<LocalFileSystem>();
  return local;
}

}  // namespace orc
