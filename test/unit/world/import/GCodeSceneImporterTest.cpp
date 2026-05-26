#include <gtest/gtest.h>

#include "render/primitives/Curve.h"
#include "world/import/GCodeSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

namespace GCodeSceneImporterTest {
  Group* childGroup(Element* parent, int index) {
    auto* group = dynamic_cast<Group*>(parent->childElements()[index]);
    EXPECT_NE(nullptr, group);
    return group;
  }

  Group* firstGroupWithSourceFormat(Element* parent, const QString& sourceFormat) {
    for (Element* child : parent->childElements()) {
      auto* group = dynamic_cast<Group*>(child);
      if (group && group->metadataValue("sourceFormat").toString() == sourceFormat)
        return group;
    }
    return nullptr;
  }

  TEST(GCodeSceneImporter, IsRegisteredForGCodeExtensions) {
    auto importer = world::SceneImporterRegistry::self().createForFile("part.gcode");
    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QStringLiteral("gcode"), importer->name());
  }

  TEST(GCodeSceneImporter, ImportsLayersToolsFeaturesAndCurveMetadata) {
    world::GCodeSceneImporter importer;
    const auto result = importer.importFile("test/fixtures/gcode/absolute_layers.gcode");

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.sceneRoot());

    auto* root = firstGroupWithSourceFormat(result.sceneRoot(), QStringLiteral("gcode"));
    ASSERT_NE(nullptr, root);
    EXPECT_EQ(QStringLiteral("gcode"), root->metadataValue("sourceFormat").toString());
    ASSERT_EQ(2, root->childElements().size());

    auto* layerZero = childGroup(root, 0);
    EXPECT_EQ(0, layerZero->stepIndex());
    EXPECT_EQ(0, layerZero->layerIndex());
    EXPECT_EQ(0.2, layerZero->metadataValue("z").toDouble());
    ASSERT_EQ(1, layerZero->childElements().size());

    auto* toolZero = childGroup(layerZero, 0);
    EXPECT_EQ(0, toolZero->metadataValue("tool").toInt());
    ASSERT_EQ(1, toolZero->childElements().size());

    auto* skirt = childGroup(toolZero, 0);
    EXPECT_EQ(QStringLiteral("SKIRT"), skirt->metadataValue("featureType").toString());
    ASSERT_EQ(3, skirt->childElements().size());

    auto* extrusionSurface = dynamic_cast<CompiledPrimitive*>(skirt->childElements()[1]);
    ASSERT_NE(nullptr, extrusionSurface);
    EXPECT_EQ(QStringLiteral("extrusion"), extrusionSurface->metadataValue("moveType").toString());

    auto curve = std::dynamic_pointer_cast<render::Curve>(extrusionSurface->toRaytracerPrimitive());
    ASSERT_NE(nullptr, curve);
    ASSERT_EQ(1u, curve->polyline().segmentCount());
    EXPECT_EQ("extrusion", *curve->polyline().segmentAttributeAs<std::string>(0, "move_type"));
    EXPECT_DOUBLE_EQ(900.0, *curve->polyline().segmentAttributeAs<double>(0, "speed"));
    EXPECT_DOUBLE_EQ(0.8, *curve->polyline().segmentAttributeAs<double>(0, "extrusion_amount"));

    auto* layerOne = childGroup(root, 1);
    EXPECT_EQ(1, layerOne->stepIndex());
    EXPECT_EQ(1, layerOne->layerIndex());
    auto* wall = childGroup(childGroup(layerOne, 0), 0);
    EXPECT_EQ(QStringLiteral("WALL-INNER"), wall->metadataValue("featureType").toString());
  }
}
