#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "raytracer/primitives/Box.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a centered box") {
  auto box = std::make_shared<Box>(Vector3d::null(), Vector3d(1, 1, 1));
  box->setMaterial(test->redDiffuse());
  test->add(box);
}

GIVEN(RaytracerFeatureTest, "a displaced box") {
  auto box = std::make_shared<Box>(Vector3d(0, 20, 0), Vector3d(1, 1, 1));
  box->setMaterial(test->redDiffuse());
  test->add(box);
}

THEN(RaytracerFeatureTest, "i should see the box") {
  ShapeRecognition rec;
  ASSERT_TRUE(rec.recognizeRect(test->buffer()));
}

THEN(RaytracerFeatureTest, "i should not see the box") {
  ShapeRecognition rec;
  ASSERT_FALSE(rec.recognizeRect(test->buffer()));
}
