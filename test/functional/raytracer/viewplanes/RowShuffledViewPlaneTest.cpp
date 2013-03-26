#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "raytracer/viewplanes/RowShuffledViewPlane.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  INSTANTIATE_TYPED_TEST_CASE_P(RowShuffled, AbstractViewPlaneTest, RowShuffledViewPlane);
}
