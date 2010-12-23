#include "test/functional/support/RaytracerFeatureTest.h"

namespace SphereTest {
  using namespace ::testing;
  
  class SphereTest : public RaytracerFeatureTest {};
  
  TEST_F(SphereTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }
  
  TEST_F(SphereTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced sphere");
    when("i look at the origin");
    then("i should not see the sphere");
  }

  TEST_F(SphereTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }
}
