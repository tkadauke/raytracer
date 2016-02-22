#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "core/formats/ply/PlyFile.h"
#include "raytracer/primitives/Mesh.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Instance.h"

#include <fstream>

using namespace testing;
using namespace std;
using namespace raytracer;

GIVEN(RaytracerFeatureTest, "a centered cube mesh") {
  ifstream stream("test/fixtures/cube.ply");
  auto mesh = new Mesh;
  PlyFile file(stream, *mesh);
  mesh->computeNormals();
  
  mesh->addFlatTrianglesTo(test->scene(), test->redDiffuse());
}

GIVEN(RaytracerFeatureTest, "a displaced cube mesh") {
  ifstream stream("test/fixtures/cube.ply");
  auto mesh = new Mesh;
  PlyFile file(stream, *mesh);
  mesh->computeNormals();
  auto composite = std::make_shared<Composite>();
  mesh->addFlatTrianglesTo(composite.get(), test->redDiffuse());
  
  auto instance = std::make_shared<Instance>(composite);
  instance->setMatrix(Matrix4d::translate(Vector3d(0, 20, 0)));
  
  test->add(instance);
}

THEN(RaytracerFeatureTest, "i should see the mesh") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the mesh") {
  ASSERT_FALSE(test->objectVisible());
}
