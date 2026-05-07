#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/primitives/Plane.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered plane") {
  auto plane = std::make_shared<Plane>(Vector3d(0, 0, 1), 0);
  plane->setMaterial(test->redDiffuse());
  test->add(plane);
}

THEN(EngineFeatureTest, "i should see the plane") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(EngineFeatureTest, "i should not see the plane") {
  ASSERT_FALSE(test->objectVisible());
}

THEN(EngineFeatureTest, "the plane should cover the whole image") {
  ASSERT_FALSE(test->colorPresent(Colord::white()));
}
