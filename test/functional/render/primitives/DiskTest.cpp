#include "test/functional/support/RaytracerFeatureTest.h"

namespace DiskTest {
  using namespace ::testing;
  
  class DiskTest : public RaytracerFeatureTest {};
  
  TEST_F(DiskTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered disk");
    when("i look at the origin");
    then("i should see the disk");
  }
  
  TEST_F(DiskTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced disk");
    when("i look at the origin");
    then("i should not see the disk");
  }

  TEST_F(DiskTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered disk");
    when("i look away from the origin");
    then("i should not see the disk");
  }
}
