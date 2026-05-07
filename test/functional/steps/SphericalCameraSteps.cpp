#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/SphericalCamera.h"

using namespace testing;
using namespace engine::raytracer;
using namespace render;

GIVEN(RaytracerFeatureTest, "a spherical camera") {
  test->setCamera(std::make_shared<SphericalCamera>());
}

WHEN(RaytracerFeatureTest, "i set the spherical camera's field of view to ([\\d.]+) by ([\\d.]+) degrees") {
  double horizontal = std::stod(match[1]);
  double vertical = std::stod(match[2]);
  static_cast<SphericalCamera*>(test->camera().get())
    ->setFieldOfView(Angled::fromDegrees(horizontal), Angled::fromDegrees(vertical));
}
