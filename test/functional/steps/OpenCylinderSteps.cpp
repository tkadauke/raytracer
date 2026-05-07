#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Instance.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered open cylinder") {
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  test->add(cylinder);
}

GIVEN(EngineFeatureTest, "a displaced open cylinder") {
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  auto instance = std::make_shared<Instance>(cylinder);
  instance->setMatrix(Matrix4d::translate(0, 20, 0));
  test->add(instance);
}

GIVEN(EngineFeatureTest, "an open cylinder rotated ([\\d.]+) degrees around the ([xyz]) axis") {
  double degrees = std::stod(match[1]);
  std::string axis = match[2];
  auto cylinder = std::make_shared<OpenCylinder>(1, 2);
  cylinder->setMaterial(test->redDiffuse());
  auto instance = std::make_shared<Instance>(cylinder);
  Matrix3d m = (axis == "x") ? Matrix3d::rotateX(Angled::fromDegrees(degrees))
             : (axis == "y") ? Matrix3d::rotateY(Angled::fromDegrees(degrees))
                             : Matrix3d::rotateZ(Angled::fromDegrees(degrees));
  instance->setMatrix(m);
  test->add(instance);
}

THEN(EngineFeatureTest, "i should see the open cylinder") {
  ShapeRecognition rec(test->primaryColor());
  // This should be a different shape than circle
  ASSERT_TRUE(rec.recognizeCircle(test->buffer()));
}

THEN(EngineFeatureTest, "i should not see the open cylinder") {
  ShapeRecognition rec(test->primaryColor());
  ASSERT_FALSE(rec.recognizeCircle(test->buffer()));
}

THEN(EngineFeatureTest, "i should see a ring") {
  ShapeRecognition rec(test->primaryColor());
  // This should be a different shape than circle, since it is a ring. The
  // ShapeRecognition class only looks at the outlines.
  ASSERT_TRUE(rec.recognizeCircle(test->buffer()));
}
