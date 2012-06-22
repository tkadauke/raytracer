#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "surfaces/Disk.h"

using namespace testing;

GIVEN(RaytracerFeatureTest, "a centered disk") {
  Disk* disk = new Disk(Vector3d::null(), Vector3d(0, 0, -1), 1);
  disk->setMaterial(test->redDiffuse());
  test->add(disk);
}

GIVEN(RaytracerFeatureTest, "a displaced disk") {
  Disk* disk = new Disk(Vector3d(0, 20, 0), Vector3d(0, 0, -1), 1);
  disk->setMaterial(test->redDiffuse());
  test->add(disk);
}

THEN(RaytracerFeatureTest, "i should see the disk") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the disk") {
  ASSERT_FALSE(test->objectVisible());
}