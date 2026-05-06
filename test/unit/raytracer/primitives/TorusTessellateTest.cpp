#include "gtest/gtest.h"
#include "raytracer/primitives/Torus.h"
#include "core/geometry/Mesh.h"
#include <cmath>

namespace TorusTessellateTest {
  using namespace raytracer;

  const double kEps = 1e-10;

  // lod=0: (16+1) × (16+1) = 17 × 17 = 289 vertices
  TEST(TorusTessellate, VertexCountAtLod0) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    ASSERT_EQ(289u, mesh->vertices().size());
  }

  // lod=0: 16 × 16 = 256 quad faces
  TEST(TorusTessellate, FaceCountAtLod0) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    ASSERT_EQ(256u, mesh->faces().size());
  }

  // lod=1: (32+1) × (32+1) = 33 × 33 = 1089 vertices
  TEST(TorusTessellate, VertexCountAtLod1) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(1);
    ASSERT_EQ(1089u, mesh->vertices().size());
  }

  // lod=1: 32 × 32 = 1024 quad faces
  TEST(TorusTessellate, FaceCountAtLod1) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(1);
    ASSERT_EQ(1024u, mesh->faces().size());
  }

  // Lod 1 must produce more vertices than lod 0 (resolution increases)
  TEST(TorusTessellate, Lod1HasMoreVerticesThanLod0) {
    Torus torus(2.0, 0.5);
    EXPECT_GT(torus.tessellate(1)->vertices().size(),
              torus.tessellate(0)->vertices().size());
  }

  // Every vertex must lie between (R-r) and (R+r) from the y-axis (XZ-plane
  // distance), since the torus ring is centred on the y-axis.
  TEST(TorusTessellate, AllVerticesWithinRadialBounds) {
    const double R = 2.0, r = 0.5;
    Torus torus(R, r);
    auto mesh = torus.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      double distFromAxis = std::sqrt(v.point.x() * v.point.x() +
                                     v.point.z() * v.point.z());
      EXPECT_GE(distFromAxis, R - r - kEps);
      EXPECT_LE(distFromAxis, R + r + kEps);
    }
  }

  TEST(TorusTessellate, NormalsAreUnitLength) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_NEAR(1.0, v.normal.length(), kEps);
    }
  }

  // For each vertex: the vector from the nearest major-circle point to the
  // surface point must have magnitude r and direction equal to the normal.
  TEST(TorusTessellate, NormalsPointFromTubeCenter) {
    const double R = 2.0, r = 0.5;
    Torus torus(R, r);
    auto mesh = torus.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      // Project vertex onto XZ plane to find the closest point on the major circle
      double px = v.point.x(), pz = v.point.z();
      double axisLen = std::sqrt(px * px + pz * pz);
      Vector3d tubeCenter(px / axisLen * R, 0.0, pz / axisLen * R);
      Vector3d toSurface = v.point - tubeCenter;
      EXPECT_NEAR(r, toSurface.length(), kEps);
      // Direction from tube center to surface must match the stored normal
      Vector3d dir = toSurface / r;
      EXPECT_NEAR(1.0, dir * v.normal, kEps);
    }
  }

  // u ∈ [0,1] around the major circle, v ∈ [0,1] around the tube
  TEST(TorusTessellate, UVCoversFullRange) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    double minU = 1.0, maxU = 0.0, minV = 1.0, maxV = 0.0;
    for (const auto& v : mesh->vertices()) {
      minU = std::min(minU, v.uv.x());
      maxU = std::max(maxU, v.uv.x());
      minV = std::min(minV, v.uv.y());
      maxV = std::max(maxV, v.uv.y());
    }
    EXPECT_NEAR(0.0, minU, kEps);
    EXPECT_NEAR(1.0, maxU, kEps);
    EXPECT_NEAR(0.0, minV, kEps);
    EXPECT_NEAR(1.0, maxV, kEps);
  }

  // All faces must be quads (4 vertex indices)
  TEST(TorusTessellate, AllFacesAreQuads) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    for (const auto& face : mesh->faces()) {
      EXPECT_EQ(4u, face.size());
    }
  }

  // Face indices must be within vertex array bounds
  TEST(TorusTessellate, FaceIndicesInBounds) {
    Torus torus(2.0, 0.5);
    auto mesh = torus.tessellate(0);
    int n = static_cast<int>(mesh->vertices().size());
    for (const auto& face : mesh->faces()) {
      for (int idx : face) {
        EXPECT_GE(idx, 0);
        EXPECT_LT(idx, n);
      }
    }
  }
}
