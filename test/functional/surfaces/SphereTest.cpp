#include "gtest/gtest.h"
#include "surfaces/Sphere.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace SphereTest {
  using namespace ::testing;
  
  class SphereTest : public RaytracerFunctionalTest {};
  
  TEST_F(SphereTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredSphere());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(SphereTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    add(displacedSphere());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(SphereTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredSphere());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
}
