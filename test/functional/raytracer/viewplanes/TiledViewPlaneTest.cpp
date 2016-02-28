#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "raytracer/viewplanes/TiledViewPlane.h"

namespace TiledViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  INSTANTIATE_TYPED_TEST_CASE_P(
    Tiled,
    AbstractViewPlaneTest,
    TiledViewPlane
  );
}
