#include "gtest.h"
#include "primitives/SmoothMeshTriangle.h"
#include "test/abstract/AbstractMeshTriangleTest.h"

namespace SmoothMeshTriangleTest {
  using namespace ::testing;
  
  TEST(FlatMeshTriangle, ShouldHaveDifferentNormalsAtDifferentHitPoints) {
    Mesh mesh;
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(-1, -1, 0), Vector3d(0, -2, 1).normalized()));
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(-1, 1, 0), Vector3d(-2, 0, 1).normalized()));
    mesh.vertices.push_back(Mesh::Vertex(Vector3d(1, -1, 0), Vector3d(0, 2, 1).normalized()));
    
    SmoothMeshTriangle triangle(&mesh, 0, 1, 2);
    HitPointInterval hitPoints1;
    Ray ray1(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray1, hitPoints1);
    Vector3d normal1 = hitPoints1.min().normal();

    HitPointInterval hitPoints2;
    Ray ray2(Vector3d(-1, -1, -1), Vector3d(0, 0, 1));
    triangle.intersect(ray2, hitPoints2);
    Vector3d normal2 = hitPoints2.min().normal();
    
    ASSERT_NE(normal1, normal2);
  }

  INSTANTIATE_TYPED_TEST_CASE_P(Smooth, AbstractMeshTriangleTest, SmoothMeshTriangle);
}
