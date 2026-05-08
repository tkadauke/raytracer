#include <gtest/gtest.h>
#include <cmath>
#include "render/primitives/Disk.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/MeshTestHelper.h"

namespace DiskTessellateTest {
  using namespace render;
using namespace render;
using namespace render;

  static constexpr double kEps = 1e-9;

  TEST(DiskTessellate, ShouldReturn17VerticesAtLod0) {
    // lod=0 → 16 segments → 1 centre + 16 rim = 17 vertices
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(0);
    ASSERT_NE(nullptr, mesh);
    ASSERT_EQ(17u, mesh->vertices().size());
  }

  TEST(DiskTessellate, ShouldReturn16TrianglesAtLod0) {
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(0);
    ASSERT_EQ(16u, mesh->faces().size());
  }

  TEST(DiskTessellate, ShouldReturn65VerticesAtLod2) {
    // lod=2 → 16<<2 = 64 segments → 1 + 64 = 65 vertices
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(2);
    ASSERT_EQ(65u, mesh->vertices().size());
  }

  TEST(DiskTessellate, ShouldReturn64TrianglesAtLod2) {
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(2);
    ASSERT_EQ(64u, mesh->faces().size());
  }

  TEST(DiskTessellate, ShouldHaveAllNormalsEqualToDiskNormal) {
    Vector3d normal(0, 1, 0);
    Disk disk(Vector3d(), normal, 1.0);
    auto mesh = disk.tessellate(0);
    for (const auto& v : mesh->vertices())
      EXPECT_EQ(normal, v.normal);
  }

  TEST(DiskTessellate, ShouldWindFacesWithDiskNormal) {
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*disk.tessellate(1));
  }

  TEST(DiskTessellate, ShouldHaveCentreUVAtHalfHalf) {
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(0);
    // Centre vertex is index 0
    EXPECT_NEAR(0.5, mesh->vertices()[0].uv.x(), kEps);
    EXPECT_NEAR(0.5, mesh->vertices()[0].uv.y(), kEps);
  }

  TEST(DiskTessellate, ShouldHaveRimUVsAtHalfRadiusFromCentre) {
    Disk disk(Vector3d(), Vector3d(0, 1, 0), 1.0);
    auto mesh = disk.tessellate(0);
    // Rim vertices (indices 1..16) should have UV distance of 0.5 from (0.5, 0.5)
    for (std::size_t i = 1; i < mesh->vertices().size(); ++i) {
      double dx = mesh->vertices()[i].uv.x() - 0.5;
      double dy = mesh->vertices()[i].uv.y() - 0.5;
      EXPECT_NEAR(0.5, std::sqrt(dx * dx + dy * dy), kEps) << "rim vertex " << i;
    }
  }

  TEST(DiskTessellate, ShouldHaveRimPointsAtRadius) {
    double radius = 2.5;
    Disk disk(Vector3d(), Vector3d(0, 1, 0), radius);
    auto mesh = disk.tessellate(0);
    // Rim vertices should be at distance `radius` from the centre in the XZ plane
    Vector3d centre = mesh->vertices()[0].point;
    for (std::size_t i = 1; i < mesh->vertices().size(); ++i) {
      Vector3d delta = mesh->vertices()[i].point - centre;
      EXPECT_NEAR(radius, delta.length(), 1e-9) << "rim vertex " << i;
    }
  }

  TEST(DiskTessellate, ShouldWorkWithNonYAxisNormal) {
    Disk disk(Vector3d(), Vector3d(0, 0, 1), 1.0);
    auto mesh = disk.tessellate(0);
    ASSERT_EQ(17u, mesh->vertices().size());
    for (const auto& v : mesh->vertices())
      EXPECT_EQ(Vector3d(0, 0, 1), v.normal);
  }
}
