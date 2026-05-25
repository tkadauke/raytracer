#include <gtest/gtest.h>

#include "world/objects/Curve.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Scene.h"

#include <QJsonArray>
#include <QJsonObject>

namespace CurveTest {
  class ExposedCurve : public Curve {
  public:
    using Curve::toRaytracerPrimitive;
  };

  TEST(Curve, ShouldDefaultToRibbonDisplayOptions) {
    Curve curve;

    EXPECT_DOUBLE_EQ(0.1, curve.width());
    EXPECT_EQ(QString("ribbon"), curve.tessellationMode());
    EXPECT_TRUE(curve.pointVectors().empty());
  }

  TEST(Curve, ShouldSetAndGetTypedPoints) {
    Curve curve;
    curve.setPointVectors({Vector3d(1, 2, 3), Vector3d(4, 5, 6)});

    ASSERT_EQ(2u, curve.pointVectors().size());
    EXPECT_EQ(Vector3d(1, 2, 3), curve.pointVectors()[0]);
    EXPECT_EQ(Vector3d(4, 5, 6), curve.pointVectors()[1]);
  }

  TEST(Curve, ShouldRoundtripPointsAndDisplayOptionsViaJson) {
    Curve original;
    original.setId("curve-id");
    original.setName("curve name");
    original.setPointVectors({Vector3d(1.0, 2.0, 3.0), Vector3d(4.0, 5.0, 6.0)});
    original.setWidth(0.25);
    original.setTessellationMode("tube");

    QJsonObject json;
    original.write(json);

    EXPECT_EQ(QString("Curve"), json["type"].toString());
    ASSERT_TRUE(json["points"].isArray());
    ASSERT_EQ(2, json["points"].toArray().size());
    EXPECT_TRUE(json["points"].toArray()[0].isArray());
    EXPECT_DOUBLE_EQ(0.25, json["width"].toDouble());
    EXPECT_EQ(QString("tube"), json["tessellationMode"].toString());

    Curve decoded;
    decoded.read(json);

    EXPECT_EQ(QString("curve-id"), decoded.id());
    EXPECT_EQ(QString("curve name"), decoded.name());
    EXPECT_DOUBLE_EQ(0.25, decoded.width());
    EXPECT_EQ(QString("tube"), decoded.tessellationMode());
    ASSERT_EQ(2u, decoded.pointVectors().size());
    EXPECT_EQ(Vector3d(1.0, 2.0, 3.0), decoded.pointVectors()[0]);
    EXPECT_EQ(Vector3d(4.0, 5.0, 6.0), decoded.pointVectors()[1]);
  }

  TEST(Curve, ShouldBeRegisteredWithElementFactory) {
    auto curve = ElementFactory::self().create("Curve");

    ASSERT_NE(nullptr, curve);
    EXPECT_NE(nullptr, dynamic_cast<Curve*>(curve.get()));
  }

  TEST(Curve, ShouldBeAcceptedByGroups) {
    Group group;
    Curve curve;

    EXPECT_TRUE(group.canHaveChild(&curve));
  }

  TEST(Curve, ShouldConvertToRuntimeCurvePrimitive) {
    ExposedCurve curve;
    curve.setPointVectors({Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(1, 1, 0)});
    curve.setWidth(0.5);
    curve.setTessellationMode("tube");

    auto primitive = std::dynamic_pointer_cast<render::Curve>(curve.toRaytracerPrimitive());

    ASSERT_NE(nullptr, primitive);
    EXPECT_DOUBLE_EQ(0.5, primitive->width());
    EXPECT_EQ(render::Curve::TessellationMode::Tube, primitive->tessellationMode());
    ASSERT_EQ(3u, primitive->polyline().pointCount());
    EXPECT_EQ(Vector3d(1, 1, 0), primitive->polyline().point(2));
  }

  TEST(Curve, ShouldRenderWhenNestedInsideGroup) {
    Scene scene;
    auto* group = new Group;
    auto* curve = new Curve;
    curve->setPointVectors({Vector3d(0, 0, 0), Vector3d(1, 0, 0)});
    curve->setWidth(0.5);
    group->addChild(curve);
    scene.addChild(group);

    auto rt = scene.toRaytracerScene();

    EXPECT_EQ(1u, rt->primitives().size());
  }
}
