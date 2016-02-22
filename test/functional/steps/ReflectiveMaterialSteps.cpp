#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a perfectly reflective box") {
  auto box = std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 0.1));
  ReflectiveMaterial* material = new ReflectiveMaterial;
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a reflective box which filters the colors") {
  auto box = std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 0.1));
  ReflectiveMaterial* material = new ReflectiveMaterial(
    std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))
  );
  material->setReflectionCoefficient(0);
  material->setAmbientCoefficient(1);
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a sphere behind the camera") {
  auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, -4), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(RaytracerFeatureTest, "i should see the color filtered view through the box") {
  ASSERT_TRUE(test->colorPresent(Colord(1, 0, 0)));
}
