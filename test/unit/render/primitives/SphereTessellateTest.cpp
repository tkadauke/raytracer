#include "gtest/gtest.h"
#include "render/primitives/Sphere.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/MeshTestHelper.h"
#include <cmath>

namespace SphereTessellateTest {
  using namespace render;
  using namespace render;
  using namespace render;

  const double kEps = 1e-10;

  // lod=0: (8+1) lat rows × (16+1) lon cols = 9 × 17 = 153 vertices
  TEST(SphereTessellate, VertexCountAtLod0) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
    ASSERT_EQ(153u, mesh->vertices().size());
  }

  // lod=0: 8 lat bands × 16 lon segments = 128 quad faces
  TEST(SphereTessellate, FaceCountAtLod0) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
    ASSERT_EQ(128u, mesh->faces().size());
  }

  // lod=1: (16+1) × (32+1) = 17 × 33 = 561 vertices
  TEST(SphereTessellate, VertexCountAtLod1) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(1);
    ASSERT_EQ(561u, mesh->vertices().size());
  }

  // lod=1: 16 × 32 = 512 quad faces
  TEST(SphereTessellate, FaceCountAtLod1) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(1);
    ASSERT_EQ(512u, mesh->faces().size());
  }

  // Lod 1 must produce more vertices than lod 0 (resolution increases)
  TEST(SphereTessellate, Lod1HasMoreVerticesThanLod0) {
    Sphere sphere(Vector3d(), 1.0);
    EXPECT_GT(sphere.tessellate(1)->vertices().size(), sphere.tessellate(0)->vertices().size());
  }

  TEST(SphereTessellate, AllVerticesAtRadius) {
    const double radius = 2.5;
    const Vector3d origin(1.0, 2.0, 3.0);
    Sphere sphere(origin, radius);
    auto mesh = sphere.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_NEAR(radius, (v.point - origin).length(), kEps);
    }
  }

  TEST(SphereTessellate, NormalsAreUnitLength) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      EXPECT_NEAR(1.0, v.normal.length(), kEps);
    }
  }

  // Normal must equal the unit radial direction from origin to vertex.
  TEST(SphereTessellate, NormalsAreRadial) {
    const double radius = 3.0;
    const Vector3d origin(0.0, 0.0, 0.0);
    Sphere sphere(origin, radius);
    auto mesh = sphere.tessellate(0);
    for (const auto& v : mesh->vertices()) {
      Vector3d radial = (v.point - origin) / radius;
      EXPECT_NEAR(1.0, v.normal * radial, kEps);
    }
  }

  TEST(SphereTessellate, FacesAreWoundWithRadialNormals) {
    Sphere sphere(Vector3d(), 1.0);
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*sphere.tessellate(1));
  }

  // u ∈ [0,1] across the longitude range, v ∈ [0,1] south→north
  TEST(SphereTessellate, UVCoversFullRange) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
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
  TEST(SphereTessellate, AllFacesAreQuads) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
    for (const auto& face : mesh->faces()) {
      EXPECT_EQ(4u, face.size());
    }
  }

  // Face indices must be within vertex array bounds
  TEST(SphereTessellate, FaceIndicesInBounds) {
    Sphere sphere(Vector3d(), 1.0);
    auto mesh = sphere.tessellate(0);
    int n = static_cast<int>(mesh->vertices().size());
    for (const auto& face : mesh->faces()) {
      for (int idx : face) {
        EXPECT_GE(idx, 0);
        EXPECT_LT(idx, n);
      }
    }
  }
}
