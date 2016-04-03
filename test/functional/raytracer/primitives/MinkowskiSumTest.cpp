#include "test/functional/support/RaytracerFeatureTest.h"

namespace MinkowskiSumTest {
  using namespace ::testing;
  
  class MinkowskiSumTest : public RaytracerFeatureTest {};
  
  TEST_F(MinkowskiSumTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered minkowski sum with two boxes");
    when("i look at the origin");
    then("i should see the box");
  }
  
  TEST_F(MinkowskiSumTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced minkowski sum with two boxes");
    when("i look at the origin");
    then("i should not see the box");
  }

  TEST_F(MinkowskiSumTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered minkowski sum with two boxes");
    when("i look away from the origin");
    then("i should not see the box");
  }
}
