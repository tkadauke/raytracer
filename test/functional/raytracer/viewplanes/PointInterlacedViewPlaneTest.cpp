#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "raytracer/viewplanes/PointInterlacedViewPlane.h"

namespace PointInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  INSTANTIATE_TYPED_TEST_CASE_P(
    PointInterlaced,
    AbstractViewPlaneTest,
    PointInterlacedViewPlane
  );
}
