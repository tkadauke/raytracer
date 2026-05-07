#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeClassifier.h"

#include "render/primitives/Torus.h"
#include "render/primitives/Instance.h"

#include "core/math/Matrix.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered torus") {
  auto torus = std::make_shared<Torus>(1, 0.5);
  torus->setMaterial(test->redDiffuse());
  test->add(torus);
}

GIVEN(EngineFeatureTest, "a torus rotated ([\\d.]+) degrees around the ([xyz]) axis") {
  double degrees = std::stod(match[1]);
  std::string axis = match[2];
  auto torus = std::make_shared<Torus>(1, 0.5);
  auto instance = std::make_shared<Instance>(torus);
  Matrix3d m = (axis == "x") ? Matrix3d::rotateX(Angled::fromDegrees(degrees))
             : (axis == "y") ? Matrix3d::rotateY(Angled::fromDegrees(degrees))
                             : Matrix3d::rotateZ(Angled::fromDegrees(degrees));
  instance->setMatrix(m);
  instance->setMaterial(test->redDiffuse());
  test->add(instance);
}

THEN(EngineFeatureTest, "i should see the torus") {
  ShapeClassifier rec(test->primaryColor());
  ASSERT_TRUE(rec.isCircle(test->buffer()));
}

THEN(EngineFeatureTest, "i should see the torus with a hole in the middle") {
  ASSERT_TRUE(test->objectVisible());
  ASSERT_EQ(Colord::white().rgb(), test->colorAt(100, 75));
}

THEN(EngineFeatureTest, "i should not see the torus") {
  ShapeClassifier rec(test->primaryColor());
  ASSERT_FALSE(rec.isCircle(test->buffer()));
}
