#include "gtest.h"
#include "PointInterlacedViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  
  TEST(PointInterlacedViewPlane, ShouldInitialize) {
    PointInterlacedViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(PointInterlaced, AbstractViewPlaneIteratorTest, PointInterlacedViewPlane);
}
