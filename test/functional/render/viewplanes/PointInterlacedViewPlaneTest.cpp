#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "render/viewplanes/PointInterlacedViewPlane.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    PointInterlaced,
    AbstractViewPlaneTest,
    PointInterlacedViewPlane
  );
}
