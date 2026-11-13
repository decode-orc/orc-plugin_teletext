/*
 * File:        nabts_sink_deps_test.cpp
 * Module:      orc-tests
 * Purpose:     Unit tests for the NABTS sink's file-system work, run over an
 *              in-memory file system
 *
 * Covers: the record files of a reference folder read and named for the
 * recording, a missing or unreadable reference counted, the run's own
 * records never read as a reference, and the web mirror's folders made
 * through the file system before its files are written through the host's
 * writer. No I/O.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "nabts_sink_deps.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "nabts_file_system.h"
#include "stage_services_mock.h"

namespace orc_unit_test {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/// Files and folders held in memory; paths are compared lexically.
class InMemoryFileSystem final : public orc::INabtsFileSystem {
 public:
  void add_file(const std::string& path, std::vector<uint8_t> data) {
    files_[normal(path)] = std::move(data);
    add_folder(std::filesystem::path(path).parent_path().string());
  }
  void add_folder(const std::string& path) {
    for (std::filesystem::path p(normal(path)); !p.empty() && p != p.root_path();
         p = p.parent_path()) {
      folders_.insert(p.string());
    }
  }
  // A file that is listed but cannot be read.
  void add_unreadable(const std::string& path) {
    add_file(path, {});
    unreadable_.insert(normal(path));
  }
  const std::set<std::string>& made() const { return made_; }

  Kind kind(const std::string& path) const override {
    const std::string p = normal(path);
    if (folders_.count(p) > 0) return Kind::kDirectory;
    if (files_.count(p) > 0) return Kind::kFile;
    return Kind::kMissing;
  }
  std::vector<std::string> list(const std::string& directory) const override {
    std::vector<std::string> out;
    const std::string d = normal(directory);
    for (const auto& [path, data] : files_) {
      if (std::filesystem::path(path).parent_path().string() == d) {
        out.push_back(path);
      }
    }
    return out;
  }
  std::optional<std::vector<uint8_t>> read(
      const std::string& path) const override {
    const std::string p = normal(path);
    const auto it = files_.find(p);
    if (it == files_.end() || unreadable_.count(p) > 0) return std::nullopt;
    return it->second;
  }
  bool make_directories(const std::string& directory) const override {
    made_.insert(normal(directory));
    return true;
  }
  std::string canonical(const std::string& path) const override {
    return normal(path);
  }

 private:
  static std::string normal(const std::string& path) {
    return std::filesystem::path(path).lexically_normal().string();
  }
  std::map<std::string, std::vector<uint8_t>> files_;
  std::set<std::string> folders_;
  std::set<std::string> unreadable_;
  mutable std::set<std::string> made_;
};

class NabtsSinkDepsFiles : public ::testing::Test {
 protected:
  std::shared_ptr<InMemoryFileSystem> fs_ =
      std::make_shared<InMemoryFileSystem>();
  NiceMock<MockStageServices> services_;
  orc::NabtsSinkDeps deps_{&services_, fs_};
};

// A folder stands for the record files export_records wrote in it: each read
// through the file system, named for the recording (a run's out.t33 by its
// folder) and keyed by its file name. Anything else in the folder is passed.
TEST_F(NabtsSinkDepsFiles, ReadsTheRecordFilesOfAReferenceFolder) {
  fs_->add_file("/refs/tapeA/out.t33.000-150-v0.rec", {0x41, 0x42, 0x43});
  fs_->add_file("/refs/tapeA/out.t33.000-151-v2.rec", {0x44});
  fs_->add_file("/refs/tapeA/notes.txt", {0x20});
  orc::NabtsSinkOptions options;
  options.reference_records = {"/refs/tapeA"};
  orc::NabtsSinkResult result;
  deps_.load_references(options, result);

  ASSERT_EQ(result.dataset.references.size(), 2u);
  EXPECT_EQ(result.dataset.references[0].source, "tapeA");
  EXPECT_EQ(result.dataset.references[0].key, "000-150-v0");
  EXPECT_EQ(result.dataset.references[0].data,
            (std::vector<uint8_t>{0x41, 0x42, 0x43}));
  EXPECT_EQ(result.dataset.references[1].key, "000-151-v2");
  EXPECT_EQ(result.reference_streams, 1u);
  EXPECT_EQ(result.reference_paths_unreadable, 0u);
}

// A path that is not there, and a file that cannot be read, are each counted
// and the run goes on.
TEST_F(NabtsSinkDepsFiles, CountsAReferenceItCouldNotRead) {
  fs_->add_unreadable("/refs/tapeB/out.t33.000-150-v0.rec");
  orc::NabtsSinkOptions options;
  options.reference_records = {"/refs/missing.t33",
                               "/refs/tapeB/out.t33.000-150-v0.rec"};
  orc::NabtsSinkResult result;
  deps_.load_references(options, result);

  EXPECT_TRUE(result.dataset.references.empty());
  EXPECT_EQ(result.reference_paths_unreadable, 2u);
}

// The records this run exported beside its own stream are its own records,
// not another recording's, however the reference names the folder.
TEST_F(NabtsSinkDepsFiles, NeverReadsTheRunsOwnRecords) {
  fs_->add_file("/runs/today/out.t33.000-150-v0.rec", {0x41});
  fs_->add_file("/runs/yesterday/out.t33.000-150-v0.rec", {0x42});
  orc::NabtsSinkOptions options;
  options.reference_records = {"/runs/today/../today", "/runs/yesterday"};
  orc::NabtsSinkResult result;
  result.output_path = "/runs/today/out.t33";
  deps_.load_references(options, result);

  ASSERT_EQ(result.dataset.references.size(), 1u);
  EXPECT_EQ(result.dataset.references[0].source, "yesterday");
}

// A framed service's file goes to the mirror: its folder is made through the
// file system, and the file written there through the host's writer.
TEST_F(NabtsSinkDepsFiles, MakesTheMirrorsFoldersThroughTheFileSystem) {
  orc::NabtsCarriedFile file;
  file.channel = 0x010;
  file.last_channel = 0x01F;
  file.transfer = "framed service";
  file.name = "page.htm";
  file.source = "http://home.example.net/news/page.htm";
  file.time = "1999-01-02 03:04:05";
  file.copies = 1;
  const std::string text = "<html>news</html>";
  file.data.assign(text.begin(), text.end());
  file.format = "text";

  orc::NabtsSinkOptions options;
  options.export_records = true;
  orc::NabtsSinkResult result;
  result.output_path = "/out/run.t33";
  result.dataset.carried_files = {file};

  std::vector<std::string> opened;
  ON_CALL(services_, create_buffered_file_writer_uint8(_))
      .WillByDefault([&opened](size_t) {
        auto writer = std::make_shared<NiceMock<MockFileWriterUint8>>();
        ON_CALL(*writer, open(_)).WillByDefault([&opened](const std::string& p) {
          opened.push_back(p);
          return true;
        });
        return writer;
      });
  deps_.write_carried_files(options, result);

  const std::string folder =
      (std::filesystem::path("/out/run.t33.mirror") / "home.example.net" /
       "news")
          .lexically_normal()
          .string();
  EXPECT_EQ(fs_->made().count(folder), 1u);
  EXPECT_NE(std::find(opened.begin(), opened.end(),
                      (std::filesystem::path(folder) / "page.htm").string()),
            opened.end());
  EXPECT_GE(result.mirror_files_exported, 1u);
}

}  // namespace
}  // namespace orc_unit_test
