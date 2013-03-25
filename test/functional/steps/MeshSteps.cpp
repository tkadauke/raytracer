#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "formats/ply/PlyFile.h"
#include "primitives/Mesh.h"
#include "primitives/Scene.h"
#include "primitives/Instance.h"

#include <fstream>

using namespace testing;
using namespace std;

GIVEN(RaytracerFeatureTest, "a centered cube mesh") {
  ifstream stream("test/fixtures/cube.ply");
  Mesh* mesh = new Mesh;
  PlyFile file(stream, *mesh);
  mesh->computeNormals();
  
  mesh->addFlatTrianglesTo(test->scene(), test->redDiffuse());
}

GIVEN(RaytracerFeatureTest, "a displaced cube mesh") {
  ifstream stream("test/fixtures/cube.ply");
  Mesh* mesh = new Mesh;
  PlyFile file(stream, *mesh);
  mesh->computeNormals();
  Composite* composite = new Composite;
  mesh->addFlatTrianglesTo(composite, test->redDiffuse());
  
  Instance* instance = new Instance(composite);
  instance->setMatrix(Matrix4d::translate(Vector3d(0, 20, 0)));
  
  test->add(instance);
}

THEN(RaytracerFeatureTest, "i should see the mesh") {
  ASSERT_TRUE(test->objectVisible());
}

THEN(RaytracerFeatureTest, "i should not see the mesh") {
  ASSERT_FALSE(test->objectVisible());
}
