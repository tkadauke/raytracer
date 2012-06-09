#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "surfaces/Plane.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered plane") {
  Plane* plane = new Plane(Vector3d(0, 0, 1), 0);
  plane->setMaterial(test->redDiffuse());
  test->add(plane);
}

THEN(RaytracerFeatureTest, "i should see the plane") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the plane") {
  ASSERT_FALSE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "the plane should cover the whole image") {
  ASSERT_FALSE(test->colorPresent(Colord::white()));
}
