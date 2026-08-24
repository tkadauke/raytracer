#ifndef VECTOR_TEST_HELPER_H
#define VECTOR_TEST_HELPER_H

#include "test/helpers/NearTestHelper.h"

namespace testing {
  namespace internal {
    template<int Dimensions, class T, class StorageCellType, class Derived>
    bool vectorNear(const Vector<Dimensions, T, StorageCellType, Derived>& expected,
                    const Vector<Dimensions, T, StorageCellType, Derived>& actual,
                    const T& threshold = 0.0001) {
      return elementwiseNear(expected, actual, threshold, Dimensions,
                             [](const auto& v, int i) -> const T& { return v[i]; });
    }

    template<int Dimensions, class T, class StorageCellType, class Derived>
    // Helper function for implementing ASSERT_VECTOR_NEAR.
    AssertionResult
    VectorNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr,
                         const Vector<Dimensions, T, StorageCellType, Derived>& val1,
                         const Vector<Dimensions, T, StorageCellType, Derived>& val2,
                         double abs_error) {
      return nearAssertionResult("vectors", expr1, expr2, abs_error_expr, val1, val2, abs_error,
                                 vectorNear(val1, val2, T(abs_error)));
    }
  }
}

#define ASSERT_VECTOR_NEAR(val1, val2, abs_error)                                                  \
  ASSERT_PRED_FORMAT3(::testing::internal::VectorNearPredFormat, val1, val2, abs_error)

#define EXPECT_VECTOR_NEAR(val1, val2, abs_error)                                                  \
  EXPECT_PRED_FORMAT3(::testing::internal::VectorNearPredFormat, val1, val2, abs_error)

#endif
