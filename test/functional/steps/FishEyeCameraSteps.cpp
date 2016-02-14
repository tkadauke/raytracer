#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/FishEyeCamera.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a fish-eye camera") {
  test->setCamera(new FishEyeCamera);
}

WHEN(RaytracerFeatureTest, "i set the fish-eye camera's field of view to maximum") {
  static_cast<FishEyeCamera*>(test->camera())->setFieldOfView(Angled::fromDegrees(360));
}

THEN(RaytracerFeatureTest, "i should see a black ring around the image") {
  ASSERT_TRUE(test->colorPresent(Colord::black()));
  ASSERT_TRUE(test->colorPresent(Colord::white()));
}
