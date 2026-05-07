#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/PinholeCamera.h"

using namespace testing;
using namespace engine::raytracer;
using namespace render;

GIVEN(RaytracerFeatureTest, "a pinhole camera") {
  // do nothing, the pinhole camera is the default
  (void)test;
}

WHEN(RaytracerFeatureTest, "i set the pinhole camera's view plane distance to ([\\d.]+)") {
  double distance = std::stod(match[1]);
  static_cast<PinholeCamera*>(test->camera().get())->setDistance(distance);
}
