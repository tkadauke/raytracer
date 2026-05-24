#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/FishEyeCamera.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a fish-eye camera") {
  test->setCamera(std::make_shared<FishEyeCamera>());
}

WHEN(EngineFeatureTest, "i set the fish-eye camera's field of view to ([\\d.]+) degrees") {
  double degrees = std::stod(match[1]);
  static_cast<FishEyeCamera*>(test->camera().get())->setFieldOfView(Angled::fromDegrees(degrees));
}

THEN(EngineFeatureTest, "i should see a black ring around the image") {
  ASSERT_TRUE(test->colorPresent(Colord::black()));
  ASSERT_TRUE(test->colorPresent(Colord::white()));
}
