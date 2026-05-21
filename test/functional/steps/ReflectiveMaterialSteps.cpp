#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/primitives/Box.h"
#include "render/primitives/Sphere.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/textures/ConstantColorTexture.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a perfectly reflective box") {
  auto box = std::make_shared<Box>(Vector3d::null, Vector3d(1, 1, 0.1));
  auto material = std::make_shared<ReflectiveMaterial>();
  box->setMaterial(material);
  test->add(box);
}

GIVEN(EngineFeatureTest, "a reflective box which filters the colors") {
  auto box = std::make_shared<Box>(Vector3d::null, Vector3d(1, 1, 0.1));
  auto material = std::make_shared<ReflectiveMaterial>(
    std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))
  );
  material->setReflectionCoefficient(0);
  material->setAmbientCoefficient(1);
  box->setMaterial(material);
  test->add(box);
}

GIVEN(EngineFeatureTest, "a sphere behind the camera") {
  auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, -4), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(EngineFeatureTest, "i should see the color filtered view through the box") {
  ASSERT_TRUE(test->colorPresent(Colord(1, 0, 0)));
}
