#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/OrthographicCamera.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "an orthographic camera") {
  test->setCamera(new OrthographicCamera);
}
