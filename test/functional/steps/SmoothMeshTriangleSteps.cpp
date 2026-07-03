#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/MeshTestHelper.h"

#include "render/primitives/SmoothMeshTriangle.h"

using namespace testing;
using namespace render;
using test::helpers::createCenteredTriangleMesh;
using test::helpers::createDisplacedTriangleMesh;

GIVEN(EngineFeatureTest, "a centered smooth mesh triangle") {
  auto triangle = std::make_shared<SmoothMeshTriangle>(createCenteredTriangleMesh(), 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(EngineFeatureTest, "a displaced smooth mesh triangle") {
  auto triangle = std::make_shared<SmoothMeshTriangle>(createDisplacedTriangleMesh(), 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}
