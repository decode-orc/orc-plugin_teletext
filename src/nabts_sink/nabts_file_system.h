/*
 * File:        nabts_file_system.h
 * Module:      orc-stage-plugin-nabts_sink
 * Purpose:     The file system the NABTS sink reads reference recordings
 *              from and makes its export folders in, behind an interface
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */
#ifndef ORC_NABTS_FILE_SYSTEM_H
#define ORC_NABTS_FILE_SYSTEM_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orc {

/**
 * @brief What the NABTS sink asks of the file system beyond writing files
 *
 * Files are written through the host's writers (IStageServices); this is the
 * rest: listing and reading the reference recordings reference_records
 * names, and making the folders export_records writes under. Tests stand an
 * in-memory one in for nabts_local_file_system().
 *
 * Paths are strings in the platform's own form. No method throws: a path
 * that cannot be read or made is reported as such.
 */
class INabtsFileSystem {
 public:
  virtual ~INabtsFileSystem() = default;

  enum class Kind { kMissing, kFile, kDirectory };

  /// Whether |path| is a regular file, a directory or neither.
  virtual Kind kind(const std::string& path) const = 0;

  /// The paths of the entries directly in |directory|, in no set order;
  /// empty where it cannot be listed.
  virtual std::vector<std::string> list(const std::string& directory) const = 0;

  /// The bytes of |path|, or nothing where it cannot be read.
  virtual std::optional<std::vector<uint8_t>> read(
      const std::string& path) const = 0;

  /// Makes |directory| and any parents missing. False where it could not.
  virtual bool make_directories(const std::string& directory) const = 0;

  /// |path| made absolute with its "." and ".." resolved, as far as it
  /// exists, so two names of one file compare equal; empty on failure.
  virtual std::string canonical(const std::string& path) const = 0;
};

/// The machine's own file system (std::filesystem).
std::shared_ptr<const INabtsFileSystem> nabts_local_file_system();

}  // namespace orc

#endif  // ORC_NABTS_FILE_SYSTEM_H
