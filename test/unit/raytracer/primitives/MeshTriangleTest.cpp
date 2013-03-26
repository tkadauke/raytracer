#include "gtest.h"
#include "raytracer/primitives/MeshTriangle.h"
#include "raytracer/primitives/Mesh.h"

namespace MeshTriangleTest {
  using namespace ::testing;
  using namespace raytracer;
  
  class ConcreteMeshTriangle : public MeshTriangle {
  public:
    ConcreteMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2) {}
    Primitive* intersect(const Ray&, HitPointInterval&) { return 0; }
  };
  
  TEST(MeshTriangle, ShouldComputeBoundingBox) {
    Mesh mesh;
    
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(0, 0, 0), Vector3d::null()));
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(0, 1, 0), Vector3d::null()));
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(1, 0, 0), Vector3d::null()));
    ConcreteMeshTriangle triangle(&mesh, 0, 1, 2);
    
    BoundingBox expected(Vector3d(0, 0, 0), Vector3d(1, 1, 0));
    ASSERT_EQ(expected, triangle.boundingBox());
  }
}
