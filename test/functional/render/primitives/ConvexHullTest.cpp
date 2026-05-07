#include "test/functional/support/RaytracerFeatureTest.h"

namespace ConvexHullTest {
  using namespace ::testing;
  
  class ConvexHullTest : public RaytracerFeatureTest {};
  
  TEST_F(ConvexHullTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered convex hull with two boxes side by side");
    when("i look at the origin");
    then("i should see the box");
  }
  
  TEST_F(ConvexHullTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced convex hull with two boxes side by side");
    when("i look at the origin");
    then("i should not see the box");
  }

  TEST_F(ConvexHullTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered convex hull with two boxes side by side");
    when("i look away from the origin");
    then("i should not see the box");
  }
}
