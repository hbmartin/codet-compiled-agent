#include "codet/detail/diagnostic_stream.hpp"
#ifdef INCLUDE_NLOHMANN_JSON_HPP_
#error "The byte/rendering diagnostic interface must not import JSON or the core model"
#endif
#include <gtest/gtest.h>

TEST(DiagnosticInterface, LightweightHeaderIsSelfContained) {
  codet::detail::DiagnosticState state;
  EXPECT_FALSE(state.pending);
}
