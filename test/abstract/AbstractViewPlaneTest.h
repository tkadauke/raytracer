#ifndef ABSTRACT_VIEW_PLANE_TEST_H
#define ABSTRACT_VIEW_PLANE_TEST_H

#include "gtest.h"
#include "Sphere.h"
#include "test/helpers/RayTracerTestHelper.h"
#include "Camera.h"
#include "ViewPlane.h"

namespace testing {
  template<class VP>
  struct AbstractViewPlaneTest : public RaytracerFunctionalTest {
    AbstractViewPlaneTest() : RaytracerFunctionalTest() {
      camera()->setViewPlane(new VP);
    }
  };

  TYPED_TEST_CASE_P(AbstractViewPlaneTest);

  TYPED_TEST_P(AbstractViewPlaneTest, ShouldEmptyScene) {
    this->lookAtOrigin();

    this->render();
    ASSERT_FALSE(this->objectVisible());
  }

  TYPED_TEST_P(AbstractViewPlaneTest, ShouldRenderSphere) {
    this->add(this->centeredSphere());
    this->lookAtOrigin();

    this->render();
    ASSERT_TRUE(this->objectVisible());
  }

  REGISTER_TYPED_TEST_CASE_P(AbstractViewPlaneTest,
                             ShouldEmptyScene, ShouldRenderSphere);
}

#endif
