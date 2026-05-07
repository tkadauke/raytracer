#include "test/functional/support/RaytracerFeatureTest.h"

namespace TorusTest {
  using namespace ::testing;
  
  class TorusTest : public RaytracerFeatureTest {};
  
  TEST_F(TorusTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered torus");
    when("i look at the origin");
    then("i should see the torus");
  }

  TEST_F(TorusTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered torus");
    when("i look away from the origin");
    then("i should not see the torus");
  }
  
  TEST_F(TorusTest, ShouldHaveAHoleInTheMiddle) {
    given("a 90 degree rotated torus");
    when("i look at the origin");
    then("i should see the torus with a hole in the middle");
  }
}
