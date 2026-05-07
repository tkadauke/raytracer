#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

using namespace testing;

GIVEN(EngineFeatureTest, "a blank canvas") {
  test->clear();
}

GIVEN(EngineFeatureTest, "an empty scene") {
  // do nothing
  (void)test;
}

WHEN(EngineFeatureTest, "the render process is canceled") {
  test->cancel();
}

THEN(EngineFeatureTest, "i should see something") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(EngineFeatureTest, "i should see nothing") {
  ASSERT_FALSE(test->objectVisible());
}

THEN(EngineFeatureTest, "show me") {
  test->show();
}
