#include "test/functional/support/RaytracerFeatureTest.h"

namespace FlatMeshTriangleTest {
  using namespace ::testing;
  
  class FlatMeshTriangleTest : public RaytracerFeatureTest {};
  
  TEST_F(FlatMeshTriangleTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered flat mesh triangle");
    when("i look at the origin");
    then("i should see the triangle");
  }
  
  TEST_F(FlatMeshTriangleTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced flat mesh triangle");
    when("i look at the origin");
    then("i should not see the triangle");
  }

  TEST_F(FlatMeshTriangleTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered flat mesh triangle");
    when("i look away from the origin");
    then("i should not see the triangle");
  }
}
