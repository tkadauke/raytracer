#include "gtest.h"
#include "core/math/Rect.h"

#include <sstream>

using namespace std;

namespace RectTest {
  TEST(Rect, ShouldInitialize) {
    Rect rect;
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(0, rect.width());
    ASSERT_EQ(0, rect.height());
  }

  TEST(Rect, ShouldInitializeWithWidthAndHeight) {
    Rect rect(8, 6);
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(8, rect.width());
    ASSERT_EQ(6, rect.height());
  }
  
  TEST(Rect, ShouldInitializeWithValues) {
    Rect rect(2, 3, 8, 6);
    ASSERT_EQ(2, rect.x());
    ASSERT_EQ(3, rect.y());
    ASSERT_EQ(8, rect.width());
    ASSERT_EQ(6, rect.height());
  }
  
  TEST(Rect, ShouldCalculateRightEnd) {
    Rect rect(2, 3, 8, 6);
    ASSERT_EQ(10, rect.right());
  }
  
  TEST(Rect, ShouldCalculateBottomEnd) {
    Rect rect(2, 3, 8, 6);
    ASSERT_EQ(9, rect.bottom());
  }
  
  TEST(Rect, ShouldStreamToString) {
    Rect rect(2, 3, 8, 6);
    ostringstream str;
    str << rect;
    ASSERT_EQ("Rect(2, 3 to 10, 9)", str.str());
  }
}
