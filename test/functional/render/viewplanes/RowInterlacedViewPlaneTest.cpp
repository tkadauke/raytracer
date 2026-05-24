#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "render/viewplanes/RowInterlacedViewPlane.h"

namespace RowInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace render;

  INSTANTIATE_TYPED_TEST_SUITE_P(RowInterlaced, AbstractViewPlaneTest, RowInterlacedViewPlane);
}
