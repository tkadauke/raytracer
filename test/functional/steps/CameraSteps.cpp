#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

using namespace testing;

WHEN(RaytracerFeatureTest, "i look at the origin") {
  test->lookAtOrigin();
}

WHEN(RaytracerFeatureTest, "i look away from the origin") {
  test->lookAway();
}

WHEN(RaytracerFeatureTest, "i go far away from the origin") {
  test->goFarAway();
}
