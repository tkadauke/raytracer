#include "test/functional/support/RaytracerFeatureTest.h"

namespace PinholeCameraTest {
  using namespace ::testing;
  
  struct PinholeCameraTest : public RaytracerFeatureTest {};
  
  TEST_F(PinholeCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a pinhole camera");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }
  
  TEST_F(PinholeCameraTest, ShouldNotBeVisibileOutsideOfView) {
    given("a pinhole camera");
    given("a displaced sphere");
    when("i look at the origin");
    then("i should not see the sphere");
  }

  TEST_F(PinholeCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a pinhole camera");
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }
  
  TEST_F(PinholeCameraTest, ShouldShrinkSizeOfObjectWithLargerDistance) {
    given("a pinhole camera");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere with size S");
    when("i go far away from the origin");
    then("i should see the sphere with size smaller than S");
  }
  
  TEST_F(PinholeCameraTest, ShouldShrinkObjectWithSmallerViewPlaneDistance) {
    given("a pinhole camera");
    given("a centered sphere");
    when("i look at the origin");
    when("i set the pinhole camera's view plane distance to a normal value");
    then("i should see the sphere with size S");
    when("i set the pinhole camera's view plane distance to a very small value");
    then("i should see the sphere with size smaller than S");
  }
}
