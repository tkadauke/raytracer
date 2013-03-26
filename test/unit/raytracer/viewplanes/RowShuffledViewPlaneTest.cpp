#include "gtest.h"
#include "raytracer/viewplanes/RowShuffledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(RowShuffledViewPlane, ShouldInitialize) {
    RowShuffledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowShuffled, AbstractViewPlaneIteratorTest, RowShuffledViewPlane);
}
