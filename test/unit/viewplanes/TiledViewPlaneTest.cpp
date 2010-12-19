#include "gtest.h"
#include "viewplanes/TiledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace TiledViewPlaneTest {
  using namespace ::testing;
  
  TEST(TiledViewPlane, ShouldInitialize) {
    TiledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(Tiled, AbstractViewPlaneIteratorTest, TiledViewPlane);
  
  namespace Iterator {
    TEST(TiledViewPlane_Iterator, ShouldReturnTrueWhenTwoBeginIteratorsAreCompared) {
      TiledViewPlane plane;
      Rect fullRect(8, 6);
      plane.setup(Matrix4d(), fullRect);
      ASSERT_TRUE(plane.begin(fullRect) == plane.begin(fullRect));
    }
  }
}
