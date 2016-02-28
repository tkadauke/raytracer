#ifndef MATRIX_TEST_HELPER_H
#define MATRIX_TEST_HELPER_H

namespace testing {
  namespace internal {
    template<int Dimensions, class T, class Derived, class VectorType>
    bool matrixNear(
      const Matrix<Dimensions, T, Derived, VectorType>& expected,
      const Matrix<Dimensions, T, Derived, VectorType>& actual,
      const T& threshold = 0.0001
    ) {
      for (int row = 0; row != Dimensions; ++row) {
        for (int col = 0; col != Dimensions; ++col) {
          if (actual[row][col] < expected[row][col] - threshold || actual[row][col] > expected[row][col] + threshold)
            return false;
        }
      }
      return true;
    }

    template<int Dimensions, class T, class Derived, class VectorType>
    // Helper function for implementing ASSERT_MATRIX_NEAR.
    AssertionResult MatrixNearPredFormat(
      const char* expr1,
      const char* expr2,
      const char* abs_error_expr,
      const Matrix<Dimensions, T, Derived, VectorType>& val1,
      const Matrix<Dimensions, T, Derived, VectorType>& val2,
      double abs_error
    ) {
      if (matrixNear(val1, val2, T(abs_error))) return AssertionSuccess();

      Message msg;
      msg << "The difference between matrixes " << expr1 << " and " << expr2
          << " exceeds " << abs_error_expr << ", where\n"
          << expr1 << " evaluates to " << val1 << ",\n"
          << expr2 << " evaluates to " << val2 << ", and\n"
          << abs_error_expr << " evaluates to " << abs_error << ".";
      return AssertionFailure(msg);
    }
  }
}

#define ASSERT_MATRIX_NEAR(val1, val2, abs_error) ASSERT_PRED_FORMAT3(::testing::internal::MatrixNearPredFormat, val1, val2, abs_error)

#endif
