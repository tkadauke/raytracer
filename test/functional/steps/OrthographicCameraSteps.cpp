#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/OrthographicCamera.h"

using namespace testing;
using namespace engine::raytracer;
using namespace render;

GIVEN(RaytracerFeatureTest, "an orthographic camera") {
  test->setCamera(std::make_shared<OrthographicCamera>());
}
