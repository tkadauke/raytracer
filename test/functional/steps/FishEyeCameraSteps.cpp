#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "cameras/FishEyeCamera.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a fish-eye camera") {
  test->setCamera(new FishEyeCamera);
}

WHEN(RaytracerFeatureTest, "i set the fish-eye camera's field of view to maximum") {
  static_cast<FishEyeCamera*>(test->camera())->setFieldOfView(360);
}

THEN(RaytracerFeatureTest, "i should see a black ring around the image") {
  test->render();
  ASSERT_TRUE(test->colorPresent(Colord::black));
  ASSERT_TRUE(test->colorPresent(Colord::white));
}
