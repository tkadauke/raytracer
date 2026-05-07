#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/ShapeClassifier.h"

#include "render/primitives/ConvexHull.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Box.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered convex hull with two boxes side by side") {
  auto hull = std::make_shared<ConvexHull>();
  hull->add(std::make_shared<Box>(Vector3d(-1, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->add(std::make_shared<Box>(Vector3d( 1, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->setMaterial(test->redDiffuse());
  
  test->add(hull);
}

GIVEN(EngineFeatureTest, "a displaced convex hull with two boxes side by side") {
  auto hull = std::make_shared<ConvexHull>();
  hull->add(std::make_shared<Box>(Vector3d(-1, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->add(std::make_shared<Box>(Vector3d( 1, 0, 0), Vector3d(0.25, 0.25, 0.25)));
  hull->setMaterial(test->redDiffuse());

  auto instance = std::make_shared<Instance>(hull);
  instance->setMatrix(Matrix4d::translate(20, 0, 0));
  test->add(instance);
}
