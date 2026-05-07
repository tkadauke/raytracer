#include "test/functional/support/RaytracerFeatureTest.h"

namespace SphericalCameraTest {
  using namespace ::testing;
  
  struct SphericalCameraTest : public RaytracerFeatureTest {};
  
  TEST_F(SphericalCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a spherical camera");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }
  
  TEST_F(SphericalCameraTest, ShouldNotBeVisibileOutsideOfView) {
    given("a spherical camera");
    given("a displaced sphere");
    when("i look at the origin");
    then("i should not see the sphere");
  }

  TEST_F(SphericalCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a spherical camera");
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }
  
  TEST_F(SphericalCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfPlace) {
    given("a spherical camera");
    given("a displaced sphere");
    when("i look at the origin");
    when("i set the spherical camera's field of view to maximum");
    then("i should see the sphere");
  }
  
  TEST_F(SphericalCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfDirection) {
    given("a spherical camera");
    given("a centered sphere");
    when("i look away from the origin");
    when("i set the spherical camera's field of view to maximum");
    then("i should see the sphere");
  }

  TEST_F(SphericalCameraTest, ShouldNotRenderWhenCanceled) {
    given("a blank canvas");
    given("a spherical camera");
    when("the render process is canceled");
    when("i look at the origin");
    then("i should see nothing");
  }
}
