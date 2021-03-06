#include "gtest.h"
#include "core/InPlaceSetOperators.h"

using namespace std;

namespace InPlaceSetOperatorsTest {
  using namespace ::testing;
  
  struct ConcreteSet : public InPlaceSetOperators<ConcreteSet> {
    inline ConcreteSet()
      : value(0)
    {
    }

    inline ConcreteSet(int v)
      : value(v)
    {
    }

    inline ConcreteSet operator|(const ConcreteSet&) {
      return ConcreteSet(17);
    }

    inline ConcreteSet operator&(const ConcreteSet&) {
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
