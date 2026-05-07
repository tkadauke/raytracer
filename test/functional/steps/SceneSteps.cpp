#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "render/primitives/Scene.h"

using namespace testing;

GIVEN(EngineFeatureTest, "an empty scene with blue background") {
  test->scene()->setBackground(Colord(0.4, 0.8, 1));
}

THEN(EngineFeatureTest, "i should see only blue") {
  ASSERT_TRUE(test->colorPresent(Colord(0.4, 0.8, 1)));
}
