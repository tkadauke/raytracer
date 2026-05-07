#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeClassifier.h"

#include "render/primitives/Sphere.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered sphere") {
  auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

GIVEN(EngineFeatureTest, "a displaced sphere") {
  auto sphere = std::make_shared<Sphere>(Vector3d(0, 20, 0), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

THEN(EngineFeatureTest, "i should see the sphere") {
  ShapeClassifier rec(test->primaryColor());
  ASSERT_TRUE(rec.isCircle(test->buffer()));
}

THEN(EngineFeatureTest, "i should see the sphere with size S") {
  ASSERT_TRUE(test->objectVisible());
  if (test->previousObjectSize) {
    ASSERT_EQ(test->previousObjectSize, test->objectSize());
  } else {
    test->previousObjectSize = test->objectSize();
  }
}

THEN(EngineFeatureTest, "i should see the sphere with size smaller than S") {
  ASSERT_TRUE(test->objectVisible());
  ASSERT_TRUE(test->previousObjectSize > test->objectSize());
}

THEN(EngineFeatureTest, "i should see the sphere with size larger than S") {
  ASSERT_TRUE(test->objectVisible());
  ASSERT_TRUE(test->previousObjectSize < test->objectSize());
}

THEN(EngineFeatureTest, "i should not see the sphere") {
  ShapeClassifier rec(test->primaryColor());
  ASSERT_FALSE(rec.isCircle(test->buffer()));
}
