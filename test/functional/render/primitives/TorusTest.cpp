#include "test/functional/support/RaytracerFeatureTest.h"

namespace TorusTest {
  using namespace ::testing;
  
  class TorusTest : public RaytracerFeatureTest {};
  
  TEST_F(TorusTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered torus");
    when("i look at the origin");
    // A torus seen from the side is an elongated ring, not a circle
    // — visibility check only.
    then("i should see something");
  }

  TEST_F(TorusTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered torus");
    when("i look away from the origin");
    then("i should not see the torus");
  }
  
  TEST_F(TorusTest, ShouldHaveAHoleInTheMiddle) {
    given("a torus rotated 90 degrees around the x axis");
    when("i look at the origin");
    then("i should see the torus with a hole in the middle");
  }
}
