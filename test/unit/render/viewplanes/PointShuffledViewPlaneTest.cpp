#include <gtest/gtest.h>
#include "render/viewplanes/PointShuffledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace PointShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  
  TEST(PointShuffledViewPlane, ShouldInitialize) {
    PointShuffledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    PointShuffled,
    AbstractViewPlaneIteratorTest,
    PointShuffledViewPlane
  );
}
