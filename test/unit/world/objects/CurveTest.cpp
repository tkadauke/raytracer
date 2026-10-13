#include <gtest/gtest.h>

#include "world/objects/Curve.h"
#include "world/objects/ElementFactory.h"

#include <QJsonArray>
#include <QJsonObject>

namespace CurveWorldTest {
  namespace {
    // Exposes the protected runtime factory so tests can pin its dispatch.
    class TestableCurve : public Curve {
    public:
      using Curve::toRaytracerPrimitive;
    };

    core::Polyline attributedPolyline() {
      core::Polyline polyline({Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(1, 2, 0)});
      polyline.setAttribute("route", std::string("line-7"));
      polyline.setAttribute("closed", false);
      polyline.setSegmentAttribute(0, "speed", 12.5);
      polyline.setSegmentAttribute(0, "category", std::string("travel"));
      polyline.setSegmentAttribute(1, "layer", 3);
      polyline.setSegmentAttribute(1, "direction", Vector3d(0, 1, 0));
      return polyline;
    }
  }

  TEST(WorldCurve, ShouldDefaultToZeroWidthRibbon) {
    Curve curve;
    EXPECT_DOUBLE_EQ(0.0, curve.width());
    EXPECT_EQ(QStringLiteral("ribbon"), curve.tessellationMode());
    EXPECT_TRUE(curve.polyline().empty());
  }

  TEST(WorldCurve, ShouldClampNegativeWidthToZero) {
    Curve curve;
    curve.setWidth(-1.0);
    EXPECT_DOUBLE_EQ(0.0, curve.width());
    curve.setWidth(0.25);
    EXPECT_DOUBLE_EQ(0.25, curve.width());
  }

  TEST(WorldCurve, ShouldNormalizeTessellationModeCaseAndFallBackToRibbon) {
    Curve curve;
    curve.setTessellationMode(QStringLiteral("TUBE"));
    EXPECT_EQ(QStringLiteral("tube"), curve.tessellationMode());
    curve.setTessellationMode(QStringLiteral("bogus"));
    EXPECT_EQ(QStringLiteral("ribbon"), curve.tessellationMode());
  }

  TEST(WorldCurve, ShouldRegisterWithElementFactory) {
    auto element = ElementFactory::self().create("Curve");
    ASSERT_NE(nullptr, element);
    EXPECT_NE(nullptr, dynamic_cast<Curve*>(element.get()));
  }

  TEST(WorldCurve, ShouldRoundTripPointsAndAttributesThroughJson) {
    Curve original;
    original.setPolyline(attributedPolyline());
    original.setWidth(0.4);

    QJsonObject json;
    original.write(json);
    ASSERT_EQ(3, json["points"].toArray().size());
    ASSERT_EQ(2, json["segmentAttributes"].toArray().size());

    Curve restored;
    restored.read(json);

    const auto& polyline = restored.polyline();
    ASSERT_EQ(3u, polyline.pointCount());
    ASSERT_EQ(2u, polyline.segmentCount());
    EXPECT_DOUBLE_EQ(2.0, polyline.point(2).y());
    EXPECT_EQ("line-7", *polyline.attributeAs<std::string>("route"));
    EXPECT_FALSE(*polyline.attributeAs<bool>("closed"));
    EXPECT_DOUBLE_EQ(12.5, *polyline.segmentAttributeAs<double>(0, "speed"));
    EXPECT_EQ("travel", *polyline.segmentAttributeAs<std::string>(0, "category"));
    EXPECT_EQ(3, *polyline.segmentAttributeAs<int>(1, "layer"));
    EXPECT_DOUBLE_EQ(1.0, polyline.segmentAttributeAs<Vector3d>(1, "direction")->y());
  }

  TEST(WorldCurve, ShouldOmitEmptyAttributeSectionsFromJson) {
    Curve curve;
    curve.setPolyline(core::Polyline({Vector3d(0, 0, 0), Vector3d(1, 0, 0)}));

    QJsonObject json;
    curve.write(json);

    EXPECT_FALSE(json.contains("curveAttributes"));
    ASSERT_EQ(2, json["points"].toArray().size());
  }

  TEST(WorldCurve, ShouldIgnoreSegmentAttributesBeyondSegmentCount) {
    QJsonObject json;
    QJsonArray points;
    points.append(QJsonArray{0, 0, 0});
    points.append(QJsonArray{1, 0, 0});
    json["points"] = points;
    QJsonArray segments;
    segments.append(QJsonObject{{"speed", 1.5}});
    segments.append(QJsonObject{{"speed", 9.5}});
    json["segmentAttributes"] = segments;

    Curve curve;
    curve.read(json);

    ASSERT_EQ(1u, curve.polyline().segmentCount());
    EXPECT_DOUBLE_EQ(1.5, *curve.polyline().segmentAttributeAs<double>(0, "speed"));
  }

  TEST(WorldCurve, ShouldProduceRuntimeCurveWithWidthModeAndBounds) {
    TestableCurve curve;
    curve.setPolyline(attributedPolyline());
    curve.setWidth(0.5);
    curve.setTessellationMode(QStringLiteral("tube"));

    auto runtime = std::dynamic_pointer_cast<render::Curve>(curve.toRaytracerPrimitive());
    ASSERT_NE(nullptr, runtime);
    EXPECT_DOUBLE_EQ(0.5, runtime->width());
    EXPECT_EQ(render::Curve::TessellationMode::Tube, runtime->tessellationMode());
    EXPECT_EQ(3u, runtime->polyline().pointCount());
    EXPECT_NE(nullptr, runtime->tessellate(0));
  }
}
