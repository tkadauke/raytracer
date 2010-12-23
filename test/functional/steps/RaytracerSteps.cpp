#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "an empty scene") {
  // do nothing
}

THEN(RaytracerFeatureTest, "i should see nothing") {
  test->render();
  ASSERT_FALSE(test->objectVisible());
}
