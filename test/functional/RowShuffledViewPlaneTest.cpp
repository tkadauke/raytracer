#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "RowShuffledViewPlane.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowShuffled, AbstractViewPlaneTest, RowShuffledViewPlane);
}
