#include "native_io_metrics.hpp"
#include "temp.hpp"
#include <fcntl.h>
#include <fstream>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/stat.h>

TEST(NativeStat, InterposedMetadataMatchesFileAbiAndObservesEveryCall) {
  TempDirectory temporary;
  const auto path = temporary.root / "seven-bytes";
  std::ofstream(path) << "1234567";
  const int file = ::open(path.c_str(), O_RDONLY);
  ASSERT_GE(file, 0);
  struct Close {
    int fd;
    ~Close() {
      ::close(fd);
    }
  } close{file};
  struct stat direct {
  }, observed{};
  ASSERT_EQ(::stat(path.c_str(), &direct), 0);
  auto &metrics = codet::test::native_io_metrics;
  struct Reset {
    ~Reset() {
      codet::test::native_io_metrics = {};
    }
  } reset;
  metrics = {};
  metrics.interrupt_fd = file;
  ASSERT_EQ(::fstat(file, &observed), 0);
  EXPECT_EQ(metrics.inspections, 1u);
  EXPECT_EQ(observed.st_dev, direct.st_dev);
  EXPECT_EQ(observed.st_ino, direct.st_ino);
  EXPECT_EQ(observed.st_mode, direct.st_mode);
  EXPECT_EQ(observed.st_size, 7);
  EXPECT_EQ(observed.st_size, direct.st_size);
  metrics.stats_interrupted = 1;
  EXPECT_EQ(::fstat(file, &observed), -1);
  EXPECT_EQ(errno, EINTR);
  EXPECT_EQ(metrics.inspections, 2u);
  EXPECT_EQ(::fstat(file, &observed), 0);
  EXPECT_EQ(metrics.inspections, 3u);
}
TEST(NativeStat, PipeAndSocketLayoutsRemainIntact) {
  int pipe[2], sockets[2];
  ASSERT_EQ(::pipe(pipe), 0);
  struct Close {
    int a, b;
    ~Close() {
      ::close(a);
      ::close(b);
    }
  } pipe_close{pipe[0], pipe[1]};
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  Close socket_close{sockets[0], sockets[1]};
  struct stat status {};
  ASSERT_EQ(::fstat(pipe[1], &status), 0);
  EXPECT_TRUE(S_ISFIFO(status.st_mode));
  ASSERT_EQ(::fstat(sockets[1], &status), 0);
  EXPECT_TRUE(S_ISSOCK(status.st_mode));
}
