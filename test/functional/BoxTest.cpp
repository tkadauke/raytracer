#include "gtest/gtest.h"
#include "Box.h"
#include "test/helpers/RayTracerTestHelper.h"

namespace BoxTest {
  using namespace ::testing;
  
  struct BoxTest : public RaytracerFunctionalTest {
    Box* centeredBox() {
      Box* box = new Box(Vector3d::null, Vector3d(1, 1, 1));
      box->setMaterial(redDiffuse());
      return box;
    }

    Box* displacedBox() {
      Box* box = new Box(Vector3d(0, 20, 0), Vector3d(1, 1, 1));
      box->setMaterial(redDiffuse());
      return box;
    }
  };
  
  TEST_F(BoxTest, ShouldBeVisibileInFrontOfTheCamera) {
    add(centeredBox());
    lookAtOrigin();

    render();
    ASSERT_TRUE(objectVisible());
  }
  
  TEST_F(BoxTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    add(displacedBox());
    lookAtOrigin();
    
    render();
    ASSERT_FALSE(objectVisible());
  }

  TEST_F(BoxTest, ShouldNotBeVisibileBehindTheCamera) {
    add(centeredBox());
    lookAway();

    render();
    ASSERT_FALSE(objectVisible());
  }
}
