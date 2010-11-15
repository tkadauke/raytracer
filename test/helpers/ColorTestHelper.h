#ifndef COLOR_TEST_HELPER_H
#define COLOR_TEST_HELPER_H

template<class T>
bool colorNear(const Color<T>& expected, const Color<T>& actual, const T& threshold = 0.0001) {
  for (int i = 0; i != 3; ++i) {
    if (actual[i] < expected[i] - threshold || actual[i] > expected[i] + threshold)
      return false;
  }
  return true;
}

namespace testing {
  namespace internal {
    template<class T>
    // Helper function for implementing ASSERT_VECTOR_NEAR.
    AssertionResult ColorNearPredFormat(const char* expr1,
                                        const char* expr2,
                                        const char* abs_error_expr,
                                        const Color<T>& val1,
                                        const Color<T>& val2,
                                        double abs_error) {
      if (colorNear(val1, val2, T(abs_error))) return AssertionSuccess();

      Message msg;
      msg << "The difference between colors " << expr1 << " and " << expr2
          << " exceeds " << abs_error_expr << ", where\n"
          << expr1 << " evaluates to " << val1 << ",\n"
          << expr2 << " evaluates to " << val2 << ", and\n"
          << abs_error_expr << " evaluates to " << abs_error << ".";
      return AssertionFailure(msg);
    }
  }
}

#define ASSERT_COLOR_NEAR(val1, val2, abs_error) ASSERT_PRED_FORMAT3(::testing::internal::ColorNearPredFormat, val1, val2, abs_error)

#endif
