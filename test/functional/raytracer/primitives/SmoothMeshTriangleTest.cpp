#include "test/functional/support/RaytracerFeatureTest.h"

namespace SmoothMeshTriangleTest {
  using namespace ::testing;
  
  class SmoothMeshTriangleTest : public RaytracerFeatureTest {};
  
  TEST_F(SmoothMeshTriangleTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered smooth mesh triangle");
    when("i look at the origin");
    then("i should see the triangle");
  }
  
  TEST_F(SmoothMeshTriangleTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced smooth mesh triangle");
    when("i look at the origin");
    then("i should not see the triangle");
  }

  TEST_F(SmoothMeshTriangleTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered smooth mesh triangle");
    when("i look away from the origin");
    then("i should not see the triangle");
  }
}
