#include <gtest/gtest.h>
#include "render/viewplanes/PointInterlacedViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  
  TEST(PointInterlacedViewPlane, ShouldInitialize) {
    render::PointInterlacedViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }

  TEST(PointInterlacedViewPlane, ShouldChooseInitialPixelSizeFromFullViewPlaneForTiledRects) {
    render::PointInterlacedViewPlane plane;
    plane.setup(Matrix4d(), Recti(0, 0, 800, 600));

    auto iterator = plane.begin(Recti(0, 0, 160, 300));

    EXPECT_EQ(64, iterator.pixelSize());
  }
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    PointInterlaced,
    AbstractViewPlaneIteratorTest,
    PointInterlacedViewPlane
  );
}
