#ifndef MATRIX_TEST_HELPER_H
#define MATRIX_TEST_HELPER_H

#include "test/helpers/NearTestHelper.h"

namespace testing {
  namespace internal {
    template<int Dimensions, class T, class VectorType, class Derived>
    bool matrixNear(const Matrix<Dimensions, T, VectorType, Derived>& expected,
                    const Matrix<Dimensions, T, VectorType, Derived>& actual,
                    const T& threshold = 0.0001) {
      return elementwiseNear(expected, actual, threshold, Dimensions * Dimensions,
                             [](const auto& m, int i) -> const T& {
                               return m[i / Dimensions][i % Dimensions];
                             });
    }

    template<int Dimensions, class T, class VectorType, class Derived>
    // Helper function for implementing ASSERT_MATRIX_NEAR.
    AssertionResult
    MatrixNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr,
                         const Matrix<Dimensions, T, VectorType, Derived>& val1,
                         const Matrix<Dimensions, T, VectorType, Derived>& val2, double abs_error) {
      return nearAssertionResult("matrixes", expr1, expr2, abs_error_expr, val1, val2, abs_error,
                                 matrixNear(val1, val2, T(abs_error)));
    }
  }
}

#define ASSERT_MATRIX_NEAR(val1, val2, abs_error)                                                  \
  ASSERT_PRED_FORMAT3(::testing::internal::MatrixNearPredFormat, val1, val2, abs_error)

#endif
