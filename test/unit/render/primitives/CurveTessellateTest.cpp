#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "core/math/RayPacket.h"
#include "render/State.h"
#include "render/primitives/Curve.h"
#include "test/helpers/MeshTestHelper.h"
#include "test/helpers/PrimitiveTestHelper.h"

#include <string>

namespace CurveTessellateTest {
  using namespace render;
  using test::helpers::PacketStates4;
  using test::helpers::PacketStates8;

  TEST(CurveTessellate, EmptyCurveHasUndefinedBoundingBox) {
    Curve curve(core::Polyline(), 0.5);

    EXPECT_TRUE(curve.boundingBox().isUndefined());
  }

  TEST(CurveTessellate, BoundingBoxIncludesHalfWidthAroundPolylinePoints) {
    Curve curve(core::Polyline({Vector3d(-1.0, 2.0, 3.0), Vector3d(4.0, -2.0, 5.0)}), 0.5);

    const BoundingBoxd expected = BoundingBoxd(Vector3d(-1.0, -2.0, 3.0), Vector3d(4.0, 2.0, 5.0))
                                    .grownBy(Vector3d(0.25, 0.25, 0.25))
                                    .grownByEpsilon();
    EXPECT_EQ(expected, curve.boundingBox());
  }

  TEST(CurveTessellate, Ray4PacketHitsReportMissesWithoutScalarFallback) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.2);
    const Ray4 rays(std::array<Rayd, Ray4::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 1, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, -1)), Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1))});
    PacketStates4 ps;

    const auto result = curve.intersectPacketHits(rays, ps.states);

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      EXPECT_FALSE(result.hit(lane));
      EXPECT_EQ(1, ps.lanes[lane].intersectionMisses);
      EXPECT_EQ(0u, ps.lanes[lane].packetHitScalarFallbacks);
    }
  }

  TEST(CurveTessellate, Ray8PacketHitsReportMissesWithoutScalarFallback) {
    Curve curve(core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.2);
    const Ray8 rays(std::array<Rayd, Ray8::lanes>{
      Rayd(Vector3d(0, 0, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(0, 1, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, 2), Vector3d(0, 0, -1)), Rayd(Vector3d(2, 0, -2), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 0, -3), Vector3d(0, 0, 1)), Rayd(Vector3d(1, 0, -3), Vector3d(0, 0, 1)),
      Rayd(Vector3d(0, 2, -2), Vector3d(0, 0, 1)), Rayd(Vector3d(1, 1, -2), Vector3d(0, 0, 1))});
    PacketStates8 ps;

    const auto result = curve.intersectPacketHits(rays, ps.states);

    for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
      EXPECT_FALSE(result.hit(lane));
      EXPECT_EQ(1, ps.lanes[lane].intersectionMisses);
      EXPECT_EQ(0u, ps.lanes[lane].packetHitScalarFallbacks);
    }
  }

  TEST(CurveTessellate, RibbonModeProducesOneQuadPerNonZeroSegment) {
    Curve curve(
      core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0), Vector3d(1.0, 1.0, 0.0)}),
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
    Curve curve(
      core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}),
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

  TEST(CurveTessellate, ScalarSegmentAttributesMapToFaceColors) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                             Vector3d(2.0, 0.0, 0.0), Vector3d(3.0, 0.0, 0.0)});
    polyline.setSegmentAttribute(0, "temperature", 100.0);
    polyline.setSegmentAttribute(1, "temperature", 150.0);

    Curve curve(polyline, 0.2, Curve::TessellationMode::Ribbon);
    curve.setSegmentColorMap(
      core::AttributeColorMap::scalar("temperature", 100.0, 200.0, Colord::blue(), Colord::red()));

    auto mesh = curve.tessellate();

    ASSERT_EQ(3u, mesh->faces().size());
    ASSERT_TRUE(mesh->faceColor(0).has_value());
    ASSERT_TRUE(mesh->faceColor(1).has_value());
    EXPECT_EQ(Colord::blue(), *mesh->faceColor(0));
    EXPECT_EQ(Colord(0.5, 0.0, 0.5), *mesh->faceColor(1));
    EXPECT_FALSE(mesh->faceColor(2).has_value());
  }

  TEST(CurveTessellate, CategoricalSegmentAttributesUseConfiguredColors) {
    core::Polyline polyline(
      {Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0), Vector3d(2.0, 0.0, 0.0)});
    polyline.setSegmentAttribute(0, "route", std::string("travel"));
    polyline.setSegmentAttribute(1, "route", std::string("print"));

    auto colorMap = core::AttributeColorMap::categorical("route");
    colorMap.setCategoryColor(std::string("travel"), Colord::blue());
    colorMap.setCategoryColor(std::string("print"), Colord::green());

    Curve curve(polyline, 0.2, Curve::TessellationMode::Ribbon);
    curve.setSegmentColorMap(colorMap);

    auto mesh = curve.tessellate();

    ASSERT_EQ(2u, mesh->faces().size());
    ASSERT_TRUE(mesh->faceColor(0).has_value());
    ASSERT_TRUE(mesh->faceColor(1).has_value());
    EXPECT_EQ(Colord::blue(), *mesh->faceColor(0));
    EXPECT_EQ(Colord::green(), *mesh->faceColor(1));
  }

  TEST(CurveTessellate, CategoricalFallbackColorIsDeterministicForRepeatedValues) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                             Vector3d(2.0, 0.0, 0.0), Vector3d(3.0, 0.0, 0.0)});
    polyline.setSegmentAttribute(0, "phase", std::string("roughing"));
    polyline.setSegmentAttribute(1, "phase", std::string("finishing"));
    polyline.setSegmentAttribute(2, "phase", std::string("roughing"));

    Curve curve(polyline, 0.2, Curve::TessellationMode::Ribbon);
    curve.setSegmentColorMap(core::AttributeColorMap::categorical("phase"));

    auto mesh = curve.tessellate();

    ASSERT_EQ(3u, mesh->faces().size());
    ASSERT_TRUE(mesh->faceColor(0).has_value());
    ASSERT_TRUE(mesh->faceColor(1).has_value());
    ASSERT_TRUE(mesh->faceColor(2).has_value());
    EXPECT_EQ(*mesh->faceColor(0), *mesh->faceColor(2));
    EXPECT_NE(*mesh->faceColor(0), *mesh->faceColor(1));
  }

  TEST(CurveTessellate, TubeModeAppliesSegmentColorToEveryGeneratedFace) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)});
    polyline.setSegmentAttribute(0, "route", std::string("print"));

    auto colorMap = core::AttributeColorMap::categorical("route");
    colorMap.setCategoryColor(std::string("print"), Colord::red());
    Curve curve(polyline, 0.2, Curve::TessellationMode::Tube);
    curve.setSegmentColorMap(colorMap);

    auto mesh = curve.tessellate(0);

    ASSERT_EQ(8u, mesh->faces().size());
    for (std::size_t i = 0; i != mesh->faces().size(); ++i) {
      ASSERT_TRUE(mesh->faceColor(i).has_value());
      EXPECT_EQ(Colord::red(), *mesh->faceColor(i));
    }
  }
}
