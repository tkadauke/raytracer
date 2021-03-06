#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "raytracer/viewplanes/RowInterlacedViewPlane.h"

namespace RowInterlacedViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  INSTANTIATE_TYPED_TEST_CASE_P(
    RowInterlaced,
    AbstractViewPlaneTest,
    RowInterlacedViewPlane
  );
}
