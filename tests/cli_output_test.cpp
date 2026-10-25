#include "codet/detail/cli_output.hpp"
#include "process_helper.hpp"
#include <future>
#include <gtest/gtest.h>
#include <sstream>
#include <sys/mman.h>
using namespace codet;
namespace {
struct OutputStream : std::stringbuf {
  size_t attempts = 0;
  bool short_write = false, flush_failure = false;
  std::streamsize xsputn(const char *bytes, std::streamsize size) override {
    ++attempts;
    return std::stringbuf::xsputn(bytes, short_write ? std::min(size, std::streamsize{2}) : size);
  }
  int sync() override {
    return flush_failure ? -1 : 0;
  }
};
TEST(CliOutput, FailedRequiredRecordIsNotRetriedOrFollowedByAnErrorRecord) {
  for (bool flush : {false, true}) {
    OutputStream buffer;
    buffer.short_write = !flush;
    buffer.flush_failure = flush;
    std::ostream stream(&buffer);
    detail::CliOutput output(stream);
    EXPECT_THROW(output.required({"{\"result\":42}"}), Error);
    const auto attempts = buffer.attempts;
    buffer.short_write = buffer.flush_failure = false;
    output.secondary({"{\"error\":true}"});
    EXPECT_THROW(output.required({"later"}), Error);
    EXPECT_EQ(buffer.attempts, attempts);
    EXPECT_TRUE(output.failed());
    EXPECT_EQ(output.status(0), 1);
  }
}
TEST(CliOutput, InteractiveDisplayRecoversButSuccessfulQuitStillReportsLoss) {
  for (bool mask : {false, true}) {
    OutputStream buffer;
    std::ostream stream(&buffer);
    if (mask)
      stream.exceptions(std::ios::badbit | std::ios::failbit);
    detail::CliOutput output(stream);
    buffer.flush_failure = true;
    EXPECT_FALSE(output.display({"lost reply"}));
    buffer.flush_failure = false;
    EXPECT_TRUE(output.display({"later reply"}));
    EXPECT_NE(buffer.str().find("later reply\n"), std::string::npos);
    EXPECT_EQ(output.status(0), 1);
    EXPECT_EQ(output.status(130), 130);
    EXPECT_EQ(output.status(2), 2);
  }
}
TEST(CliOutput, SecondaryReportingPreservesTheOperationException) {
  OutputStream buffer;
  buffer.flush_failure = true;
  std::ostream stream(&buffer);
  stream.exceptions(std::ios::badbit);
  detail::CliOutput output(stream);
  const auto original = std::make_exception_ptr(Error("provider failed"));
  try {
    try {
      std::rethrow_exception(original);
    } catch (...) {
      output.secondary({"error record"});
      throw;
    }
  } catch (...) {
    EXPECT_EQ(std::current_exception(), original);
  }
  EXPECT_TRUE(output.failed());
}
TEST(CliOutput, MissingBufferAndConfiguredExceptionsCannotBecomeSuccess) {
  std::ostream stream(nullptr);
  detail::CliOutput output(stream);
  EXPECT_THROW(stream.exceptions(std::ios::badbit), std::ios_base::failure);
  EXPECT_THROW(output.required({"required"}), std::ios_base::failure);
  EXPECT_EQ(output.status(0), 1);
  EXPECT_EQ(stream.exceptions(), std::ios::badbit);
}
TEST(CliOutput, SecondarySerializationCannotReplaceAnExistingFailure) {
  std::ostringstream stream;
  detail::CliOutput output(stream);
  EXPECT_NO_THROW(output.secondary_record([]() -> std::string { throw std::bad_alloc(); }));
  EXPECT_FALSE(output.failed());
  EXPECT_TRUE(stream.str().empty());
  EXPECT_NO_THROW(output.required({"later"}));
}
TEST(CliOutput, NonblockingStdoutRecoversCppAndCStateAfterThePipeDrains) {
  const auto child = test::start_helper({"--codet-stdout-recovery-helper"});
  ASSERT_GT(child, 0);
  int status = 0;
  ASSERT_TRUE(test::wait_helper(child, status));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}
struct DescriptorProbe {
  std::string delivered;
  int attempts = 0, interrupted = 0, fail = 0, fail_after = -1;
  size_t chunk = 1000000;
  static ssize_t write(void *context, int, const void *data, size_t size) {
    auto &probe = *static_cast<DescriptorProbe *>(context);
    ++probe.attempts;
    if (probe.fail_after >= 0 && probe.attempts > probe.fail_after) {
      errno = EAGAIN;
      return -1;
    }
    if (probe.interrupted-- > 0) {
      errno = EINTR;
      return -1;
    }
    if (probe.fail) {
      errno = probe.fail;
      return -1;
    }
    size = std::min(size, probe.chunk);
    probe.delivered.append(static_cast<const char *>(data), size);
    return static_cast<ssize_t>(size);
  }
  detail::DescriptorOutput writer() {
    return detail::DescriptorOutput(123, write, this);
  }
};
TEST(CliOutput, DescriptorWritesFinishShortWritesAndInterruptedCalls) {
  DescriptorProbe probe;
  probe.chunk = 2;
  probe.interrupted = 2;
  detail::CliOutput output(probe.writer());
  output.required({"first", "second"});
  EXPECT_EQ(probe.delivered, "firstsecond\n");
  EXPECT_EQ(output.status(0), 0);
}
TEST(CliOutput, DescriptorFailuresLatchWithoutRetryingOrFlushingAtDestruction) {
  for (int error : {EAGAIN, EPIPE, 0}) {
    DescriptorProbe probe;
    probe.fail = error;
    if (!error)
      probe.chunk = 0;
    {
      detail::CliOutput output(probe.writer());
      EXPECT_THROW(output.required({"lost"}), Error);
      EXPECT_EQ(output.status(0), 1);
      probe.fail = 0;
      probe.chunk = 100;
      output.secondary({"error"});
      EXPECT_THROW(output.required({"later"}), Error);
      EXPECT_EQ(probe.attempts, 1);
    }
    EXPECT_TRUE(probe.delivered.empty());
    EXPECT_EQ(probe.attempts, 1);
  }
}
TEST(CliOutput, TextExportsBatchExactBytesAndDiscardUnpublishedTails) {
  DescriptorProbe probe;
  detail::CliOutput output(probe.writer());
  const std::string message(1000, 'x');
  {
    detail::TextExport exported(output);
    for (int i = 0; i != 100; ++i)
      exported.append(message);
    exported.finish();
    exported.finish();
  }
  EXPECT_EQ(probe.delivered, std::string(100000, 'x'));
  EXPECT_EQ(probe.attempts, 2);
  {
    detail::TextExport discarded(output);
    discarded.append("must not flush from a destructor");
  }
  EXPECT_EQ(probe.attempts, 2);
}
TEST(CliOutput, HumanExportsRenderAcrossStagingBoundariesWithoutChangingRawExports) {
  const std::string raw = std::string(65534, 'x') + "\xe9\x9b\xaa\r\n\x1b\r\\x1B";
  DescriptorProbe probe;
  detail::CliOutput output(probe.writer());
  detail::TextExport exported(output);
  exported.append_human(raw);
  exported.finish();
  EXPECT_EQ(probe.delivered, std::string(65534, 'x') + "\xe9\x9b\xaa\n\\x1B\\x0D\\x1B");
  EXPECT_EQ(probe.attempts, 2);
  probe.delivered.clear();
  exported.append(raw);
  exported.finish();
  EXPECT_EQ(probe.delivered, raw);
}
TEST(CliOutput, PartialDescriptorRecordDoesNotReplayItsPrefixOrUnwrittenSuffix) {
  DescriptorProbe probe;
  probe.chunk = 2;
  probe.fail_after = 1;
  {
    detail::CliOutput output(probe.writer());
    EXPECT_THROW(output.required({"record"}), Error);
    EXPECT_EQ(probe.delivered, "re");
    probe.fail_after = -1;
    output.secondary({"error record"});
    EXPECT_THROW(output.required({"later"}), Error);
  }
  EXPECT_EQ(probe.attempts, 2);
  EXPECT_EQ(probe.delivered, "re");
}
TEST(CliOutput, CancelledInterruptedAndPartialWritesDoNotBecomeDeliveryFailures) {
  for (bool interrupted : {false, true}) {
    struct Probe {
      Cancellation cancel;
      bool interrupted;
      int calls = 0;
      static ssize_t write(void *context, int, const void *, size_t) {
        auto &self = *static_cast<Probe *>(context);
        ++self.calls;
        self.cancel.requested = true;
        errno = EINTR;
        return self.interrupted ? -1 : 1;
      }
    } probe;
    probe.interrupted = interrupted;
    detail::CliOutput output(detail::DescriptorOutput(123, Probe::write, &probe, &probe.cancel));
    try {
      output.required({"required"});
      FAIL();
    } catch (const Error &error) {
      EXPECT_EQ(error.code, 130);
    }
    EXPECT_FALSE(output.failed());
    EXPECT_EQ(output.status(0), 130);
    EXPECT_EQ(output.status(2), 2);
    probe.cancel.requested = false;
    bool built = false;
    output.secondary_record([&] {
      built = true;
      return "secondary";
    });
    EXPECT_THROW(output.required({"later"}), Error);
    EXPECT_FALSE(built);
    EXPECT_EQ(probe.calls, 1);
  }
}
TEST(CliOutput, FullBlockingPipeObservesCancellationSetByAnotherThread) {
  int descriptors[2];
  ASSERT_EQ(::pipe(descriptors), 0);
  struct Close {
    int read, write;
    ~Close() {
      ::close(read);
      ::close(write);
    }
  } close{descriptors[0], descriptors[1]};
  const auto flags = ::fcntl(close.write, F_GETFL);
  ASSERT_GE(flags, 0);
  ASSERT_EQ(::fcntl(close.write, F_SETFL, flags | O_NONBLOCK), 0);
  char bytes[4096]{};
  while (::write(close.write, bytes, sizeof(bytes)) > 0) {
  }
  ASSERT_EQ(errno, EAGAIN);
  ASSERT_EQ(::fcntl(close.write, F_SETFL, flags), 0);
  Cancellation cancel;
  detail::CliOutput output(detail::DescriptorOutput(close.write, cancel));
  std::promise<void> started;
  auto waiting = std::async(std::launch::async, [&] {
    started.set_value();
    try {
      output.required({"never delivered"});
      return 0;
    } catch (const Error &error) {
      return error.code;
    }
  });
  started.get_future().wait();
  EXPECT_EQ(waiting.wait_for(std::chrono::milliseconds(150)), std::future_status::timeout);
  cancel.requested = true;
  const auto completed = waiting.wait_for(std::chrono::seconds(2));
  if (completed != std::future_status::ready) {
    ::close(close.read);
    close.read = -1;
  }
  EXPECT_EQ(completed, std::future_status::ready);
  EXPECT_EQ(waiting.get(), 130);
  EXPECT_FALSE(output.failed());
  EXPECT_EQ(::fcntl(close.write, F_GETFL) & O_NONBLOCK, 0);
}
TEST(CliOutput, DescriptorChunksRespectTheDarwinLimitWithoutReadingHugeStorage) {
  const size_t size = static_cast<size_t>(INT_MAX) + 17;
  void *storage = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (storage == MAP_FAILED)
    GTEST_SKIP() << "Cannot reserve the oversized virtual test view";
  struct Mapping {
    void *data;
    size_t size;
    ~Mapping() {
      ::munmap(data, size);
    }
  } mapping{storage, size};
  struct Probe {
    size_t bytes = 0, largest = 0, calls = 0;
    static ssize_t write(void *context, int, const void *, size_t count) {
      auto &self = *static_cast<Probe *>(context);
      self.bytes += count;
      self.largest = std::max(self.largest, count);
      ++self.calls;
      return static_cast<ssize_t>(count);
    }
  } probe;
  EXPECT_TRUE(detail::DescriptorOutput(123, Probe::write, &probe)
                  .write({std::string_view(static_cast<const char *>(storage), size)}, false));
  EXPECT_EQ(probe.bytes, size);
  EXPECT_EQ(probe.largest, static_cast<size_t>(INT_MAX));
  EXPECT_EQ(probe.calls, 2u);
}
} // namespace
