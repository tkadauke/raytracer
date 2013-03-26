#include "gtest.h"
#include "raytracer/viewplanes/RowInterlacedViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace RowInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(RowInterlacedViewPlane, ShouldInitialize) {
    RowInterlacedViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowInterlaced, AbstractViewPlaneIteratorTest, RowInterlacedViewPlane);
}
