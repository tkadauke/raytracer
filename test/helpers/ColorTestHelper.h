#ifndef COLOR_TEST_HELPER_H
#define COLOR_TEST_HELPER_H

#include "test/helpers/NearTestHelper.h"

namespace testing {
  namespace internal {
    template<class T>
    bool colorNear(const Color<T>& expected, const Color<T>& actual, const T& threshold = 0.0001) {
      return elementwiseNear(expected, actual, threshold, 3,
                             [](const auto& c, int i) -> const T& { return c[i]; });
    }

    template<class T>
    // Helper function for implementing ASSERT_VECTOR_NEAR.
    AssertionResult ColorNearPredFormat(const char* expr1, const char* expr2,
                                        const char* abs_error_expr, const Color<T>& val1,
                                        const Color<T>& val2, double abs_error) {
      return nearAssertionResult("colors", expr1, expr2, abs_error_expr, val1, val2, abs_error,
                                 colorNear(val1, val2, T(abs_error)));
    }
  }
}

#define ASSERT_COLOR_NEAR(val1, val2, abs_error)                                                   \
  ASSERT_PRED_FORMAT3(::testing::internal::ColorNearPredFormat, val1, val2, abs_error)

#define EXPECT_COLOR_NEAR(val1, val2, abs_error)                                                   \
  EXPECT_PRED_FORMAT3(::testing::internal::ColorNearPredFormat, val1, val2, abs_error)

#endif
