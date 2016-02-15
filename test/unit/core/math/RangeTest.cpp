#include "gtest.h"
#include "core/math/Range.h"

#include <cmath>

#include <sstream>

using namespace std;

namespace RangeTest {
  template<class T>
  class RangeTest : public ::testing::Test {
  };

  typedef ::testing::Types<Range<float>, Range<double>> RangeTypes;
  TYPED_TEST_CASE(RangeTest, RangeTypes);
  
  TYPED_TEST(RangeTest, ShouldInitialize) {
    TypeParam range(3, 5);
    ASSERT_EQ(3, range.begin());
    ASSERT_EQ(5, range.end());
  }
  
  TYPED_TEST(RangeTest, ShouldTestIfRangeContainsNumber) {
    TypeParam range(3, 5);
    ASSERT_TRUE(range.contains(4));
    ASSERT_TRUE(range.contains(3));
    ASSERT_FALSE(range.contains(7));
  }
  
  TYPED_TEST(RangeTest, ShouldClampValueToRange) {
    TypeParam range(3, 5);
    ASSERT_EQ(4, range.clamp(4));
    ASSERT_EQ(3, range.clamp(3));
    ASSERT_EQ(3, range.clamp(1));
    ASSERT_EQ(5, range.clamp(7));
  }
  
  TYPED_TEST(RangeTest, ShouldStreamToString) {
    TypeParam range(3, 5);
    ostringstream str;
    str << range;
    ASSERT_EQ("[3, 5]", str.str());
  }
}
