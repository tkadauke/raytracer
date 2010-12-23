#include "test/functional/support/RaytracerFeatureTest.h"

namespace PlaneTest {
  using namespace ::testing;

  struct PlaneTest : public RaytracerFeatureTest {};

  TEST_F(PlaneTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered plane");
    when("i look at the origin");
    then("i should see the plane");
  }

  TEST_F(PlaneTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered plane");
    when("i look away from the origin");
    then("i should not see the plane");
  }
  
  TEST_F(PlaneTest, ShouldBeOfInfiniteSize) {
    given("a centered plane");
    when("i look at the origin");
    then("the plane should cover the whole image");
  }
}
