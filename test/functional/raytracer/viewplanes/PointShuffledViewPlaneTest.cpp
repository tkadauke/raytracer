#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "raytracer/viewplanes/PointShuffledViewPlane.h"

namespace PointShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  INSTANTIATE_TYPED_TEST_CASE_P(
    PointShuffled,
    AbstractViewPlaneTest,
    PointShuffledViewPlane
  );
}
