#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/SphericalCamera.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a spherical camera") {
  test->setCamera(new SphericalCamera);
}

WHEN(RaytracerFeatureTest, "i set the spherical camera's field of view to maximum") {
  static_cast<SphericalCamera*>(test->camera())->setFieldOfView(360, 180);
}
