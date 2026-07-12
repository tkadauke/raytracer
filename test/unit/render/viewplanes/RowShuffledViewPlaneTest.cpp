#include <gtest/gtest.h>
#include "render/viewplanes/RowShuffledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace render;

  TEST(RowShuffledViewPlane, ShouldInitialize) {
    RowShuffledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }

  INSTANTIATE_TYPED_TEST_SUITE_P(RowShuffled, AbstractViewPlaneIteratorTest, RowShuffledViewPlane);
}
