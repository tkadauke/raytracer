#include <gtest/gtest.h>
#include <cmath>
#include "render/primitives/OpenCylinder.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/MeshTestHelper.h"

namespace OpenCylinderTessellateTest {
  using namespace render;
  using namespace render;
  using namespace render;

  static constexpr double kEps = 1e-9;

  TEST(OpenCylinderTessellate, ShouldReturn34VerticesAtLod0) {
    // lod=0 → 16 segments → 2*(16+1) = 34 vertices
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(0);
    ASSERT_NE(nullptr, mesh);
    ASSERT_EQ(34u, mesh->vertices().size());
  }

  TEST(OpenCylinderTessellate, ShouldReturn16QuadFacesAtLod0) {
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(0);
    ASSERT_EQ(16u, mesh->faces().size());
  }

  TEST(OpenCylinderTessellate, ShouldReturn130VerticesAtLod2) {
    // lod=2 → 16<<2 = 64 segments → 2*(64+1) = 130 vertices
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(2);
    ASSERT_EQ(130u, mesh->vertices().size());
  }

  TEST(OpenCylinderTessellate, ShouldReturn64QuadFacesAtLod2) {
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(2);
    ASSERT_EQ(64u, mesh->faces().size());
  }

  TEST(OpenCylinderTessellate, ShouldHaveRadiallyOutwardNormals) {
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      // Normal Y component must be zero (radially outward from Y axis)
      EXPECT_NEAR(0.0, v.normal.y(), kEps) << "normal.y should be 0";
      // Normal must be unit-length
      EXPECT_NEAR(1.0, v.normal.length(), kEps);
    }
  }

  TEST(OpenCylinderTessellate, ShouldWindFacesWithRadialNormals) {
    OpenCylinder cyl(1.0, 2.0);
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*cyl.tessellate(1));
  }

  TEST(OpenCylinderTessellate, ShouldHaveRimVerticesAtRadius) {
    double radius = 3.0;
    OpenCylinder cyl(radius, 4.0);
    auto mesh = cyl.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      double distFromAxis = std::sqrt(v.point.x() * v.point.x() + v.point.z() * v.point.z());
      EXPECT_NEAR(radius, distFromAxis, kEps);
    }
  }

  TEST(OpenCylinderTessellate, ShouldHaveBottomVerticesAtMinusHalfHeight) {
    OpenCylinder cyl(1.0, 4.0); // halfHeight = 2.0
    auto mesh = cyl.tessellate(0);
    const auto& verts = mesh->vertices();
    // Interleaved layout: even indices = bottom (v=0), odd = top (v=1)
    for (std::size_t i = 0; i < verts.size(); i += 2) {
      EXPECT_NEAR(-2.0, verts[i].point.y(), kEps) << "bottom vertex " << i;
      EXPECT_NEAR(0.0, verts[i].uv.y(), kEps) << "bottom UV.v vertex " << i;
    }
  }

  TEST(OpenCylinderTessellate, ShouldHaveTopVerticesAtPlusHalfHeight) {
    OpenCylinder cyl(1.0, 4.0); // halfHeight = 2.0
    auto mesh = cyl.tessellate(0);
    const auto& verts = mesh->vertices();
    for (std::size_t i = 1; i < verts.size(); i += 2) {
      EXPECT_NEAR(2.0, verts[i].point.y(), kEps) << "top vertex " << i;
      EXPECT_NEAR(1.0, verts[i].uv.y(), kEps) << "top UV.v vertex " << i;
    }
  }

  TEST(OpenCylinderTessellate, ShouldHaveUVuInRange0To1) {
    OpenCylinder cyl(1.0, 2.0);
    auto mesh = cyl.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_GE(v.uv.x(), 0.0 - kEps);
      EXPECT_LE(v.uv.x(), 1.0 + kEps);
    }
  }
}
