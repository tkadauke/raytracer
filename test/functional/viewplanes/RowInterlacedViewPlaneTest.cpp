#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "viewplanes/RowInterlacedViewPlane.h"

namespace RowInterlacedViewPlaneTest {
  using namespace ::testing;
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowInterlaced, AbstractViewPlaneTest, RowInterlacedViewPlane);
}
