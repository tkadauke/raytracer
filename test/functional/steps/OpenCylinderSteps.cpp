#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "raytracer/primitives/OpenCylinder.h"
#include "raytracer/primitives/Instance.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a centered open cylinder") {
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  test->add(cylinder);
}

GIVEN(RaytracerFeatureTest, "a displaced open cylinder") {
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  auto instance = std::make_shared<Instance>(cylinder);
  instance->setMatrix(Matrix4d::translate(0, 20, 0));
  test->add(instance);
}

GIVEN(RaytracerFeatureTest, "an open cylinder rotated 90 degrees around the x axis") {
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  auto instance = std::make_shared<Instance>(cylinder);
  instance->setMatrix(Matrix3d::rotateX(90_degrees));
  test->add(instance);
}

THEN(RaytracerFeatureTest, "i should see the open cylinder") {
  ShapeRecognition rec;
  // This should be a different shape than circle
  ASSERT_TRUE(rec.recognizeCircle(test->buffer()));
}

THEN(RaytracerFeatureTest, "i should not see the open cylinder") {
  ShapeRecognition rec;
  ASSERT_FALSE(rec.recognizeCircle(test->buffer()));
}

THEN(RaytracerFeatureTest, "i should see a ring") {
  ShapeRecognition rec;
  // This should be a different shape than circle, since it is a ring. The
  // ShapeRecognition class only looks at the outlines.
  ASSERT_TRUE(rec.recognizeCircle(test->buffer()));
}
