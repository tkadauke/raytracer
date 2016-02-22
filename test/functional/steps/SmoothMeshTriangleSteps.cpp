#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/primitives/SmoothMeshTriangle.h"
#include "raytracer/primitives/Mesh.h"

using namespace testing;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a centered smooth mesh triangle") {
  auto mesh = new Mesh;
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1).normalized()));
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(-1, 1, 0), Vector3d(0, 0, 1).normalized()));
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(1, -1, 0), Vector3d(0, 0, 1).normalized()));
  
  auto triangle = std::make_shared<SmoothMeshTriangle>(mesh, 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}

GIVEN(RaytracerFeatureTest, "a displaced smooth mesh triangle") {
  auto mesh = new Mesh;
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(-1, 20, 0), Vector3d(0, 0, 1).normalized()));
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(-1, 21, 0), Vector3d(0, 0, 1).normalized()));
  mesh->vertices.push_back(Mesh::Vertex(Vector3d(1, 20, 0), Vector3d(0, 0, 1).normalized()));
  
  auto triangle = std::make_shared<SmoothMeshTriangle>(mesh, 0, 1, 2);
  triangle->setMaterial(test->redDiffuse());
  test->add(triangle);
}
