#include <gtest/gtest.h>
#include "render/viewplanes/PointInterlacedViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  
  TEST(PointInterlacedViewPlane, ShouldInitialize) {
    render::PointInterlacedViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    PointInterlaced,
    AbstractViewPlaneIteratorTest,
    PointInterlacedViewPlane
  );
}
