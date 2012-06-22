#include "test/functional/support/RaytracerFeatureTest.h"

namespace FishEyeCameraTest {
  using namespace ::testing;
  
  struct FishEyeCameraTest : public RaytracerFeatureTest {};
  
  TEST_F(FishEyeCameraTest, ShouldHaveBlackRingAroundImage) {
    given("a fish-eye camera");
    when("i look at the origin");
    then("i should see a black ring around the image");
  }
  
  TEST_F(FishEyeCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a fish-eye camera");
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }
  
  TEST_F(FishEyeCameraTest, ShouldNotBeVisibileOutsideOfView) {
    given("a fish-eye camera");
    given("a displaced sphere");
    when("i look at the origin");
    then("i should not see the sphere");
  }

  TEST_F(FishEyeCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a fish-eye camera");
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }
  
  TEST_F(FishEyeCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfPlace) {
    given("a fish-eye camera");
    given("a displaced sphere");
    when("i look at the origin");
    when("i set the fish-eye camera's field of view to maximum");
    then("i should see the sphere");
  }
  
  TEST_F(FishEyeCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfDirection) {
    given("a fish-eye camera");
    given("a centered sphere");
    when("i look away from the origin");
    when("i set the fish-eye camera's field of view to maximum");
    then("i should see the sphere");
  }

  TEST_F(FishEyeCameraTest, ShouldNotRenderWhenCanceled) {
    given("a blank canvas");
    given("a fish-eye camera");
    when("the render process is canceled");
    when("i look at the origin");
    then("i should see nothing");
  }
}
