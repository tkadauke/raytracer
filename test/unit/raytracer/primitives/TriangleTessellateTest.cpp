#include "gtest/gtest.h"
#include "raytracer/primitives/Triangle.h"
#include "core/geometry/Mesh.h"

namespace TriangleTessellateTest {
  using namespace raytracer;
using namespace render;

  TEST(TriangleTessellate, ShouldHave3VerticesAnd1Face) {
    Triangle tri(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = tri.tessellate(0);
    EXPECT_EQ(3u, mesh->vertices().size());
    EXPECT_EQ(1u, mesh->faces().size());
  }

  TEST(TriangleTessellate, ShouldIgnoreLod) {
    Triangle tri(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto m0 = tri.tessellate(0);
    auto m5 = tri.tessellate(5);
    EXPECT_EQ(m0->vertices().size(), m5->vertices().size());
    EXPECT_EQ(m0->faces().size(), m5->faces().size());
  }

  TEST(TriangleTessellate, ShouldHaveCorrectVertexPositions) {
    Vector3d a(0, 0, 0), b(1, 0, 0), c(0, 1, 0);
    Triangle tri(a, b, c);
    auto mesh = tri.tessellate(0);
    ASSERT_EQ(3u, mesh->vertices().size());
    EXPECT_EQ(a, mesh->vertices()[0].point);
    EXPECT_EQ(b, mesh->vertices()[1].point);
    EXPECT_EQ(c, mesh->vertices()[2].point);
  }

  TEST(TriangleTessellate, ShouldHaveFlatNormalOnAllVertices) {
    // (1,0,0)-(0,0,0) = (1,0,0); (0,1,0)-(0,0,0) = (0,1,0)
    // normal = (1,0,0) ^ (0,1,0) = (0,0,1)
    Triangle tri(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = tri.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_EQ(Vector3d(0, 0, 1), v.normal);
    }
  }

  TEST(TriangleTessellate, ShouldHaveBarycentricUVs) {
    Triangle tri(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = tri.tessellate(0);
    ASSERT_EQ(3u, mesh->vertices().size());
    EXPECT_EQ(Vector2d(0, 0), mesh->vertices()[0].uv);
    EXPECT_EQ(Vector2d(1, 0), mesh->vertices()[1].uv);
    EXPECT_EQ(Vector2d(0, 1), mesh->vertices()[2].uv);
  }
}
