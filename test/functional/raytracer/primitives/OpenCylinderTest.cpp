#include "test/functional/support/RaytracerFeatureTest.h"

namespace OpenCylinderTest {
  using namespace ::testing;
  
  class OpenCylinderTest : public RaytracerFeatureTest {};
  
  TEST_F(OpenCylinderTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered open cylinder");
    when("i look at the origin");
    then("i should see the open cylinder");
  }
  
  TEST_F(OpenCylinderTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced open cylinder");
    when("i look at the origin");
    then("i should not see the open cylinder");
  }

  TEST_F(OpenCylinderTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered open cylinder");
    when("i look away from the origin");
    then("i should not see the open cylinder");
  }

  TEST_F(OpenCylinderTest, ShouldLookLikeARingFromAbove) {
    given("an open cylinder rotated 90 degrees around the x axis");
    when("i look at the origin");
    then("i should see a ring");
  }
}
