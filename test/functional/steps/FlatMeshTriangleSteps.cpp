#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"
#include "test/helpers/MeshTestHelper.h"

#include "render/primitives/FlatMeshTriangle.h"

using namespace testing;
using namespace render;
using test::helpers::createCenteredTriangleMesh;
using test::helpers::createDisplacedTriangleMesh;

GIVEN(EngineFeatureTest, "a centered flat mesh triangle") {
  auto triangle = std::make_shared<FlatMeshTriangle>(createCenteredTriangleMesh(), 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(EngineFeatureTest, "a displaced flat mesh triangle") {
  auto triangle = std::make_shared<FlatMeshTriangle>(createDisplacedTriangleMesh(), 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}
