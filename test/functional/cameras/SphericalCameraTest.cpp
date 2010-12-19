#include "gtest/gtest.h"
#include "surfaces/Sphere.h"
#include "cameras/SphericalCamera.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace SphericalCameraTest {
  using namespace ::testing;
  
  struct SphericalCameraTest : public RaytracerFunctionalTest {
    SphericalCameraTest() : RaytracerFunctionalTest() {
      setCamera(new SphericalCamera);
    }
    
    void maximumFieldOfView() {
      static_cast<SphericalCamera*>(RaytracerFunctionalTest::camera())->setFieldOfView(360, 180);
    }
  };
  
  TEST_F(SphericalCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(SphericalCameraTest, ShouldNotBeVisibileOutsideOfView) {
    add(displacedSphere());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(SphericalCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredSphere());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
  
  TEST_F(SphericalCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfPlace) {
    add(displacedSphere());
    lookAtOrigin();
    maximumFieldOfView();
    
    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(SphericalCameraTest, ShouldSeeAllObjectsWithMaximumFieldOfViewRegardlessOfDirection) {
    add(centeredSphere());
    lookAway();
    maximumFieldOfView();
    
    render();
    ASSERT_TRUE(objectVisible());
  }
}
