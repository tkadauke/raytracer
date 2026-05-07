#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "render/primitives/Rectangle.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered rectangle") {
  auto rectangle = std::make_shared<Rectangle>(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

GIVEN(EngineFeatureTest, "a displaced rectangle") {
  auto rectangle = std::make_shared<Rectangle>(Vector3d(-1, 20, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

THEN(EngineFeatureTest, "i should see the rectangle") {
  ShapeRecognition rec(test->primaryColor());
  ASSERT_TRUE(rec.recognizeRect(test->buffer()));
}

THEN(EngineFeatureTest, "i should not see the rectangle") {
  ShapeRecognition rec(test->primaryColor());
  ASSERT_FALSE(rec.recognizeRect(test->buffer()));
}
