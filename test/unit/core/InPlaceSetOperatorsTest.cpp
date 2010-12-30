#include "gtest.h"
#include "core/InPlaceSetOperators.h"

using namespace std;

namespace InPlaceSetOperatorsTest {
  using namespace ::testing;
  
  struct ConcreteSet : public InPlaceSetOperators<ConcreteSet> {
    ConcreteSet()
      : value(0) {}
    ConcreteSet(int v)
      : value(v) {}
    ConcreteSet operator|(const ConcreteSet&) {
      return ConcreteSet(17);
    }
    ConcreteSet operator&(const ConcreteSet&) {
      return ConcreteSet(42);
    }
    
    int value;
  };
  
  TEST(InPlaceSetOperators, ShouldBuildUnionInPlace) {
    ConcreteSet set1, set2;
    set1 |= set2;
    ASSERT_EQ(17, set1.value);
  }
  
  TEST(InPlaceSetOperators, ShouldBuildIntersectionInPlace) {
    ConcreteSet set1, set2;
    set1 &= set2;
    ASSERT_EQ(42, set1.value);
  }
}
