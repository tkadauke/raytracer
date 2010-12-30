#include "gtest.h"
#include "core/InequalityOperator.h"

using namespace std;

namespace InequalityOperatorTest {
  using namespace ::testing;
  
  struct AlwaysEqual : public InequalityOperator<AlwaysEqual> {
    bool operator==(const AlwaysEqual&) {
      return true;
    }
  };
  
  struct NeverEqual : public InequalityOperator<NeverEqual> {
    bool operator==(const NeverEqual&) {
      return false;
    }
  };
  
  TEST(InequalityOperator, ShouldNotBeEqualForSameInstance) {
    AlwaysEqual instance;
    ASSERT_FALSE(instance != instance);
  }
  
  TEST(InequalityOperator, ShouldBeInequalIfEqualityOperatorReturnsFalse) {
    AlwaysEqual instance1, instance2;
    ASSERT_FALSE(instance1 != instance2);
  }

  TEST(InequalityOperator, ShouldBeInequalIfEqualityOperatorReturnsTrue) {
    NeverEqual instance1, instance2;
    ASSERT_TRUE(instance1 != instance2);
  }
}
