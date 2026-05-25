#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "render/primitives/Curve.h"
#include "test/helpers/MeshTestHelper.h"

namespace CurveTessellateTest {
  using namespace render;

  TEST(CurveTessellate, EmptyCurveHasUndefinedBoundingBox) {
    Curve curve(core::Polyline(), 0.5);

    EXPECT_TRUE(curve.boundingBox().isUndefined());
  }

  TEST(CurveTessellate, BoundingBoxIncludesHalfWidthAroundPolylinePoints) {
    Curve curve(core::Polyline({Vector3d(-1.0, 2.0, 3.0), Vector3d(4.0, -2.0, 5.0)}), 0.5);

    const BoundingBoxd expected =
      BoundingBoxd(Vector3d(-1.0, -2.0, 3.0), Vector3d(4.0, 2.0, 5.0))
        .grownBy(Vector3d(0.25, 0.25, 0.25))
        .grownByEpsilon();
    EXPECT_EQ(expected, curve.boundingBox());
  }

  TEST(CurveTessellate, RibbonModeProducesOneQuadPerNonZeroSegment) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                                Vector3d(1.0, 1.0, 0.0)}),
                0.2, Curve::TessellationMode::Ribbon);

    auto mesh = curve.tessellate();

    ASSERT_NE(nullptr, mesh);
    EXPECT_EQ(8u, mesh->vertices().size());
    EXPECT_EQ(2u, mesh->faces().size());
    for (const auto& face : mesh->faces())
      EXPECT_EQ(4u, face.size());
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*mesh);
  }

  TEST(CurveTessellate, TubeModeProducesRingQuadsForNonZeroSegments) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.4,
                Curve::TessellationMode::Tube);

    auto mesh = curve.tessellate(0);

    ASSERT_NE(nullptr, mesh);
    EXPECT_EQ(16u, mesh->vertices().size());
    EXPECT_EQ(8u, mesh->faces().size());
    for (const auto& vertex : mesh->vertices())
      EXPECT_NEAR(1.0, vertex.normal.length(), 1e-9);
    EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(*mesh);
  }

  TEST(CurveTessellate, TubeLodDoublesRingResolution) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.4,
                Curve::TessellationMode::Tube);

    auto low = curve.tessellate(0);
    auto high = curve.tessellate(1);

    EXPECT_EQ(8u, low->faces().size());
    EXPECT_EQ(16u, high->faces().size());
  }

  TEST(CurveTessellate, ZeroLengthSegmentsAreSkippedSafely) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 0.0, 0.0),
                                Vector3d(1.0, 0.0, 0.0)}),
                0.2, Curve::TessellationMode::Ribbon);

    auto mesh = curve.tessellate();

    ASSERT_NE(nullptr, mesh);
    EXPECT_EQ(4u, mesh->vertices().size());
    EXPECT_EQ(1u, mesh->faces().size());
  }

  TEST(CurveTessellate, ZeroLengthOnlyCurveReturnsEmptyMesh) {
    Curve curve(core::Polyline({Vector3d(1.0, 1.0, 1.0), Vector3d(1.0, 1.0, 1.0)}), 0.2,
                Curve::TessellationMode::Tube);

    auto mesh = curve.tessellate();

    ASSERT_NE(nullptr, mesh);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }

  TEST(CurveTessellate, NonPositiveWidthReturnsEmptyMesh) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.0,
                Curve::TessellationMode::Ribbon);

    auto mesh = curve.tessellate();

    ASSERT_NE(nullptr, mesh);
    EXPECT_TRUE(mesh->vertices().empty());
    EXPECT_TRUE(mesh->faces().empty());
  }
}
