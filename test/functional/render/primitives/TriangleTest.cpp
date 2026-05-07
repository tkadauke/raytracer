#include "test/functional/support/RaytracerFeatureTest.h"

namespace TriangleTest {
  using namespace ::testing;
  
  class TriangleTest : public RaytracerFeatureTest {};
  
  TEST_F(TriangleTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered triangle");
    when("i look at the origin");
    then("i should see the triangle");
  }
  
  TEST_F(TriangleTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced triangle");
    when("i look at the origin");
    then("i should not see the triangle");
  }

  TEST_F(TriangleTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered triangle");
    when("i look away from the origin");
    then("i should not see the triangle");
  }
}
