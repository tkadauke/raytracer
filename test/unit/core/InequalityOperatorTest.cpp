#include "gtest.h"
#include "core/InequalityOperator.h"

using namespace std;

namespace InequalityOperatorTest {
  using namespace ::testing;
  
  struct AlwaysEqual : public InequalityOperator<AlwaysEqual> {
    inline bool operator==(const AlwaysEqual&) const {
      return true;
    }
  };
  
  struct NeverEqual : public InequalityOperator<NeverEqual> {
    inline bool operator==(const NeverEqual&) const {
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
