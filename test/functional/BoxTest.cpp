#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Box.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace BoxTest {
  using namespace ::testing;
  
  class BoxTest : public RaytracerFunctionalTest {};
  
  TEST_F(BoxTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(new Box(Vector3d::null, Vector3d(1, 1, 1)));
    setCamera(Vector3d(0, 0, -5), Vector3d::null);

    render();
    ASSERT_TRUE(colorPresent(Colord::black));
  }
  
  TEST_F(BoxTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    add(new Box(Vector3d(0, 20, 0), Vector3d(1, 1, 1)));
    setCamera(Vector3d(0, 0, -5), Vector3d::null);
    
    render();
    ASSERT_FALSE(colorPresent(Colord::black));
  }

  TEST_F(BoxTest, ShouldNotBeVisibileBehindTheCamera) {
    add(new Box(Vector3d::null, Vector3d(1, 1, 1)));
    setCamera(Vector3d(0, 0, -20), Vector3d(0, 0, -25));

    render();
    ASSERT_FALSE(colorPresent(Colord::black));
  }
}
