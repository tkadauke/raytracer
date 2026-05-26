#include <gtest/gtest.h>

#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/LDrawModel.h"
#include "world/objects/Scene.h"

#include <QJsonObject>

#include <memory>

namespace LDrawModelTest {
  using namespace render;

  TEST(LDrawModel, DefaultsToNoPathsFlatNormalsAndVisible) {
    LDrawModel model;

    EXPECT_TRUE(model.filePath().isEmpty());
    EXPECT_TRUE(model.libraryPath().isEmpty());
    EXPECT_FALSE(model.smoothNormals());
    EXPECT_TRUE(model.visible());
  }

  TEST(LDrawModel, RegistersWithElementFactory) {
    auto element = ElementFactory::self().create("LDrawModel");

    ASSERT_NE(nullptr, element);
    EXPECT_NE(nullptr, dynamic_cast<LDrawModel*>(element.get()));
  }

  TEST(LDrawModel, PropertiesRoundTripThroughJson) {
    LDrawModel model;
    model.setId("model");
    model.setFilePath("model.ldr");
    model.setLibraryPath("ldraw");
    model.setSmoothNormals(true);
    model.setVisible(false);
    model.setScale(Vector3d(0.5, 0.5, 0.5));

    QJsonObject json;
    model.write(json);

    auto element = ElementFactory::self().create(json["type"].toString().toStdString());
    ASSERT_NE(nullptr, element);
    element->read(json);
    auto* roundTripped = dynamic_cast<LDrawModel*>(element.get());
    ASSERT_NE(nullptr, roundTripped);

    EXPECT_EQ(QString("model.ldr"), roundTripped->filePath());
    EXPECT_EQ(QString("ldraw"), roundTripped->libraryPath());
    EXPECT_TRUE(roundTripped->smoothNormals());
    EXPECT_FALSE(roundTripped->visible());
    EXPECT_EQ(Vector3d(0.5, 0.5, 0.5), roundTripped->scale());
  }

  TEST(LDrawModel, BuildsRuntimePrimitiveTreeFromFileAndLibraryRoot) {
    LDrawModel model;
    model.setFilePath("test/fixtures/ldraw/rendercli/model.ldr");
    model.setLibraryPath("test/fixtures/ldraw/rendercli/library");

    auto primitive = model.toRaytracerPrimitive();

    auto composite = std::dynamic_pointer_cast<Composite>(primitive);
    ASSERT_NE(nullptr, composite);
    ASSERT_EQ(1u, composite->primitives().size());
    auto instance = std::dynamic_pointer_cast<Instance>(composite->primitives().front());
    ASSERT_NE(nullptr, instance);
    auto child = std::dynamic_pointer_cast<Composite>(instance->primitive());
    ASSERT_NE(nullptr, child);
    ASSERT_EQ(1u, child->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<MeshPrimitive>(child->primitives().front()));
  }

  TEST(LDrawModel, SceneConversionIncludesVisibleModel) {
    ::Scene scene;
    auto model = std::make_unique<LDrawModel>();
    model->setFilePath("test/fixtures/ldraw/rendercli/model.ldr");
    model->setLibraryPath("test/fixtures/ldraw/rendercli/library");
    scene.addChild(std::move(model));

    auto renderScene = scene.toRaytracerScene();

    ASSERT_EQ(1u, renderScene->primitives().size());
  }
}
