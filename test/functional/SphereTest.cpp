#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Sphere.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace SphereTest {
  using namespace ::testing;
  
  class SphereTest : public RaytracerFunctionalTest {};
  
  TEST_F(SphereTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(new Sphere(Vector3d::null, 1));
    setCamera(Vector3d(0, 0, -5), Vector3d::null);

    render();
    ASSERT_TRUE(colorPresent(Colord::black));
  }
  
  TEST_F(SphereTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    add(new Sphere(Vector3d(0, 20, 0), 1));
    setCamera(Vector3d(0, 0, -5), Vector3d::null);
    
    render();
    ASSERT_FALSE(colorPresent(Colord::black));
  }

  TEST_F(SphereTest, ShouldNotBeVisibileBehindTheCamera) {
    add(new Sphere(Vector3d::null, 1));
    setCamera(Vector3d(0, 0, -20), Vector3d(0, 0, -25));

    render();
    ASSERT_FALSE(colorPresent(Colord::black));
  }
}
