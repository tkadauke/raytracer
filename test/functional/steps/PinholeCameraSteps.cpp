#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/PinholeCamera.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a pinhole camera") {
  // do nothing, the pinhole camera is the default
  (void)test;
}

WHEN(RaytracerFeatureTest, "i set the pinhole camera's view plane distance to a very small value") {
  static_cast<PinholeCamera*>(test->camera())->setDistance(1);
}

WHEN(RaytracerFeatureTest, "i set the pinhole camera's view plane distance to a normal value") {
  static_cast<PinholeCamera*>(test->camera())->setDistance(5);
}

WHEN(RaytracerFeatureTest, "i zoom in") {
  auto camera = static_cast<PinholeCamera*>(test->camera());
  camera->setZoom(camera->zoom() * 2);
}

WHEN(RaytracerFeatureTest, "i zoom out") {
  auto camera = static_cast<PinholeCamera*>(test->camera());
  camera->setZoom(camera->zoom() * 0.8);
}
