#ifndef ABSTRACT_VIEW_PLANE_TEST_H
#define ABSTRACT_VIEW_PLANE_TEST_H

#include "test/functional/support/RaytracerFeatureTest.h"
#include "raytracer/cameras/Camera.h"
#include "raytracer/viewplanes/ViewPlane.h"

namespace testing {
  template<class VP>
  struct AbstractViewPlaneTest : public RaytracerFeatureTest {
    AbstractViewPlaneTest() : RaytracerFeatureTest() {
      camera()->setViewPlane(std::make_shared<VP>());
    }
  };

  TYPED_TEST_CASE_P(AbstractViewPlaneTest);

  TYPED_TEST_P(AbstractViewPlaneTest, ShouldRenderEmptyScene) {
    this->given("an empty scene");
    this->when("i look at the origin");
    this->then("i should see nothing");
  }

  TYPED_TEST_P(AbstractViewPlaneTest, ShouldRenderSphere) {
    this->given("a centered sphere");
    this->when("i look at the origin");
    this->then("i should see the sphere");
  }

  REGISTER_TYPED_TEST_CASE_P(AbstractViewPlaneTest,
                             ShouldRenderEmptyScene, ShouldRenderSphere);
}

#endif
