#include "gtest.h"
#include "TiledViewPlane.h"
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
      plane.setup(Matrix4d(), 8, 6);
      ASSERT_TRUE(plane.begin() == plane.begin());
    }
  }
}
