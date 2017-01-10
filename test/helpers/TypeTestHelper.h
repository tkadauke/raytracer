#ifndef TYPE_TEST_HELPER_H
#define TYPE_TEST_HELPER_H

namespace testing {
  namespace internal {
    template<class S, class T>
    struct TypesEqual {
      static const bool Value = false;
    };
    
    template<class T>
    struct TypesEqual<T, T> {
      static const bool Value = true;
    };
    
    template<class S, class T>
    // Helper function for implementing ASSERT_TYPES_EQ.
    AssertionResult TypesEqualPredFormat(
      const char* expr1,
      const char* expr2,
      const S&,
      const T&
    ) {
      if (TypesEqual<S, T>::Value) return AssertionSuccess();

      Message msg;
      msg << "The types of " << expr1 << " and " << expr2
          << " were not equal";
      return AssertionFailure(msg);
    }
  }
}

#define ASSERT_TYPES_EQ(expected, actual) \
  ASSERT_PRED_FORMAT2(::testing::internal::TypesEqualPredFormat, expected, actual)

#endif
