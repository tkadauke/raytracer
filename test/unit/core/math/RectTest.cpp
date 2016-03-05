#include "gtest.h"
#include "core/math/Rect.h"

#include <sstream>

using namespace std;

namespace RectTest {
  using namespace ::testing;
  
  template<class T>
  class RectTest : public ::testing::Test {
  };

  typedef ::testing::Types<
    int, float, double
  > RectTypes;

  TYPED_TEST_CASE(RectTest, RectTypes);
  
  TYPED_TEST(RectTest, ShouldInitialize) {
    Rect<TypeParam> rect;
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(0, rect.width());
    ASSERT_EQ(0, rect.height());
  }

  TYPED_TEST(RectTest, ShouldInitializeWithWidthAndHeight) {
    Rect<TypeParam> rect(8, 6);
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(8, rect.width());
    ASSERT_EQ(6, rect.height());
  }
  
  TYPED_TEST(RectTest, ShouldInitializeWithValues) {
    Rect<TypeParam> rect(2, 3, 8, 6);
    ASSERT_EQ(2, rect.x());
    ASSERT_EQ(3, rect.y());
    ASSERT_EQ(8, rect.width());
    ASSERT_EQ(6, rect.height());
  }
  
  TYPED_TEST(RectTest, ShouldCalculateRightEnd) {
    Rect<TypeParam> rect(2, 3, 8, 6);
    ASSERT_EQ(10, rect.right());
  }
  
  TYPED_TEST(RectTest, ShouldCalculateBottomEnd) {
    Rect<TypeParam> rect(2, 3, 8, 6);
    ASSERT_EQ(9, rect.bottom());
  }
  
  TYPED_TEST(RectTest, ShouldStreamToString) {
    Rect<TypeParam> rect(2, 3, 8, 6);
    ostringstream str;
    str << rect;
    ASSERT_EQ("Rect(2, 3 to 10, 9)", str.str());
  }
}
