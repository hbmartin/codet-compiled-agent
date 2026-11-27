#include "codet/patch.hpp"
#include "codet/support.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <array>
#include <fstream>
#include <gtest/gtest.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <sys/acl.h>
#include <sys/xattr.h>
#endif

using namespace codet;
namespace {
struct ControlledPatchOps : PatchFileOps {
  std::function<void(const std::string &, const fs::path &)> before = [](const auto &,
                                                                         const auto &) {};
  void link_exclusive(const fs::path &from, const fs::path &to) const override {
    before("link", to);
    PatchFileOps::link_exclusive(from, to);
  }
  void retire(const fs::path &from, const fs::path &private_name) const override {
    before("remove", from);
    PatchFileOps::retire(from, private_name);
  }
  void sync_directory(const fs::path &path) const override {
    before("sync", path);
    PatchFileOps::sync_directory(path);
  }
  void preserve_metadata(const fs::path &source, int source_fd, const fs::path &staged,
                         int staged_fd, const Cancellation &cancel) const override {
    before("metadata", staged);
    PatchFileOps::preserve_metadata(source, source_fd, staged, staged_fd, cancel);
  }
};

mode_t permissions(const fs::path &p) {
  struct stat st {};
  return ::stat(p.c_str(), &st) ? 0 : static_cast<mode_t>(st.st_mode & 0777);
}
PatchChange move_file(const fs::path &source, const fs::path &destination) {
  return {source, destination, digest("original\n"), "edited\n"};
}
} // namespace

namespace codet::test {
int patch_child(const fs::path &source, const fs::path &dest, const std::string &step) {
  NativeFileSystem files;
  Cancellation cancel;
  if (step == "mask077" || step == "mask022") {
    ::umask(step == "mask077" ? 0077 : 0022);
    files.publish_patch({{{}, dest, {}, "created\n"}}, cancel);
    return 0;
  }
  ControlledPatchOps ops;
  ops.before = [&](const auto &operation, const auto &path) {
    if ((step == "unpublished" && operation == "link" && path == dest) ||
        (step == "update-captured" && operation == "link" && path == source &&
         !fs::exists(source)) ||
        (step == "published" && operation == "remove" && path == source) ||
        (step == "retired" && operation == "sync" && path == source.parent_path() &&
         !fs::exists(source)))
      ::_exit(61);
  };
  publish_native_patch({move_file(source, dest)}, cancel, ops);
  return 2;
}
} // namespace codet::test

namespace {
TEST(PatchPublication, TerminationAtMoveBoundariesAlwaysLeavesCompleteFile) {
  for (bool nested : {false, true}) {
    for (const auto &step : {"unpublished", "published", "retired"}) {
      SCOPED_TRACE(std::string(step) + (nested ? " nested" : " same directory"));
      TempDirectory dir;
      NativeFileSystem files;
      const auto src = dir.root / "source";
      const auto dst = dir.root / (nested ? "new/deep/destination" : "destination");
      files.atomic_write(src, "original\n");
      ASSERT_EQ(::chmod(src.c_str(), 0751), 0);
      const auto pid =
          test::start_helper({"--codet-patch-helper", src.string(), dst.string(), step});
      int status = 0;
      ASSERT_TRUE(test::wait_helper(pid, status));
      ASSERT_TRUE(WIFEXITED(status));
      ASSERT_EQ(WEXITSTATUS(status), 61);
      EXPECT_EQ(fs::exists(src), std::string(step) != "retired");
      EXPECT_EQ(fs::exists(dst), std::string(step) != "unpublished");
      if (fs::exists(src)) {
        EXPECT_EQ(files.read(src), "original\n");
      }
      if (fs::exists(dst)) {
        EXPECT_EQ(files.read(dst), "edited\n");
        EXPECT_EQ(permissions(dst), 0751);
      }
    }
  }
}

TEST(PatchPublication, InterruptedInPlaceUpdateRetainsCapturedOriginal) {
  TempDirectory dir;
  NativeFileSystem files;
  const auto target = dir.root / "file";
  files.atomic_write(target, "original\n");
  ASSERT_EQ(::chmod(target.c_str(), 0751), 0);
  struct stat expected {};
  ASSERT_EQ(::lstat(target.c_str(), &expected), 0);
  const auto pid = test::start_helper(
      {"--codet-patch-helper", target.string(), target.string(), "update-captured"});
  int status = 0;
  ASSERT_TRUE(test::wait_helper(pid, status));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 61);
  EXPECT_FALSE(fs::exists(target));
  size_t captures = 0;
  for (const auto &stage : fs::directory_iterator(dir.root)) {
    if (stage.path().filename().string().rfind(".codet-patch-", 0) != 0)
      continue;
    const auto captured = stage.path() / "retired";
    if (!fs::exists(captured))
      continue;
    ++captures;
    struct stat actual {};
    ASSERT_EQ(::lstat(captured.c_str(), &actual), 0);
    EXPECT_EQ(actual.st_dev, expected.st_dev);
    EXPECT_EQ(actual.st_ino, expected.st_ino);
    EXPECT_EQ(permissions(captured), 0751);
    EXPECT_EQ(files.read(captured), "original\n");
  }
  EXPECT_EQ(captures, 1u);
}

TEST(PatchPublication, OperatingSystemAppliesUmaskToNewFiles) {
  for (const auto *mask : {"mask077", "mask022"}) {
    TempDirectory dir;
    const auto target = dir.root / "new";
    int status = 0;
    const auto pid = test::start_helper({"--codet-patch-helper", "-", target.string(), mask});
    ASSERT_TRUE(test::wait_helper(pid, status));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);
    EXPECT_EQ(permissions(target), std::string(mask) == "mask077" ? 0600 : 0644);
  }
}
TEST(PatchPublication, EditingSpecialModeStopsBeforeAnyReplacement) {
  TempDirectory dir;
  NativeFileSystem files;
  const auto source = dir.root / "source", dest = dir.root / "destination";
  files.atomic_write(source, "original\n");
  ASSERT_EQ(::chmod(source.c_str(), 01751), 0);
  EXPECT_THROW(files.publish_patch({move_file(source, dest)}, Cancellation{}), Error);
  struct stat st {};
  ASSERT_EQ(::lstat(source.c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 07777, 01751);
  EXPECT_EQ(files.read(source), "original\n");
  EXPECT_FALSE(fs::exists(dest));
}
struct InspectFirstPublication : PatchFileOps {
  fs::path destination;
  mode_t expected_mode;
  mutable bool observed = false;
  InspectFirstPublication(fs::path path, mode_t mode)
      : destination(std::move(path)), expected_mode(mode) {}
  void link_exclusive(const fs::path &from, const fs::path &to) const override {
    PatchFileOps::link_exclusive(from, to);
    if (to == destination) {
      observed = true;
      EXPECT_EQ(permissions(to), expected_mode);
    }
  }
};
TEST(PatchPublication, EditedMoveHasFinalPermissionsWhenLinkBecomesVisible) {
  for (mode_t mode : std::array<mode_t, 2>{0600, 0751}) {
    TempDirectory dir;
    NativeFileSystem files;
    const auto src = dir.root / "source", dst = dir.root / "destination";
    files.atomic_write(src, "original\n");
    ASSERT_EQ(::chmod(src.c_str(), mode), 0);
    InspectFirstPublication observe(dst, mode);
    publish_native_patch({move_file(src, dst)}, Cancellation{}, observe);
    EXPECT_TRUE(observe.observed);
    EXPECT_EQ(permissions(dst), mode);
  }
}
TEST(PatchPublication, CompetingFileAtPublicationIsNeverReplaced) {
  for (bool move : {false, true}) {
    TempDirectory dir;
    NativeFileSystem files;
    const auto src = dir.root / "source", dst = dir.root / "destination";
    files.atomic_write(src, "original\n");
    ControlledPatchOps ops;
    ops.before = [&](const auto &op, const auto &path) {
      if (op == "link" && path == dst)
        files.atomic_write(dst, "independent\n", true);
    };
    const auto change = move ? move_file(src, dst) : PatchChange{{}, dst, {}, "edited\n"};
    EXPECT_THROW(publish_native_patch({change}, Cancellation{}, ops), Error);
    EXPECT_EQ(files.read(src), "original\n");
    EXPECT_EQ(files.read(dst), "independent\n");
    EXPECT_EQ(permissions(dst), 0600);
  }
}

TEST(PatchPublication, InPlaceUpdatePreservesReplacementBetweenCheckAndRename) {
  for (bool during_rollback : {false, true}) {
    SCOPED_TRACE(during_rollback ? "rollback" : "commit");
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto target = dir.root / "file", competitor = dir.root / "independent",
               late = dir.root / "later";
    files.atomic_write(target, "original\n", true);
    files.atomic_write(competitor, "competing edit\n");
    ASSERT_EQ(::chmod(competitor.c_str(), 0640), 0);
    struct stat expected {};
    ASSERT_EQ(::lstat(competitor.c_str(), &expected), 0);
    bool failed_later = false, replaced = false;
    ops.before = [&](const auto &operation, const auto &path) {
      if (operation == "link" && path == late) {
        failed_later = true;
        throw Error("later patch failure");
      }
      if (operation == "remove" && path == target && failed_later == during_rollback && !replaced) {
        EXPECT_EQ(files.read(target), during_rollback ? "edited\n" : "original\n");
        fs::rename(competitor, target);
        replaced = true;
      }
    };
    std::vector<PatchChange> changes{move_file(target, target)};
    if (during_rollback)
      changes.push_back({{}, late, {}, "later\n"});
    std::string error;
    try {
      publish_native_patch(changes, Cancellation{}, ops);
    } catch (const Error &e) {
      error = e.what();
    }
    ASSERT_TRUE(replaced);
    EXPECT_EQ(failed_later, during_rollback);
    EXPECT_NE(error.find(target.string()), std::string::npos) << error;
    EXPECT_FALSE(fs::exists(late));
    struct stat actual {};
    ASSERT_EQ(::lstat(target.c_str(), &actual), 0);
    EXPECT_EQ(actual.st_dev, expected.st_dev);
    EXPECT_EQ(actual.st_ino, expected.st_ino);
    EXPECT_EQ(permissions(target), 0640);
    EXPECT_EQ(files.read(target), "competing edit\n");
  }
}

TEST(PatchPublication, InPlaceUpdateKeepsClaimantAndReportsCapturedFile) {
  for (bool replace_before_capture : {false, true}) {
    SCOPED_TRACE(replace_before_capture ? "racing restoration" : "racing publication");
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto target = dir.root / "file", replacement = dir.root / "replacement",
               claimant = dir.root / "claimant";
    files.atomic_write(target, "original\n", true);
    files.atomic_write(replacement, "first competing edit\n", true);
    files.atomic_write(claimant, "new occupant\n");
    struct stat captured {};
    ASSERT_EQ(::lstat((replace_before_capture ? replacement : target).c_str(), &captured), 0);
    bool claimed = false;
    ops.before = [&](const auto &operation, const auto &path) {
      if (path != target)
        return;
      if (operation == "remove" && replace_before_capture)
        fs::rename(replacement, target);
      if (operation == "link" && !claimed) {
        fs::create_hard_link(claimant, target);
        claimed = true;
      }
    };
    std::string error;
    try {
      publish_native_patch({move_file(target, target)}, Cancellation{}, ops);
    } catch (const Error &e) {
      error = e.what();
    }
    ASSERT_TRUE(claimed);
    ASSERT_TRUE(fs::equivalent(claimant, target));
    EXPECT_EQ(files.read(target), "new occupant\n");
    bool recovery_reported = false;
    for (const auto &entry : fs::recursive_directory_iterator(dir.root)) {
      const auto &path = entry.path();
      if (path.parent_path() == dir.root || !entry.is_regular_file() ||
          error.find(path.string()) == std::string::npos)
        continue;
      struct stat retained {};
      ASSERT_EQ(::lstat(path.c_str(), &retained), 0);
      if (retained.st_dev == captured.st_dev && retained.st_ino == captured.st_ino) {
        EXPECT_EQ(files.read(path),
                  replace_before_capture ? "first competing edit\n" : "original\n");
        recovery_reported = true;
      }
    }
    EXPECT_TRUE(recovery_reported) << error;
  }
}

TEST(PatchPublication, FailedPublicationOrSourceRetirementPreservesSourceAndMode) {
  for (const auto &point : {"parent", "child", "link", "sync", "remove", "metadata"}) {
    SCOPED_TRACE(point);
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto src = dir.root / "source", dst = dir.root / "new/deep/destination";
    files.atomic_write(src, "original\n", true);
    bool fired = false;
    ops.before = [&](const auto &op, const auto &path) {
      if (std::string(point) == "remove" && op == "remove" && path == src) {
        EXPECT_EQ(permissions(dst), 0600);
      }
      if (!fired && ((std::string(point) == "parent" && op == "sync" && path == dir.root &&
                      fs::exists(dir.root / "new")) ||
                     (std::string(point) == "child" && op == "sync" && path == dir.root / "new") ||
                     (std::string(point) == "link" && op == "link" && path == dst) ||
                     (std::string(point) == "sync" && op == "sync" && path == dst.parent_path()) ||
                     (std::string(point) == "remove" && op == "remove" && path == src) ||
                     (std::string(point) == "metadata" && op == "metadata"))) {
        fired = true;
        throw Error("injected patch I/O failure");
      }
    };
    EXPECT_THROW(publish_native_patch({move_file(src, dst)}, Cancellation{}, ops), Error);
    EXPECT_TRUE(fired);
    EXPECT_EQ(files.read(src), "original\n");
    EXPECT_EQ(permissions(src), 0600);
    EXPECT_FALSE(fs::exists(dst));
  }
}
TEST(PatchPublication, ReplacementDuringRetirementPreservesBothWriters) {
  for (std::string point : {"replace-source", "replace-destination", "replace-during-rollback"}) {
    SCOPED_TRACE(point);
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto src = dir.root / "source", dst = dir.root / "destination", late = dir.root / "later";
    files.atomic_write(src, "original\n", true);
    bool replaced = false;
    ops.before = [&](const auto &op, const auto &path) {
      if (op == "remove" && path == src && point != "replace-during-rollback") {
        files.atomic_write(point == "replace-source" ? src : dst, "other writer\n");
        replaced = true;
      }
      if (point == "replace-during-rollback") {
        if (op == "link" && path == late)
          throw Error("later patch failure");
        if (op == "remove" && path == dst) {
          files.atomic_write(dst, "other writer\n");
          replaced = true;
        }
      }
    };
    EXPECT_THROW(
        publish_native_patch({move_file(src, dst), {{}, late, {}, "later"}}, Cancellation{}, ops),
        Error);
    EXPECT_TRUE(replaced);
    EXPECT_EQ(files.read(src), point == "replace-source" ? "other writer\n" : "original\n");
    EXPECT_EQ(files.read(dst), point == "replace-source" ? "edited\n" : "other writer\n");
    EXPECT_FALSE(fs::exists(late));
  }
}
TEST(PatchPublication, LosingRestoredSourceDuringCleanupRetainsEditedDestination) {
  for (std::string race : {"remove-source", "replace-source", "occupy-destination"}) {
    SCOPED_TRACE(race);
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto src = dir.root / "source", dst = dir.root / "destination", late = dir.root / "late";
    files.atomic_write(src, "original\n", true);
    bool cleaning = false;
    ops.before = [&](const auto &operation, const auto &path) {
      if (operation == "link" && path == late)
        throw Error("later patch failure");
      if (operation == "remove" && path == dst) {
        EXPECT_EQ(files.read(src), "original\n");
        if (race == "replace-source")
          files.atomic_write(src, "independent source\n");
        else
          files.remove(src);
        cleaning = true;
      }
      if (operation == "sync" && cleaning && race == "occupy-destination" && !fs::exists(dst))
        files.atomic_write(dst, "independent destination\n");
    };
    try {
      publish_native_patch({move_file(src, dst), {{}, late, {}, "later"}}, Cancellation{}, ops);
      FAIL() << "expected patch failure";
    } catch (const Error &error) {
      ASSERT_TRUE(cleaning);
      EXPECT_FALSE(fs::exists(late));
      if (race == "replace-source")
        EXPECT_EQ(files.read(src), "independent source\n");
      else
        EXPECT_FALSE(fs::exists(src));
      if (race == "occupy-destination") {
        EXPECT_EQ(files.read(dst), "independent destination\n");
        const std::string message = error.what(), prefix = "; retained at ";
        const auto start = message.find(prefix);
        ASSERT_NE(start, std::string::npos) << message;
        const auto end = message.find(": ", start + prefix.size());
        ASSERT_NE(end, std::string::npos);
        const auto recovered = message.substr(start + prefix.size(), end - start - prefix.size());
        EXPECT_EQ(files.read(recovered), "edited\n");
        EXPECT_EQ(permissions(recovered), 0600);
      } else {
        EXPECT_EQ(files.read(dst), "edited\n");
        EXPECT_EQ(permissions(dst), 0600);
      }
    }
  }
}
TEST(PatchPublication, RollbackRestoresSourceBeforeRetiringOwnedDestination) {
  for (const auto &point :
       {"clean", "intervening-source", "intervening-destination", "failed-restore"}) {
    SCOPED_TRACE(point);
    TempDirectory dir;
    NativeFileSystem files;
    ControlledPatchOps ops;
    const auto src = dir.root / "source", dst = dir.root / "destination", late = dir.root / "later";
    files.atomic_write(src, "original\n", true);
    ops.before = [&](const auto &op, const auto &path) {
      if (op == "link" && path == late) {
        if (std::string(point) == "intervening-source")
          files.atomic_write(src, "other\n");
        if (std::string(point) == "intervening-destination")
          files.atomic_write(dst, "other\n");
        throw Error("later patch failure");
      }
      if (op == "link" && path == src && std::string(point) == "failed-restore")
        throw Error("cannot restore");
      if (op == "remove" && path == dst) {
        EXPECT_EQ(files.read(src), "original\n");
      }
    };
    try {
      publish_native_patch({move_file(src, dst), {{}, late, {}, "later"}}, Cancellation{}, ops);
      FAIL() << "expected patch failure";
    } catch (const Error &error) {
      if (std::string(point) != "clean") {
        const std::string message = error.what(), prefix = "(before-image: ";
        const auto offset = message.find(prefix);
        ASSERT_NE(offset, std::string::npos) << message;
        ASSERT_EQ(message.back(), ')');
        const auto saved =
            message.substr(offset + prefix.size(), message.size() - offset - prefix.size() - 1);
        EXPECT_EQ(files.read(saved), "original\n");
        EXPECT_EQ(permissions(saved), 0600);
      }
    }
    EXPECT_FALSE(fs::exists(late));
    if (std::string(point) == "failed-restore") {
      EXPECT_FALSE(fs::exists(src));
      EXPECT_EQ(files.read(dst), "edited\n");
    } else if (std::string(point) == "intervening-source") {
      EXPECT_EQ(files.read(src), "other\n");
      EXPECT_EQ(files.read(dst), "edited\n");
    } else {
      EXPECT_EQ(files.read(src), "original\n");
      EXPECT_EQ(permissions(src), 0600);
      if (std::string(point) == "clean")
        EXPECT_FALSE(fs::exists(dst));
      else
        EXPECT_EQ(files.read(dst), "other\n");
    }
  }
}

#ifdef __APPLE__
TEST(PatchPublication, UnsupportedMacFileFlagsLeaveOriginalIntact) {
  TempDirectory dir;
  NativeFileSystem files;
  const auto src = dir.root / "source", dst = dir.root / "moved";
  files.atomic_write(src, "original\n");
  ASSERT_EQ(::chflags(src.c_str(), UF_NODUMP), 0);
  EXPECT_THROW(files.publish_patch({move_file(src, dst)}, Cancellation{}), Error);
  EXPECT_EQ(files.read(src), "original\n");
  struct stat status {};
  ASSERT_EQ(::stat(src.c_str(), &status), 0);
  EXPECT_NE(status.st_flags & UF_NODUMP, 0u);
  EXPECT_FALSE(fs::exists(dst));
}
TEST(PatchPublication, MacFlagsSetWhileAcquiringBeforeImagePreserveSource) {
  TempDirectory dir;
  NativeFileSystem files;
  ControlledPatchOps ops;
  const auto src = dir.root / "source", dst = dir.root / "moved";
  files.atomic_write(src, "original\n");
  bool flagged = false;
  ops.before = [&](const auto &operation, const auto &) {
    if (operation == "link" && !flagged) {
      ASSERT_EQ(::chflags(src.c_str(), UF_NODUMP), 0);
      flagged = true;
    }
  };
  EXPECT_THROW(publish_native_patch({move_file(src, dst)}, Cancellation{}, ops), Error);
  EXPECT_TRUE(flagged);
  EXPECT_EQ(files.read(src), "original\n");
  struct stat status {};
  ASSERT_EQ(::stat(src.c_str(), &status), 0);
  EXPECT_NE(status.st_flags & UF_NODUMP, 0u);
  EXPECT_FALSE(fs::exists(dst));
}
constexpr char test_attribute[] = "com.example.codet.patch";
const std::string binary_value("hello\0world", 11);

std::string read_attribute(const fs::path &path) {
  std::string value(binary_value.size(), '\0');
  if (::getxattr(path.c_str(), test_attribute, value.data(), value.size(), 0, 0) !=
      static_cast<ssize_t>(value.size()))
    throw Error("Missing test attribute");
  return value;
}
TEST(PatchPublication, MacAttributesAndResourceForkSurviveUpdatesMovesAndRollback) {
  // Include non-text bytes and force the resource-fork verifier to cross a block boundary.
  const std::string fork = std::string(65539, 'q') + binary_value;
  for (std::string action : {"update", "move", "rollback"}) {
    SCOPED_TRACE(action);
    TempDirectory dir;
    NativeFileSystem files;
    const auto src = dir.root / "source", other = dir.root / "moved", late = dir.root / "late";
    const auto dst = action == "update" ? src : other;
    files.atomic_write(src, "original\n");
    ASSERT_EQ(
        ::setxattr(src.c_str(), test_attribute, binary_value.data(), binary_value.size(), 0, 0), 0);
    {
      std::ofstream output(src / "..namedfork/rsrc", std::ios::binary);
      output << fork;
      output.close();
      ASSERT_TRUE(output);
    }
    ControlledPatchOps ops;
    const auto change = move_file(src, dst);
    if (action == "rollback") {
      ops.before = [&](const auto &op, const auto &path) {
        if (op == "link" && path == late)
          throw Error("later patch failure");
      };
      EXPECT_THROW(publish_native_patch({change, {{}, late, {}, "later"}}, Cancellation{}, ops),
                   Error);
    } else {
      ASSERT_NO_THROW(publish_native_patch({change}, Cancellation{}, ops));
    }
    const auto &result = action == "rollback" ? src : dst;
    EXPECT_EQ(files.read(result), action == "rollback" ? "original\n" : "edited\n");
    EXPECT_EQ(read_attribute(result), binary_value);
    EXPECT_EQ(files.read(result / "..namedfork/rsrc"), fork);
  }
}
struct CorruptStagedAttribute : PatchFileOps {
  void preserve_metadata(const fs::path &source, int source_fd, const fs::path &staged,
                         int staged_fd, const Cancellation &cancel) const override {
    PatchFileOps::preserve_metadata(source, source_fd, staged, staged_fd, cancel);
    ASSERT_EQ(::fsetxattr(staged_fd, test_attribute, "other", 5, 0, 0), 0);
  }
};
TEST(PatchPublication, MetadataVerificationFailurePreservesOriginal) {
  TempDirectory dir;
  NativeFileSystem files;
  const auto src = dir.root / "source", dst = dir.root / "moved";
  files.atomic_write(src, "original\n", true);
  ASSERT_EQ(::setxattr(src.c_str(), test_attribute, binary_value.data(), binary_value.size(), 0, 0),
            0);
  EXPECT_THROW(
      publish_native_patch({move_file(src, dst)}, Cancellation{}, CorruptStagedAttribute{}), Error);
  EXPECT_EQ(files.read(src), "original\n");
  EXPECT_EQ(permissions(src), 0600);
  EXPECT_EQ(read_attribute(src), binary_value);
  EXPECT_FALSE(fs::exists(dst));
}

std::string file_acl(const fs::path &path) {
  const auto acl = ::acl_get_file(path.c_str(), ACL_TYPE_EXTENDED);
  if (!acl)
    throw Error("Cannot read test ACL");
  std::unique_ptr<void, decltype(&::acl_free)> acl_owner(acl, ::acl_free);
  ssize_t len = 0;
  const auto text = ::acl_to_text(acl, &len);
  if (!text || len < 0)
    throw Error("Cannot serialize test ACL");
  std::unique_ptr<void, decltype(&::acl_free)> text_owner(text, ::acl_free);
  return {text, static_cast<size_t>(len)};
}
TEST(PatchPublication, MacExtendedAclSurvivesEditedMove) {
  TempDirectory dir;
  NativeFileSystem files;
  const auto src = dir.root / "source", dst = dir.root / "moved";
  files.atomic_write(src, "original\n", true);
  acl_t acl = ::acl_init(1);
  ASSERT_NE(acl, nullptr);
  acl_entry_t entry;
  ASSERT_EQ(::acl_create_entry(&acl, &entry), 0);
  std::unique_ptr<void, decltype(&::acl_free)> acl_owner(acl, ::acl_free);
  // Darwin's well-known group UUID for everyone (gid 12).
  unsigned char everyone[16] = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd,
                                0xef, 0xab, 0xcd, 0xef, 0,    0,    0,    12};
  ASSERT_EQ(::acl_set_tag_type(entry, ACL_EXTENDED_ALLOW), 0);
  ASSERT_EQ(::acl_set_qualifier(entry, everyone), 0);
  acl_permset_t permissions;
  ASSERT_EQ(::acl_get_permset(entry, &permissions), 0);
  ASSERT_EQ(::acl_add_perm(permissions, ACL_READ_ATTRIBUTES), 0);
  ASSERT_EQ(::acl_set_file(src.c_str(), ACL_TYPE_EXTENDED, acl), 0);
  const auto expected = file_acl(src);
  ASSERT_FALSE(expected.empty());
  ASSERT_NO_THROW(files.publish_patch({move_file(src, dst)}, Cancellation{}));
  EXPECT_EQ(file_acl(dst), expected);
}
#endif
} // namespace
