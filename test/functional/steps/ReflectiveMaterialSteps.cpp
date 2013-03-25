#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "primitives/Box.h"
#include "primitives/Sphere.h"
#include "materials/ReflectiveMaterial.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a perfectly reflective box") {
  Box* box = new Box(Vector3d::null(), Vector3d(1, 1, 0.1));
  ReflectiveMaterial* material = new ReflectiveMaterial;
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a reflective box which filters the colors") {
  Box* box = new Box(Vector3d::null(), Vector3d(1, 1, 0.1));
  ReflectiveMaterial* material = new ReflectiveMaterial(Colord(1, 0, 0));
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a sphere behind the camera") {
  Sphere* sphere = new Sphere(Vector3d(0, 0, -4), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(RaytracerFeatureTest, "i should see the color filtered view through the box") {
  ASSERT_TRUE(test->colorPresent(Colord(1, 0, 0)));
}
