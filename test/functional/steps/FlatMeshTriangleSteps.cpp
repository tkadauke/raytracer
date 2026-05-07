#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/primitives/FlatMeshTriangle.h"
#include "core/geometry/Mesh.h"

using namespace testing;
using namespace render;

GIVEN(EngineFeatureTest, "a centered flat mesh triangle") {
  auto mesh = new Mesh;
  mesh->addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1).normalized());
  mesh->addVertex(Vector3d(-1, 1, 0), Vector3d(0, 0, 1).normalized());
  mesh->addVertex(Vector3d(1, -1, 0), Vector3d(0, 0, 1).normalized());
  
  auto triangle = std::make_shared<FlatMeshTriangle>(mesh, 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(EngineFeatureTest, "a displaced flat mesh triangle") {
  auto mesh = new Mesh;
  mesh->addVertex(Vector3d(-1, 20, 0), Vector3d(0, 0, 1).normalized());
  mesh->addVertex(Vector3d(-1, 21, 0), Vector3d(0, 0, 1).normalized());
  mesh->addVertex(Vector3d(1, 20, 0), Vector3d(0, 0, 1).normalized());
  
  auto triangle = std::make_shared<FlatMeshTriangle>(mesh, 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}
