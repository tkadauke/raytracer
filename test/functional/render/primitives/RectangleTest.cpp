#include "test/functional/support/RaytracerFeatureTest.h"

namespace RectangleTest {
  using namespace ::testing;
  
  class RectangleTest : public RaytracerFeatureTest {};
  
  TEST_F(RectangleTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered rectangle");
    when("i look at the origin");
    then("i should see the rectangle");
  }
  
  TEST_F(RectangleTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced rectangle");
    when("i look at the origin");
    then("i should not see the rectangle");
  }

  TEST_F(RectangleTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered rectangle");
    when("i look away from the origin");
    then("i should not see the rectangle");
  }
}
