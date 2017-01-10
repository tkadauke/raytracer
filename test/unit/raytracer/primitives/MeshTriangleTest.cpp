#include "gtest.h"
#include "raytracer/primitives/MeshTriangle.h"
#include "core/geometry/Mesh.h"

namespace MeshTriangleTest {
  using namespace ::testing;
  using namespace raytracer;
  
  class ConcreteMeshTriangle : public MeshTriangle {
  public:
    inline ConcreteMeshTriangle(Mesh* mesh, int index0, int index1, int index2)
      : MeshTriangle(mesh, index0, index1, index2)
    {
    }
    
    inline virtual const Primitive* intersect(const Rayd&, HitPointInterval&, State&) const {
      return nullptr;
    }
  };
  
  TEST(MeshTriangle, ShouldComputeBoundingBox) {
    Mesh mesh;
    
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d::null());
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d::null());
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d::null());
    ConcreteMeshTriangle triangle(&mesh, 0, 1, 2);
    
    BoundingBoxd expected = BoundingBoxd(Vector3d(0, 0, 0), Vector3d(1, 1, 0)).grownByEpsilon();
    ASSERT_EQ(expected, triangle.boundingBox());
  }
}
