#include "gtest/gtest.h"
#include "surfaces/Sphere.h"
#include "cameras/PinholeCamera.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace PinholeCameraTest {
  using namespace ::testing;
  
  struct PinholeCameraTest : public RaytracerFunctionalTest {
    void setDistance(double distance) {
      static_cast<PinholeCamera*>(RaytracerFunctionalTest::camera())->setDistance(distance);
    }
  };
  
  TEST_F(PinholeCameraTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(PinholeCameraTest, ShouldNotBeVisibileOutsideOfView) {
    add(displacedSphere());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(PinholeCameraTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredSphere());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
  
  TEST_F(PinholeCameraTest, ShouldShrinkSizeOfObjectWithLargerDistance) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    int size = objectSize();
    ASSERT_TRUE(size > 0);
    
    goFarAway();

    render();
    ASSERT_TRUE(size > objectSize());
  }
  
  TEST_F(PinholeCameraTest, ShouldShrinkObjectWithSmallerDistance) {
    add(centeredSphere());
    lookAtOrigin();
    setDistance(5);

    render();
    int size = objectSize();
    ASSERT_TRUE(size > 0);
    
    setDistance(1);

    render();
    ASSERT_TRUE(size > objectSize());
  }
}
