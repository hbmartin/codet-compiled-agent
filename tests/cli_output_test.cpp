#include "codet/detail/cli_output.hpp"
#include <gtest/gtest.h>
#include <sstream>
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
  EXPECT_TRUE(output.failed());
  EXPECT_TRUE(stream.str().empty());
}
} // namespace
