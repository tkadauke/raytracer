#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "cameras/OrthographicCamera.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "an orthographic camera") {
  test->setCamera(new OrthographicCamera);
}
