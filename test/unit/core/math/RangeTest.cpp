#include "gtest.h"
#include "core/math/Range.h"

#include <cmath>

#include <sstream>

using namespace std;

namespace RangeTest {
  template<class T>
  class RangeTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> RangeTypes;
  TYPED_TEST_CASE(RangeTest, RangeTypes);
  
  TYPED_TEST(RangeTest, ShouldInitialize) {
    Range<TypeParam> range(3, 5);
    ASSERT_EQ(3, range.begin());
    ASSERT_EQ(5, range.end());
  }
  
  TYPED_TEST(RangeTest, ShouldTestIfRangeContainsNumber) {
    Range<TypeParam> range(3, 5);
    ASSERT_TRUE(range.contains(4));
    ASSERT_TRUE(range.contains(3));
    ASSERT_FALSE(range.contains(7));
  }
  
  TYPED_TEST(RangeTest, ShouldClampValueToRange) {
    Range<TypeParam> range(3, 5);
    ASSERT_EQ(4, range.clamp(4));
    ASSERT_EQ(3, range.clamp(3));
    ASSERT_EQ(3, range.clamp(1));
    ASSERT_EQ(5, range.clamp(7));
  }
  
  TYPED_TEST(RangeTest, ShouldReturnRandomValue) {
    Range<TypeParam> range(3, 5);
    auto v = range.random();
    ASSERT_TRUE(v >= 3);
    ASSERT_TRUE(v <= 5);
  }
  
  TYPED_TEST(RangeTest, ShouldStreamToString) {
    Range<TypeParam> range(3, 5);
    ostringstream str;
    str << range;
    ASSERT_EQ("[3, 5]", str.str());
  }
}
