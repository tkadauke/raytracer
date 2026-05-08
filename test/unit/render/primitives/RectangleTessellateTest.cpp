#include "gtest/gtest.h"
#include "render/primitives/Rectangle.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/MeshTestHelper.h"

namespace RectangleTessellateTest {
  using namespace render;
using namespace render;
using namespace render;

  TEST(RectangleTessellate, ShouldHave4VerticesAnd2Faces) {
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = rect.tessellate(0);
    EXPECT_EQ(4u, mesh->vertices().size());
    EXPECT_EQ(2u, mesh->faces().size());
  }

  TEST(RectangleTessellate, ShouldIgnoreLod) {
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto m0 = rect.tessellate(0);
    auto m5 = rect.tessellate(5);
    EXPECT_EQ(m0->vertices().size(), m5->vertices().size());
    EXPECT_EQ(m0->faces().size(), m5->faces().size());
  }

  TEST(RectangleTessellate, ShouldHaveCorrectNormal) {
    // leg1=(1,0,0) ^ leg2=(0,1,0) = (0,0,1)
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = rect.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_EQ(Vector3d(0, 0, 1), v.normal);
    }
  }

  TEST(RectangleTessellate, ShouldWindFacesWithPlaneNormal) {
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*rect.tessellate(0));
  }

  TEST(RectangleTessellate, ShouldHaveVerticesAtFourCorners) {
    Vector3d corner(1, 2, 3);
    Vector3d leg1(1, 0, 0);
    Vector3d leg2(0, 1, 0);
    Rectangle rect(corner, leg1, leg2);
    auto mesh = rect.tessellate(0);
    ASSERT_EQ(4u, mesh->vertices().size());
    EXPECT_EQ(corner,               mesh->vertices()[0].point);
    EXPECT_EQ(corner + leg1,        mesh->vertices()[1].point);
    EXPECT_EQ(corner + leg1 + leg2, mesh->vertices()[2].point);
    EXPECT_EQ(corner + leg2,        mesh->vertices()[3].point);
  }

  TEST(RectangleTessellate, ShouldHaveUVsAtCorners) {
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = rect.tessellate(0);
    ASSERT_EQ(4u, mesh->vertices().size());
    EXPECT_EQ(Vector2d(0, 0), mesh->vertices()[0].uv);
    EXPECT_EQ(Vector2d(1, 0), mesh->vertices()[1].uv);
    EXPECT_EQ(Vector2d(1, 1), mesh->vertices()[2].uv);
    EXPECT_EQ(Vector2d(0, 1), mesh->vertices()[3].uv);
  }

  TEST(RectangleTessellate, ShouldCoverRectangleWithTwoTriangles) {
    Rectangle rect(Vector3d(), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto mesh = rect.tessellate(0);
    // 2 faces each with 3 indices covering all 4 vertices
    ASSERT_EQ(2u, mesh->faces().size());
    for (const auto& face : mesh->faces()) {
      EXPECT_EQ(3u, face.size());
    }
  }
}
