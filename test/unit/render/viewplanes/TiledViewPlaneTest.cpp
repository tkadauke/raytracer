#include <gtest/gtest.h>
#include "render/viewplanes/TiledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace TiledViewPlaneTest {
  using namespace ::testing;
  using namespace render;

  TEST(TiledViewPlane, ShouldInitialize) {
    render::TiledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }

  INSTANTIATE_TYPED_TEST_SUITE_P(Tiled, AbstractViewPlaneIteratorTest, TiledViewPlane);

  namespace Iterator {
    TEST(TiledViewPlane_Iterator, ShouldReturnTrueWhenTwoBeginIteratorsAreCompared) {
      render::TiledViewPlane plane;
      Recti fullRect(8, 6);
      plane.setup(Matrix4d(), fullRect);
      ASSERT_TRUE(plane.begin(fullRect) == plane.begin(fullRect));
    }
  }
}
