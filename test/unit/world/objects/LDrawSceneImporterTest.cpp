#include <gtest/gtest.h>

#include "render/primitives/Scene.h"
#include "world/import/LDrawSceneImporter.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/LDrawSceneImporter.h"
#include "world/objects/Scene.h"

#include "render/primitives/Instance.h"

#include <QJsonObject>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace LDrawSceneImporterTest {
  using namespace render;

  class TempTree {
  public:
    TempTree()
        : m_root(fs::temp_directory_path() /
                 fs::path(std::string("raytracer-ldraw-scene-importer-") +
                          std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" +
                          std::to_string(++s_nextId))) {
      fs::create_directories(m_root);
      std::error_code error;
      const fs::path canonical = fs::weakly_canonical(m_root, error);
      if (!error) {
        m_root = canonical;
      }
    }

    ~TempTree() {
      std::error_code error;
      fs::remove_all(m_root, error);
    }

    fs::path write(const fs::path& relativePath, const std::string& contents) const {
      const fs::path path = m_root / relativePath;
      fs::create_directories(path.parent_path());
      std::ofstream output(path);
      output << contents;
      return path;
    }

  private:
    fs::path m_root;
    static int s_nextId;
  };

  int TempTree::s_nextId = 0;

  Group* childGroup(Element* parent, int index) {
    auto* group = dynamic_cast<Group*>(parent->childElements()[index]);
    EXPECT_NE(nullptr, group);
    return group;
  }

  TEST(LDrawSceneImporter, PreservesStepBoundariesAsOrderedGroups) {
    TempTree tree;
    const auto model = tree.write("steps.ldr", "3 16 0 0 0 1 0 0 0 1 0\n"
                                               "0 STEP\n"
                                               "3 16 0 0 1 1 0 1 0 1 1\n");

    LDrawSceneImporter::Options options;
    options.filePath = QString::fromStdString(model.string());

    const auto result = LDrawSceneImporter().importFile(options);
    auto* root = dynamic_cast<Group*>(result.root.get());
    ASSERT_NE(nullptr, root);
    ASSERT_EQ(2, root->childElements().size());

    auto* firstStep = childGroup(root, 0);
    auto* secondStep = childGroup(root, 1);
    EXPECT_EQ(1, firstStep->metadataValue("buildStepIndex").toInt());
    EXPECT_EQ(2, secondStep->metadataValue("buildStepIndex").toInt());
    EXPECT_EQ(QString("ldraw"), firstStep->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString::fromStdString(model.string()),
              firstStep->metadataValue("sourceFile").toString());
    EXPECT_EQ(1, firstStep->childElements().size());
    EXPECT_EQ(1, secondStep->childElements().size());
  }

  TEST(LDrawSceneImporter, PreservesMpdSubmodelsAsNestedGroups) {
    TempTree tree;
    const auto model = tree.write("model.mpd", "0 FILE main.ldr\n"
                                               "1 16 10 0 0 1 0 0 0 1 0 0 0 1 child.ldr\n"
                                               "0 NOFILE\n"
                                               "0 FILE child.ldr\n"
                                               "3 16 0 0 0 1 0 0 0 1 0\n"
                                               "0 NOFILE\n");

    LDrawSceneImporter::Options options;
    options.filePath = QString::fromStdString(model.string());

    const auto result = LDrawSceneImporter().importFile(options);
    auto* root = dynamic_cast<Group*>(result.root.get());
    ASSERT_NE(nullptr, root);
    ASSERT_EQ(1, root->childElements().size());

    auto* firstStep = childGroup(root, 0);
    ASSERT_EQ(1, firstStep->childElements().size());
    auto* submodel = dynamic_cast<Group*>(firstStep->childElements()[0]);
    ASSERT_NE(nullptr, submodel);
    EXPECT_EQ(QString("child.ldr"), submodel->metadataValue("submodelName").toString());
    EXPECT_EQ(QString("child.ldr"), submodel->metadataValue("sourceBlock").toString());
    EXPECT_EQ(10.0, submodel->position().x());
    ASSERT_EQ(1, submodel->childElements().size());

    auto* childStep = childGroup(submodel, 0);
    EXPECT_EQ(1, childStep->metadataValue("buildStepIndex").toInt());
    EXPECT_EQ(1, childStep->childElements().size());
  }

  TEST(LDrawSceneImporter, CanReturnFlattenedMetadataGroupWhenHierarchyPreservationIsDisabled) {
    TempTree tree;
    const auto model = tree.write("flat.ldr", "3 16 0 0 0 1 0 0 0 1 0\n"
                                              "0 STEP\n"
                                              "3 16 0 0 1 1 0 1 0 1 1\n");

    LDrawSceneImporter::Options options;
    options.filePath = QString::fromStdString(model.string());
    options.preserveHierarchy = false;

    const auto result = LDrawSceneImporter().importFile(options);

    auto* ldrawModel = dynamic_cast<Group*>(result.root.get());
    ASSERT_NE(nullptr, ldrawModel);
    EXPECT_EQ(QString("LDraw"), ldrawModel->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString::fromStdString(model.string()),
              ldrawModel->metadataValue("sourcePath").toString());
    EXPECT_EQ(QString("flat"), ldrawModel->metadataValue("normalMode").toString());
  }

  TEST(LDrawSceneImporter, PreservedHierarchyStillConvertsToRuntimeScene) {
    TempTree tree;
    const auto model = tree.write("steps.ldr", "3 16 0 0 0 1 0 0 0 1 0\n"
                                               "0 STEP\n"
                                               "3 16 0 0 1 1 0 1 0 1 1\n");

    LDrawSceneImporter::Options options;
    options.filePath = QString::fromStdString(model.string());
    auto result = LDrawSceneImporter().importFile(options);

    ::Scene scene;
    scene.addChild(std::move(result.root));
    const auto runtime = scene.toRaytracerScene();

    EXPECT_EQ(1u, runtime->primitives().size());
  }

  TEST(LDrawSceneImporter, KeepsLDrawReferenceAsCollectionMetadata) {
    Group model;
    model.setId("model");
    model.setName("Imported Assembly");
    model.setScale(Vector3d(0.5, 0.5, 0.5));

    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = "model.ldr";
    metadata["libraryPath"] = "ldraw";
    metadata["scale"] = 0.5;
    metadata["coordinateConversion"] = "ldraw_to_raytracer";
    metadata["preserveHierarchy"] = false;
    metadata["normalMode"] = "smooth";
    metadata["includeEdgeOverlays"] = false;
    metadata["maxRecursion"] = 8;
    metadata["missingPartPolicy"] = "skip";
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
    EXPECT_EQ(0.5, roundTripped->metadataValue("scale").toDouble());
    EXPECT_EQ(QString("ldraw_to_raytracer"),
              roundTripped->metadataValue("coordinateConversion").toString());
    EXPECT_FALSE(roundTripped->metadataValue("preserveHierarchy").toBool());
    EXPECT_EQ(QString("smooth"), roundTripped->metadataValue("normalMode").toString());
    EXPECT_FALSE(roundTripped->metadataValue("includeEdgeOverlays").toBool());
    EXPECT_EQ(8, roundTripped->metadataValue("maxRecursion").toInt());
    EXPECT_EQ(QString("skip"), roundTripped->metadataValue("missingPartPolicy").toString());
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

  TEST(LDrawSceneImporter, AppliesExplicitRenderOptionsFromWorldMetadata) {
    Group model;
    model.setId("ldraw");
    model.setName("LDraw Import");

    QJsonObject metadata;
    metadata["sourceFormat"] = "LDraw";
    metadata["sourcePath"] = "test/fixtures/ldraw/rendercli/model.ldr";
    metadata["libraryRoot"] = "test/fixtures/ldraw/rendercli/library";
    metadata["scale"] = 2.0;
    metadata["coordinateConversion"] = "ldraw_to_raytracer";
    metadata["includeEdgeOverlays"] = false;
    metadata["normalMode"] = "smooth";
    metadata["maxRecursion"] = 4;
    metadata["missingPartPolicy"] = "error";
    model.setMetadata(metadata);

    world::imports::resolveLDrawAuthoringImports(&model);

    ASSERT_EQ(1, model.childElements().size());
    render::Scene renderScene;
    auto primitive = model.toRaytracer(&renderScene);
    ASSERT_NE(nullptr, std::dynamic_pointer_cast<render::Instance>(primitive));
    const auto& box = primitive->boundingBox();
    EXPECT_NEAR(-2.0, box.min().x(), 1e-9);
    EXPECT_NEAR(0.0, box.min().y(), 1e-9);
    EXPECT_NEAR(-2.0, box.min().z(), 1e-9);
    EXPECT_NEAR(2.0, box.max().x(), 1e-9);
    EXPECT_NEAR(0.0, box.max().y(), 1e-9);
    EXPECT_NEAR(2.0, box.max().z(), 1e-9);
  }

  TEST(LDrawSceneImporter, PreservedHierarchyMetadataDoesNotTriggerAuthoringImport) {
    Group model;
    model.setId("ldraw-step");
    model.setName("LDraw Step");

    QJsonObject metadata;
    metadata["sourceFormat"] = "ldraw";
    metadata["sourceFile"] = "model.ldr";
    metadata["sourceBlock"] = "model.ldr";
    metadata["buildStepIndex"] = 1;
    model.setMetadata(metadata);

    EXPECT_NO_THROW(world::imports::resolveLDrawAuthoringImports(&model));
    EXPECT_TRUE(model.childElements().empty());
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
