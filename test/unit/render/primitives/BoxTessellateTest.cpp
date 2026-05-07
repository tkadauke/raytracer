#include <gtest/gtest.h>

#include "render/primitives/Box.h"
#include "core/geometry/Mesh.h"

namespace BoxTessellateTest {
  using namespace raytracer;
using namespace render;

  TEST(BoxTessellate, ProducesNonEmptyMesh) {
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    ASSERT_NE(nullptr, mesh);
    EXPECT_GT(mesh->vertices().size(), 0u);
    EXPECT_GT(mesh->faces().size(), 0u);
  }

  TEST(BoxTessellate, HasSixFacesAndTwentyFourVertices) {
    // Box mesh is 6 quads × 4 vertices each = 24 vertices, 6 faces.
    // Vertices are NOT shared across faces because per-face normals
    // and per-face UVs would force a split anyway. Pin the count so
    // a future "share corners" optimisation can't silently break the
    // flat-shading / per-face-UV invariants.
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    EXPECT_EQ(24u, mesh->vertices().size());
    EXPECT_EQ(6u, mesh->faces().size());
  }

  TEST(BoxTessellate, IgnoresLodArgument) {
    // Box is polyhedral — every LOD value produces the same mesh.
    // (Sphere/Torus tessellate scale up with LOD; Box does not. Pin
    // so a future "make Box LOD-able with subdivisions" change is
    // loud.)
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto m0 = box.tessellate(0);
    auto m5 = box.tessellate(5);
    EXPECT_EQ(m0->vertices().size(), m5->vertices().size());
    EXPECT_EQ(m0->faces().size(), m5->faces().size());
  }

  TEST(BoxTessellate, AllVerticesLieOnTheBoxSurface) {
    // For a unit cube centred at the origin, every vertex must have
    // at least one component at ±1 (the surface) — interior points
    // would mean the tessellation is wrong.
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    for (const auto& v : mesh->vertices()) {
      const double tol = 1e-9;
      bool onSurface =
        std::abs(std::abs(v.point.x()) - 1.0) < tol ||
        std::abs(std::abs(v.point.y()) - 1.0) < tol ||
        std::abs(std::abs(v.point.z()) - 1.0) < tol;
      EXPECT_TRUE(onSurface) << "vertex (" << v.point.x() << ", "
                             << v.point.y() << ", " << v.point.z()
                             << ") is not on the box surface";
    }
  }

  TEST(BoxTessellate, NormalsAreUnitLengthAndAxisAligned) {
    // Box face normals should be exactly ±X, ±Y, or ±Z. Pin so a
    // future change can't silently introduce skewed normals that
    // would break flat shading.
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    for (const auto& v : mesh->vertices()) {
      EXPECT_NEAR(1.0, v.normal.length(), 1e-9);
      // Exactly one component is ±1, the other two are 0.
      int nonzero =
        (std::abs(v.normal.x()) > 0.5 ? 1 : 0) +
        (std::abs(v.normal.y()) > 0.5 ? 1 : 0) +
        (std::abs(v.normal.z()) > 0.5 ? 1 : 0);
      EXPECT_EQ(1, nonzero);
    }
  }

  TEST(BoxTessellate, EachFaceHasAllSixOutwardNormals) {
    // Across the 6 quad faces, the set of outward normals must
    // cover ±X, ±Y, ±Z exactly once each.
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    int count[6] = {0, 0, 0, 0, 0, 0};  // +X, -X, +Y, -Y, +Z, -Z
    for (const auto& face : mesh->faces()) {
      // All 4 vertices in a face share a normal. Sample the first.
      const Vector3d& n = mesh->vertices()[face[0]].normal;
      if      (n.x() >  0.5) count[0]++;
      else if (n.x() < -0.5) count[1]++;
      else if (n.y() >  0.5) count[2]++;
      else if (n.y() < -0.5) count[3]++;
      else if (n.z() >  0.5) count[4]++;
      else if (n.z() < -0.5) count[5]++;
    }
    for (int i = 0; i < 6; i++) {
      EXPECT_EQ(1, count[i]) << "expected exactly one face with outward "
                                "axis " << i;
    }
  }

  TEST(BoxTessellate, UVsSpanUnitSquarePerFace) {
    // Every face's four vertices must produce the four corners of
    // [0, 1]² in UV space — not necessarily in any particular order,
    // but the SET of UVs must be {(0,0), (1,0), (1,1), (0,1)}.
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    for (const auto& face : mesh->faces()) {
      bool corners[4] = {false, false, false, false};
      for (int idx : face) {
        const Vector2d& uv = mesh->vertices()[idx].uv;
        if      (uv.x() == 0 && uv.y() == 0) corners[0] = true;
        else if (uv.x() == 1 && uv.y() == 0) corners[1] = true;
        else if (uv.x() == 1 && uv.y() == 1) corners[2] = true;
        else if (uv.x() == 0 && uv.y() == 1) corners[3] = true;
        else FAIL() << "unexpected UV (" << uv.x() << ", " << uv.y() << ")";
      }
      for (bool c : corners) EXPECT_TRUE(c);
    }
  }

  TEST(BoxTessellate, AllFacesAreQuads) {
    // Sanity — pin that we use 4-vertex faces (so the
    // TriangleIterator's fan triangulation produces exactly 2
    // triangles per face = 12 triangles total).
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    for (const auto& face : mesh->faces()) {
      EXPECT_EQ(4u, face.size());
    }
  }

  TEST(BoxTessellate, TriangleIteratorYieldsTwelveTriangles) {
    Box box(Vector3d::null(), Vector3d(1, 1, 1));
    auto mesh = box.tessellate();
    int count = 0;
    for (auto it = mesh->begin(); it != mesh->end(); ++it) ++count;
    EXPECT_EQ(12, count);
  }

  TEST(BoxTessellate, RespectsCenterAndEdgeArguments) {
    // Box at non-zero center with non-uniform half-extents. The
    // bounding box of the resulting mesh's vertex points must match
    // [center - edge, center + edge].
    Box box(Vector3d(5, 6, 7), Vector3d(1, 2, 3));
    auto mesh = box.tessellate();
    Vector3d minV(1e9, 1e9, 1e9), maxV(-1e9, -1e9, -1e9);
    for (const auto& v : mesh->vertices()) {
      minV = Vector3d(std::min(minV.x(), v.point.x()),
                      std::min(minV.y(), v.point.y()),
                      std::min(minV.z(), v.point.z()));
      maxV = Vector3d(std::max(maxV.x(), v.point.x()),
                      std::max(maxV.y(), v.point.y()),
                      std::max(maxV.z(), v.point.z()));
    }
    EXPECT_EQ(Vector3d(4, 4, 4), minV);
    EXPECT_EQ(Vector3d(6, 8, 10), maxV);
  }
}
