#ifndef VECTOR_TEST_HELPER_H
#define VECTOR_TEST_HELPER_H

template<int Dimensions, class T>
bool vectorNear(const Vector<Dimensions, T>& expected, const Vector<Dimensions, T>& actual, const T& threshold = 0.0001) {
  for (int i = 0; i != Dimensions; ++i) {
    if (actual[i] < expected[i] - threshold || actual[i] > expected[i] + threshold)
      return false;
  }
  return true;
}

namespace testing {
  namespace internal {
    template<int Dimensions, class T>
    // Helper function for implementing ASSERT_VECTOR_NEAR.
    AssertionResult VectorNearPredFormat(const char* expr1,
                                         const char* expr2,
                                         const char* abs_error_expr,
                                         const Vector<Dimensions, T>& val1,
                                         const Vector<Dimensions, T>& val2,
                                         double abs_error) {
      if (vectorNear(val1, val2, T(abs_error))) return AssertionSuccess();

      Message msg;
      msg << "The difference between vectors " << expr1 << " and " << expr2
          << " exceeds " << abs_error_expr << ", where\n"
          << expr1 << " evaluates to " << val1 << ",\n"
          << expr2 << " evaluates to " << val2 << ", and\n"
          << abs_error_expr << " evaluates to " << abs_error << ".";
      return AssertionFailure(msg);
    }
  }
}

#define ASSERT_VECTOR_NEAR(val1, val2, abs_error) ASSERT_PRED_FORMAT3(::testing::internal::VectorNearPredFormat, val1, val2, abs_error)

#endif
