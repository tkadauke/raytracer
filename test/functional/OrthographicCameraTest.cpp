#include "gtest/gtest.h"
#include "Sphere.h"
#include "OrthographicCamera.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace OrthographicCameraTest {
  using namespace ::testing;
  
  struct OrthographicCameraTest : public RaytracerFunctionalTest {
    OrthographicCameraTest() : RaytracerFunctionalTest() {
      setCamera(new OrthographicCamera);
    }
  };
  
  TEST_F(OrthographicCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(OrthographicCameraTest, ShouldNotBeVisibileOutsideOfView) {
    add(displacedSphere());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(OrthographicCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredSphere());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
  
  TEST_F(OrthographicCameraTest, ShouldNotShrinkSizeOfObjectWithLargerDistance) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    int size = objectSize();
    ASSERT_TRUE(size > 0);
    
    goFarAway();

    render();
    ASSERT_EQ(size, objectSize());
  }
}
