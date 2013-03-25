#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/primitives/Triangle.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered triangle") {
  Triangle* triangle = new Triangle(Vector3d(-1, -1, 0), Vector3d(-1, 1, 0), Vector3d(1, -1, 0));
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(RaytracerFeatureTest, "a displaced triangle") {
  Triangle* triangle = new Triangle(Vector3d(-1, 20, 0), Vector3d(-1, 21, 0), Vector3d(1, 20, 0));
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

THEN(RaytracerFeatureTest, "i should see the triangle") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the triangle") {
  ASSERT_FALSE(test->objectVisible());
}
