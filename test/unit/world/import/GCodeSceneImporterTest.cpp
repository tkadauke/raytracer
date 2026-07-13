#include <gtest/gtest.h>

#include "render/primitives/Curve.h"
#include "render/primitives/Scene.h"
#include "render/cameras/Camera.h"
#include "core/geometry/Mesh.h"
#include "test/helpers/ImporterTestHelper.h"
#include "world/objects/Camera.h"
#include "world/import/GCodeSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <vector>

namespace GCodeSceneImporterTest {
  using test::importers::childGroup;

  Group* firstGroupWithSourceFormat(Element* parent, const QString& sourceFormat) {
    for (Element* child : parent->childElements()) {
      auto* group = dynamic_cast<Group*>(child);
      if (group && group->metadataValue("sourceFormat").toString() == sourceFormat)
        return group;
    }
    return nullptr;
  }

  std::vector<CompiledPrimitive*> compiledPrimitives(Element* root) {
    std::vector<CompiledPrimitive*> result;
    if (auto* primitive = dynamic_cast<CompiledPrimitive*>(root))
      result.push_back(primitive);
    for (Element* child : root->childElements()) {
      const auto childPrimitives = compiledPrimitives(child);
      result.insert(result.end(), childPrimitives.begin(), childPrimitives.end());
    }
    return result;
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

  TEST(GCodeSceneImporter, SelectsVisualizationAttributeColorMap) {
    world::GCodeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("visualization", "speed");

    const auto result = importer.importFile("test/fixtures/gcode/absolute_layers.gcode", options);

    ASSERT_TRUE(result.succeeded());
    const auto primitives = compiledPrimitives(result.sceneRoot());
    ASSERT_FALSE(primitives.empty());
    auto curve =
      std::dynamic_pointer_cast<render::Curve>(primitives.front()->toRaytracerPrimitive());
    ASSERT_NE(nullptr, curve);
    ASSERT_TRUE(curve->segmentColorMap());
    EXPECT_EQ("speed", curve->segmentColorMap()->attributeName());
    EXPECT_EQ(core::AttributeColorMap::Mode::Scalar, curve->segmentColorMap()->mode());
  }

  TEST(GCodeSceneImporter, CanHideTravelMoves) {
    world::GCodeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("hide_travel", true);

    const auto result = importer.importFile("test/fixtures/gcode/absolute_layers.gcode", options);

    ASSERT_TRUE(result.succeeded());
    const auto primitives = compiledPrimitives(result.sceneRoot());
    ASSERT_EQ(2u, primitives.size());
    for (auto* primitive : primitives) {
      EXPECT_EQ(QStringLiteral("extrusion"), primitive->metadataValue("moveType").toString());
    }
  }

  TEST(GCodeSceneImporter, ImportsRenderableCurveGeometry) {
    world::GCodeSceneImporter importer;
    const auto result = importer.importFile("test/fixtures/gcode/absolute_layers.gcode");

    ASSERT_TRUE(result.succeeded());
    auto runtimeScene = result.sceneRoot()->toRaytracerScene();
    ASSERT_NE(nullptr, runtimeScene);
    auto mesh = runtimeScene->tessellate(0);
    ASSERT_NE(nullptr, mesh);
    EXPECT_GT(mesh->faces().size(), 0u);

    auto* camera = result.sceneRoot()->activeCamera();
    ASSERT_NE(nullptr, camera);
    auto runtimeCamera = camera->toRaytracer();
    runtimeCamera->viewPlane()->setup(runtimeCamera->matrix(), Recti(0, 0, 64, 64));
    const Vector2d projected = runtimeCamera->projectPoint(Vector3d(10.0, 0.0, 0.2));
    EXPECT_FALSE(projected.isUndefined());
    EXPECT_GE(projected.x(), 0.0);
    EXPECT_LT(projected.x(), 64.0);
    EXPECT_GE(projected.y(), 0.0);
    EXPECT_LT(projected.y(), 64.0);
  }
}
