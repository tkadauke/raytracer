#include "gtest.h"
#include "RowShuffledViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  
  TEST(RowShuffledViewPlane, ShouldInitialize) {
    RowShuffledViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowShuffled, AbstractViewPlaneIteratorTest, RowShuffledViewPlane);
}
