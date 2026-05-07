#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "render/viewplanes/TiledViewPlane.h"

namespace TiledViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    Tiled,
    AbstractViewPlaneTest,
    TiledViewPlane
  );
}
