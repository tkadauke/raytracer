#include "gtest/gtest.h"
#include "Sphere.h"
#include "FishEyeCamera.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace FishEyeCameraTest {
  using namespace ::testing;
  
  struct FishEyeCameraTest : public RaytracerFunctionalTest {
    FishEyeCameraTest() : RaytracerFunctionalTest() {
      setCamera(new FishEyeCamera);
    }
    
    void maximumFieldOfView() {
      static_cast<FishEyeCamera*>(RaytracerFunctionalTest::camera())->setFieldOfView(360);
    }
  };
  
  TEST_F(FishEyeCameraTest, ShouldHaveBlackRingAroundImage) {
    lookAtOrigin();
    render();
    ASSERT_TRUE(colorPresent(Colord::black));
    ASSERT_TRUE(colorPresent(Colord::white));
  }
  
  TEST_F(FishEyeCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(FishEyeCameraTest, ShouldNotBeVisibileOutsideOfView) {
    add(displacedSphere());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(FishEyeCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredSphere());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
  
  TEST_F(FishEyeCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfPlace) {
    add(displacedSphere());
    lookAtOrigin();
    maximumFieldOfView();
    
    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(FishEyeCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfDirection) {
    add(centeredSphere());
    lookAway();
    maximumFieldOfView();
    
    render();
    ASSERT_TRUE(objectVisible());
  }
}
