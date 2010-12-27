#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "surfaces/Box.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered box") {
  Box* box = new Box(Vector3d::null, Vector3d(1, 1, 1));
  box->setMaterial(test->redDiffuse());
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a displaced box") {
  Box* box = new Box(Vector3d(0, 20, 0), Vector3d(1, 1, 1));
  box->setMaterial(test->redDiffuse());
  test->add(box);
}

THEN(RaytracerFeatureTest, "i should see the box") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the box") {
  ASSERT_FALSE(test->objectVisible());
}