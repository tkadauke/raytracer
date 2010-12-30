#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

using namespace testing;

THEN(RaytracerFeatureTest, "i should see the triangle") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the triangle") {
  ASSERT_FALSE(test->objectVisible());
}
