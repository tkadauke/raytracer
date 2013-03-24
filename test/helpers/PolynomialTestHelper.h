#ifndef POLYNOMIAL_TEST_HELPER_H
#define POLYNOMIAL_TEST_HELPER_H

#include "math/Number.h"
#include "test/helpers/ContainerTestHelper.h"

namespace testing {
  namespace internal {
    template<class Container>
    bool containersNear(const Container& expected, const Container& actual, typename Container::value_type epsilon) {
      if (expected.size() != actual.size())
        return false;
      for (typename Container::const_iterator i = expected.begin(), j = actual.begin(); i != expected.end(); ++i, ++j) {
        if (!(isAlmost(*i, *j, epsilon)))
          return false;
      }
      return true;
    }

    template<class Container>
    // Helper function for implementing ASSERT_CONTAINERS_NEAR.
    AssertionResult ContainersNearPredFormat(const char* expr1,
                                             const char* expr2,
                                             const char* expr3,
                                             const Container& val1,
                                             const Container& val2,
                                             double epsilon) {
      if (containersNear(val1, val2, epsilon)) return AssertionSuccess();

      Message msg;
      msg << "The containers " << expr1 << " and " << expr2
          << " are not close to each other (within " << expr3 << "), where\n"
          << expr1 << " evaluates to ";
      outputContainer(msg, val1);
      msg << ",\n" << expr2 << " evaluates to ";
      outputContainer(msg, val2);
      msg << ".";
      return AssertionFailure(msg);
    }
  }
}

#define ASSERT_CONTAINERS_NEAR(val1, val2, epsilon) ASSERT_PRED_FORMAT3(::testing::internal::ContainersNearPredFormat, val1, val2, epsilon)

#endif
