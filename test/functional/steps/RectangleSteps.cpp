#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "surfaces/Rectangle.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered rectangle") {
  Rectangle* rectangle = new Rectangle(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

GIVEN(RaytracerFeatureTest, "a displaced rectangle") {
  Rectangle* rectangle = new Rectangle(Vector3d(-1, 20, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

THEN(RaytracerFeatureTest, "i should see the rectangle") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the rectangle") {
  ASSERT_FALSE(test->objectVisible());
}
