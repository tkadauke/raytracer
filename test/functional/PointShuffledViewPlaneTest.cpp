#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "PointShuffledViewPlane.h"

namespace PointShuffledViewPlaneTest {
  using namespace ::testing;
  
  INSTANTIATE_TYPED_TEST_CASE_P(PointShuffled, AbstractViewPlaneTest, PointShuffledViewPlane);
}