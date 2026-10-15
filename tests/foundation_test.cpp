#include "codet/core.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
#include "text_compat.h"
}
#include <array>
#include <cstring>
TEST(TextCompatibility, PaddingAndExactFieldsMatchLibc) {
  for (size_t len = 0; len < 96; ++len) {
    std::string source(len, 'x');
    for (size_t count = 0; count <= 96; ++count) {
      std::array<char, 128> expected{}, actual{};
      expected.fill('!');
      actual.fill('!');
      strncpy(expected.data() + 8, source.c_str(), count);
      EXPECT_EQ(app_copy_padded(actual.data() + 8, source.c_str(), count), actual.data() + 8);
      ASSERT_EQ(expected, actual) << len << ":" << count;
    }
  }
}
TEST(TextCompatibility, TruncationAndReturnValuesMatchLibc) {
  for (size_t cap = 0; cap <= 96; ++cap) {
    std::array<char, 128> expected{}, actual{};
    expected.fill('!');
    actual.fill('!');
    EXPECT_EQ(
        snprintf(expected.data() + 8, cap, "%s/%d/%08x/%.3f", "long field", -42, 0x1234u, 1.234567),
        app_format_truncated(actual.data() + 8, cap, "%s/%d/%08x/%.3f", "long field", -42, 0x1234u,
                             1.234567));
    EXPECT_EQ(expected, actual);
  }
}
TEST(TextCompatibility, ZeroCapacityAcceptsNull) {
  EXPECT_EQ(app_format_truncated(nullptr, 0, "%s:%d", "length", 42), 9);
}
