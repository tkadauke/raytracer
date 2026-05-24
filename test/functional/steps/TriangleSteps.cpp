#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/primitives/Triangle.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered triangle") {
  auto triangle =
    std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(-1, 1, 0), Vector3d(1, -1, 0));
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(EngineFeatureTest, "a displaced triangle") {
  auto triangle =
    std::make_shared<Triangle>(Vector3d(-1, 20, 0), Vector3d(-1, 21, 0), Vector3d(1, 20, 0));
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

THEN(EngineFeatureTest, "i should see the triangle") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(EngineFeatureTest, "i should not see the triangle") {
  ASSERT_FALSE(test->objectVisible());
}
