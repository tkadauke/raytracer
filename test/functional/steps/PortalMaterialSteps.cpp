#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/PortalMaterial.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a box portal") {
  Box* box = new Box(Vector3d::null(), Vector3d(1, 1, 0.1));
  PortalMaterial* material = new PortalMaterial(Matrix3d(), Colord::white());
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a box portal which turns the rays towards the displaced sphere") {
  Box* box = new Box(Vector3d::null(), Vector3d(1, 1, 0.1));
  PortalMaterial* material = new PortalMaterial(Matrix3d::rotateX(0.79), Colord::white());
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a box portal which filters the colors") {
  Box* box = new Box(Vector3d::null(), Vector3d(1, 1, 0.1));
  PortalMaterial* material = new PortalMaterial(Matrix3d(), Colord(1, 0, 0));
  box->setMaterial(material);
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a sphere behind the box") {
  Sphere* sphere = new Sphere(Vector3d(0, 0, 4), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(RaytracerFeatureTest, "i should see the color filtered view through the portal") {
  ASSERT_TRUE(test->colorPresent(Colord(1, 0, 0)));
}
