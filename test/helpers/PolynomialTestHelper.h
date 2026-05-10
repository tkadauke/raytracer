#ifndef POLYNOMIAL_TEST_HELPER_H
#define POLYNOMIAL_TEST_HELPER_H

#include <type_traits>
#include "core/math/Number.h"
#include "test/helpers/ContainerTestHelper.h"

namespace testing {
  namespace internal {
    template<class Container1, class Container2>
    bool containersNear(const Container1& expected, const Container2& actual, double epsilon) {
      if (expected.size() != actual.size())
        return false;
      auto i = expected.begin();
      auto j = actual.begin();
      for (; i != expected.end(); ++i, ++j) {
        using V = std::decay_t<decltype(*i)>;
        if (!(isAlmost(*i, *j, static_cast<V>(epsilon))))
          return false;
      }
      return true;
    }

    template<class Container1, class Container2>
    // Helper function for implementing ASSERT_CONTAINERS_NEAR.
    AssertionResult ContainersNearPredFormat(
      const char* expr1,
      const char* expr2,
      const char* expr3,
      const Container1& val1,
      const Container2& val2,
      double epsilon
    ) {
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

#define ASSERT_CONTAINERS_NEAR(val1, val2, epsilon) \
  ASSERT_PRED_FORMAT3(::testing::internal::ContainersNearPredFormat, val1, val2, epsilon)

#endif
