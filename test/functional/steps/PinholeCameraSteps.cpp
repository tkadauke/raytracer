#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "cameras/PinholeCamera.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a pinhole camera") {
  // do nothing, the pinhole camera is the default
}

WHEN(RaytracerFeatureTest, "i set the pinhole camera's view plane distance to a very small value") {
  static_cast<PinholeCamera*>(test->camera())->setDistance(1);
}

WHEN(RaytracerFeatureTest, "i set the pinhole camera's view plane distance to a normal value") {
  static_cast<PinholeCamera*>(test->camera())->setDistance(5);
}
