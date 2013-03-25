#include "gtest.h"
#include "primitives/Mesh.h"
#include "test/helpers/ContainerTestHelper.h"

#include "materials/MatteMaterial.h"
#include "primitives/Scene.h"

namespace MeshTest {
  using namespace ::testing;
  
  struct MeshTest : public ::testing::Test {
    void SetUp() {
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(0, 0, 0), Vector3d::null()));
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(0, 1, 0), Vector3d::null()));
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(1, 0, 0), Vector3d::null()));

      mesh.faces.push_back(makeStdVector(0, 1, 2));
    }
    Mesh mesh;
  };
  
  TEST(Mesh, ShouldInstantiate) {
    Mesh mesh;
  }
  
  TEST_F(MeshTest, ShouldComputeNormals) {
    this->mesh.computeNormals();
    
    ASSERT_EQ(Vector3d(0, 0, 1), mesh.vertices[0].normal);
  }
  
  TEST_F(MeshTest, ShouldAddSmoothTrianglesToComposite) {
    MatteMaterial material;
    Scene scene;
    this->mesh.addSmoothTrianglesTo(&scene, &material);
    ASSERT_EQ(1u, scene.primitives().size());
  }
  
  TEST_F(MeshTest, ShouldAddFlatTrianglesToComposite) {
    this->mesh.computeNormals();
    
    MatteMaterial material;
    Scene scene;
    this->mesh.addFlatTrianglesTo(&scene, &material);
    ASSERT_EQ(1u, scene.primitives().size());
  }
}
