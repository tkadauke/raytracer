#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/SphericalCamera.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a spherical camera") {
  test->setCamera(std::make_shared<SphericalCamera>());
}

WHEN(RaytracerFeatureTest, "i set the spherical camera's field of view to maximum") {
  static_cast<SphericalCamera*>(test->camera().get())
    ->setFieldOfView(360_degrees, 180_degrees);
}
