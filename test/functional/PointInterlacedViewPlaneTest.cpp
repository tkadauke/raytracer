#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "PointInterlacedViewPlane.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  
  INSTANTIATE_TYPED_TEST_CASE_P(PointInterlaced, AbstractViewPlaneTest, PointInterlacedViewPlane);
}
