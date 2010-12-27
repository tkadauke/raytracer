#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "surfaces/Sphere.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered sphere") {
  Sphere* sphere = new Sphere(Vector3d::null, 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

GIVEN(RaytracerFeatureTest, "a displaced sphere") {
  Sphere* sphere = new Sphere(Vector3d(0, 20, 0), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(RaytracerFeatureTest, "i should see the sphere") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should see the sphere with size S") {
  ASSERT_TRUE(test->objectVisible());
  if (test->previousObjectSize) {
    ASSERT_EQ(test->previousObjectSize, test->objectSize());
  } else {
    test->previousObjectSize = test->objectSize();
  }
}

THEN(RaytracerFeatureTest, "i should see the sphere with size smaller than S") {
  ASSERT_TRUE(test->objectVisible());
  ASSERT_TRUE(test->previousObjectSize > test->objectSize());
}

THEN(RaytracerFeatureTest, "i should not see the sphere") {
  ASSERT_FALSE(test->objectVisible());
}
