#include "gtest/gtest.h"
#include "render/primitives/SmoothMeshTriangle.h"
#include "core/geometry/Mesh.h"

namespace SmoothMeshTriangleTessellateTest {
  using namespace raytracer;
using namespace render;

  // Triangle in XY-plane with distinct per-vertex normals.
  static Mesh makeTriangleMesh() {
    Mesh mesh;
    mesh.addVertex(Vector3d(0, 0, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
    mesh.addVertex(Vector3d(1, 0, 0), Vector3d(0, 1, 0), Vector2d(1, 0));
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d(1, 0, 0), Vector2d(0, 1));
    mesh.addFace({0, 1, 2});
    return mesh;
  }

  TEST(SmoothMeshTriangleTessellate, ShouldHave3VerticesAnd1Face) {
    Mesh mesh = makeTriangleMesh();
    SmoothMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    EXPECT_EQ(3u, result->vertices().size());
    EXPECT_EQ(1u, result->faces().size());
  }

  TEST(SmoothMeshTriangleTessellate, ShouldCopyVertexPositions) {
    Mesh mesh = makeTriangleMesh();
    SmoothMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    ASSERT_EQ(3u, result->vertices().size());
    EXPECT_EQ(Vector3d(0, 0, 0), result->vertices()[0].point);
    EXPECT_EQ(Vector3d(1, 0, 0), result->vertices()[1].point);
    EXPECT_EQ(Vector3d(0, 1, 0), result->vertices()[2].point);
  }

  TEST(SmoothMeshTriangleTessellate, ShouldCopyPerVertexNormalsFromSourceMesh) {
    // Unlike FlatMeshTriangle, each vertex keeps its own mesh normal.
    Mesh mesh = makeTriangleMesh();
    SmoothMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    ASSERT_EQ(3u, result->vertices().size());
    EXPECT_EQ(Vector3d(0, 0, 1), result->vertices()[0].normal);
    EXPECT_EQ(Vector3d(0, 1, 0), result->vertices()[1].normal);
    EXPECT_EQ(Vector3d(1, 0, 0), result->vertices()[2].normal);
  }

  TEST(SmoothMeshTriangleTessellate, ShouldCopyUVsFromSourceMesh) {
    Mesh mesh = makeTriangleMesh();
    SmoothMeshTriangle tri(&mesh, 0, 1, 2);
    auto result = tri.tessellate(0);
    ASSERT_EQ(3u, result->vertices().size());
    EXPECT_EQ(Vector2d(0, 0), result->vertices()[0].uv);
    EXPECT_EQ(Vector2d(1, 0), result->vertices()[1].uv);
    EXPECT_EQ(Vector2d(0, 1), result->vertices()[2].uv);
  }
}
