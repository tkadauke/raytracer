#include <gtest/gtest.h>

#include "render/primitives/Scene.h"
#include "world/import/LDrawSceneImporter.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <QJsonObject>

#include <memory>

namespace LDrawSceneImporterTest {
  using namespace render;

  TEST(LDrawSceneImporter, KeepsLDrawReferenceAsCollectionMetadata) {
    Group model;
    model.setId("model");
    model.setName("Imported Assembly");
    model.setScale(Vector3d(0.5, 0.5, 0.5));

    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = "model.ldr";
    metadata["libraryPath"] = "ldraw";
    metadata["normalMode"] = "smooth";
    model.setMetadata(metadata);

    QJsonObject json;
    model.write(json);

    auto element = ElementFactory::self().create("Collection");
    ASSERT_NE(nullptr, element);
    element->read(json);
    auto* roundTripped = dynamic_cast<Group*>(element.get());
    ASSERT_NE(nullptr, roundTripped);

    EXPECT_EQ(QString("LDraw"), roundTripped->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString("model.ldr"), roundTripped->metadataValue("sourcePath").toString());
    EXPECT_EQ(QString("ldraw"), roundTripped->metadataValue("libraryPath").toString());
    EXPECT_EQ(QString("smooth"), roundTripped->metadataValue("normalMode").toString());
    EXPECT_EQ(Vector3d(0.5, 0.5, 0.5), roundTripped->scale());
  }

  TEST(LDrawSceneImporter, AttachesGeneratedCompiledGeometryToCollection) {
    Group model;
    model.setId("ldraw");
    model.setName("LDraw Import");

    world::imports::LDrawImportOptions options;
    options.filePath = "test/fixtures/ldraw/rendercli/model.ldr";
    options.libraryPath = "test/fixtures/ldraw/rendercli/library";

    world::imports::attachLDrawImport(&model, options);

    ASSERT_EQ(1, model.childElements().size());
    EXPECT_TRUE(model.childElements().front()->isGenerated());
  }

  TEST(LDrawSceneImporter, SceneConversionRendersImportedCollection) {
    auto scene = std::make_unique<::Scene>();
    auto model = std::make_unique<Group>();
    model->setId("ldraw");
    model->setName("LDraw Import");

    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = "test/fixtures/ldraw/rendercli/model.ldr";
    metadata["libraryPath"] = "test/fixtures/ldraw/rendercli/library";
    metadata["normalMode"] = "flat";
    model->setMetadata(metadata);
    scene->addChild(std::move(model));

    world::imports::resolveLDrawAuthoringImports(scene.get());
    auto renderScene = scene->toRaytracerScene();

    ASSERT_EQ(1u, renderScene->primitives().size());
    EXPECT_FALSE(renderScene->primitives().front()->boundingBox().isInfinite());
  }
}
