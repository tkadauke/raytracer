#include "gtest/gtest.h"
#include "raytracer/primitives/FlatMeshTriangle.h"
#include "core/geometry/Mesh.h"

namespace FlatMeshTriangleTessellateTest {
  using namespace raytracer;

  // Mesh vertices have normals pointing in +Y; FlatMeshTriangle averages them
  // to get its face normal, which is also +Y.
  static Mesh makeTriangleMesh() {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d(0, 1, 0), Vector2d(0, 0));
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d(0, 1, 0), Vector2d(1, 0));
    mesh.addVertex(Vector3d(0, 0, 1), Vector3d(0, 1, 0), Vector2d(0, 1));
    mesh.addFace({0, 1, 2});
    return mesh;
  }

  TEST(FlatMeshTriangleTessellate, ShouldHave3VerticesAnd1Face) {
    Mesh mesh = makeTriangleMesh();
    FlatMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    EXPECT_EQ(3u, result->vertices().size());
    EXPECT_EQ(1u, result->faces().size());
  }

  TEST(FlatMeshTriangleTessellate, ShouldCopyVertexPositions) {
    Mesh mesh = makeTriangleMesh();
    FlatMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    ASSERT_EQ(3u, result->vertices().size());
    EXPECT_EQ(Vector3d(0, 0, 0), result->vertices()[0].point);
    EXPECT_EQ(Vector3d(1, 0, 0), result->vertices()[1].point);
    EXPECT_EQ(Vector3d(0, 0, 1), result->vertices()[2].point);
  }

  TEST(FlatMeshTriangleTessellate, ShouldUseFaceNormalForAllVertices) {
    // FlatMeshTriangle computes a single face normal (average of vertex normals).
    Mesh mesh = makeTriangleMesh();
    FlatMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    Vector3d n0 = result->vertices()[0].normal;
    EXPECT_EQ(n0, result->vertices()[1].normal);
    EXPECT_EQ(n0, result->vertices()[2].normal);
    EXPECT_DOUBLE_EQ(1.0, n0.squaredLength());
  }

  TEST(FlatMeshTriangleTessellate, ShouldCopyUVsFromSourceMesh) {
    Mesh mesh = makeTriangleMesh();
    FlatMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    ASSERT_EQ(3u, result->vertices().size());
    EXPECT_EQ(Vector2d(0, 0), result->vertices()[0].uv);
    EXPECT_EQ(Vector2d(1, 0), result->vertices()[1].uv);
    EXPECT_EQ(Vector2d(0, 1), result->vertices()[2].uv);
  }
}
