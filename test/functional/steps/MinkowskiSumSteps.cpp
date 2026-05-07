#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeRecognition.h"

#include "render/primitives/MinkowskiSum.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Box.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered minkowski sum with two boxes") {
  auto hull = std::make_shared<MinkowskiSum>();
  hull->add(std::make_shared<Box>(Vector3d(0, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->add(std::make_shared<Box>(Vector3d(0, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->setMaterial(test->redDiffuse());
  
  test->add(hull);
}

GIVEN(EngineFeatureTest, "a displaced minkowski sum with two boxes") {
  auto hull = std::make_shared<MinkowskiSum>();
  hull->add(std::make_shared<Box>(Vector3d(0, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->add(std::make_shared<Box>(Vector3d(0, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->setMaterial(test->redDiffuse());

  auto instance = std::make_shared<Instance>(hull);
  instance->setMatrix(Matrix4d::translate(20, 0, 0));
  test->add(instance);
}
