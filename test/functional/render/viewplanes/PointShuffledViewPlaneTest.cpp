#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "render/viewplanes/PointShuffledViewPlane.h"

namespace PointShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace render;

  INSTANTIATE_TYPED_TEST_SUITE_P(PointShuffled, AbstractViewPlaneTest, PointShuffledViewPlane);
}
