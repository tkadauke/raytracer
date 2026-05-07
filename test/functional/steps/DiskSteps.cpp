#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "render/primitives/Disk.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered disk") {
  auto disk = std::make_shared<Disk>(Vector3d::null(), Vector3d(0, 0, -1), 1);
  disk->setMaterial(test->redDiffuse());
  test->add(disk);
}

GIVEN(EngineFeatureTest, "a displaced disk") {
  auto disk = std::make_shared<Disk>(Vector3d(0, 20, 0), Vector3d(0, 0, -1), 1);
  disk->setMaterial(test->redDiffuse());
  test->add(disk);
}

THEN(EngineFeatureTest, "i should see the disk") {
  ShapeRecognition rec;
  ASSERT_TRUE(rec.recognizeCircle(test->buffer()));
}

THEN(EngineFeatureTest, "i should not see the disk") {
  ShapeRecognition rec;
  ASSERT_FALSE(rec.recognizeCircle(test->buffer()));
}
