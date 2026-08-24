#ifndef NEAR_TEST_HELPER_H
#define NEAR_TEST_HELPER_H

namespace testing {
  namespace internal {
    template<class T, class Expected, class Actual, class ElementAt>
    bool elementwiseNear(const Expected& expected, const Actual& actual, const T& threshold,
                         int count, ElementAt elementAt) {
      for (int i = 0; i != count; ++i) {
        const T& e = elementAt(expected, i);
        const T& a = elementAt(actual, i);
        if (a < e - threshold || a > e + threshold)
          return false;
      }
      return true;
    }

    template<class V1, class V2>
    // Shared message formatting for the *NearPredFormat helpers below.
    AssertionResult nearAssertionResult(const char* noun, const char* expr1, const char* expr2,
                                        const char* abs_error_expr, const V1& val1, const V2& val2,
                                        double abs_error, bool isNear) {
      if (isNear)
        return AssertionSuccess();

      Message msg;
      msg << "The difference between " << noun << " " << expr1 << " and " << expr2 << " exceeds "
          << abs_error_expr << ", where\n"
          << expr1 << " evaluates to " << val1 << ",\n"
          << expr2 << " evaluates to " << val2 << ", and\n"
          << abs_error_expr << " evaluates to " << abs_error << ".";
      return AssertionFailure(msg);
    }
  }
}

#endif
