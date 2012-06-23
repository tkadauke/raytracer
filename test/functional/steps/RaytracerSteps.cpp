#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a blank canvas") {
  test->clear();
}

GIVEN(RaytracerFeatureTest, "an empty scene") {
  // do nothing
}

WHEN(RaytracerFeatureTest, "the render process is canceled") {
  test->cancel();
}

THEN(RaytracerFeatureTest, "i should see something") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should see nothing") {
  ASSERT_FALSE(test->objectVisible());
}
