#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "raytracer/primitives/Rectangle.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a centered rectangle") {
  auto rectangle = std::make_shared<Rectangle>(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

GIVEN(RaytracerFeatureTest, "a displaced rectangle") {
  auto rectangle = std::make_shared<Rectangle>(Vector3d(-1, 20, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
  rectangle->setMaterial(test->redDiffuse());
  test->add(rectangle);
}

THEN(RaytracerFeatureTest, "i should see the rectangle") {
  ShapeRecognition rec;
  ASSERT_TRUE(rec.recognizeRect(test->buffer()));
}

THEN(RaytracerFeatureTest, "i should not see the rectangle") {
  ShapeRecognition rec;
  ASSERT_FALSE(rec.recognizeRect(test->buffer()));
}
