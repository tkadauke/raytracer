#include "gtest.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/ContainerTestHelper.h"

#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/primitives/Scene.h"

namespace MeshTest {
  using namespace ::testing;
  using namespace raytracer;
  
  struct MeshTest : public ::testing::Test {
    inline void SetUp() {
      mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null());
      mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null());
      mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null());

      mesh.addFace(makeStdVector(0, 1, 2));
    }
    Mesh mesh;
  };
  
  TEST(Mesh, ShouldInstantiate) {
    Mesh mesh;
  }
  
  TEST_F(MeshTest, ShouldComputeNormals) {
    this->mesh.computeNormals();
    
    ASSERT_EQ(Vector3d(0, 0, 1), mesh.vertices().front().normal);
  }
  
  TEST_F(MeshTest, ShouldAddVertex) {
    auto before = this->mesh.vertices().size();
    this->mesh.addVertex(Vector3d(1, 1, 1), Vector3d::null());
    auto after = this->mesh.vertices().size();
    ASSERT_EQ(1u, after - before);
  }
  
  TEST_F(MeshTest, ShouldAddFace) {
    auto before = this->mesh.faces().size();
    this->mesh.addFace(makeStdVector(2, 1, 0));
    auto after = this->mesh.faces().size();
    ASSERT_EQ(1u, after - before);
  }
}
