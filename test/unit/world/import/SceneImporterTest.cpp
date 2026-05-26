#include <gtest/gtest.h>

#include "world/import/JsonSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/import/SceneImporter.h"
#include "world/objects/Group.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include <memory>
#include <QJsonObject>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

namespace SceneImporterTest {
  namespace {
    class FakeImporter : public world::SceneImporter {
    public:
      QString name() const override {
        return "Fake";
      }

      QStringList supportedExtensions() const override {
        return {"fake", "fakez"};
      }

      world::ImportOptionSchemas optionSchema() const override {
        return {
          {"mergeGroups", world::ImportOptionType::Boolean, "Merge groups",
           "Flatten hierarchy-only groups during import.", false, false, {}},
          {"quality", world::ImportOptionType::Choice, "Quality", "Geometry conversion quality.",
           "balanced", true, {"fast", "balanced", "exact"}},
        };
      }

      world::ImportResult importFile(const QString& filename,
                                     const world::ImportOptions& options) const override {
        world::ImportSourceMetadata source;
        source.importerName = name();
        source.formatName = "Fake scene";
        source.sourcePath = filename;
        source.properties = {{"extension", "fake"}};

        if (options.value("fail").toBool()) {
          return world::ImportResult::failed(
            {world::ImportDiagnostic::error("Unable to import fake scene", filename, 12, 4)},
            source);
        }

        auto scene = std::make_unique<Scene>();
        scene->setName("Imported fake scene");

        world::ImportResult result(std::move(scene), source);
        result.addDiagnostic(world::ImportDiagnostic::warning("Ignored unsupported annotation",
                                                             filename, 3, 1));
        return result;
      }
    };

    class OptionEchoImporter : public world::SceneImporter {
    public:
      QString name() const override {
        return "option-echo";
      }

      QStringList supportedExtensions() const override {
        return {};
      }

      world::ImportOptionSchemas optionSchema() const override {
        return {};
      }

      world::ImportResult importFile(const QString& filename,
                                     const world::ImportOptions& options) const override {
        auto group = std::make_unique<Group>();
        group->setName(options.value("groupName", filename).toString());
        return world::ImportResult(std::move(group));
      }
    };
  }

  TEST(SceneImporter, ReportsSupportedExtensionsAndOptionSchema) {
    FakeImporter importer;

    EXPECT_EQ(QString("Fake"), importer.name());
    EXPECT_EQ(QStringList({"fake", "fakez"}), importer.supportedExtensions());

    const auto schema = importer.optionSchema();
    ASSERT_EQ(2u, schema.size());
    EXPECT_EQ(QString("mergeGroups"), schema[0].name);
    EXPECT_EQ(world::ImportOptionType::Boolean, schema[0].type);
    EXPECT_FALSE(schema[0].required);
    EXPECT_EQ(false, schema[0].defaultValue.toBool());
    EXPECT_EQ(QString("quality"), schema[1].name);
    EXPECT_EQ(world::ImportOptionType::Choice, schema[1].type);
    EXPECT_TRUE(schema[1].required);
    EXPECT_EQ(QString("balanced"), schema[1].defaultValue.toString());
    EXPECT_EQ(QStringList({"fast", "balanced", "exact"}), schema[1].choices);
  }

  TEST(ImportOptions, StoresFormatNeutralOptionValues) {
    world::ImportOptions options;
    options.setValue("mergeGroups", true);
    options.setValue("quality", "exact");

    EXPECT_TRUE(options.contains("mergeGroups"));
    EXPECT_TRUE(options.value("mergeGroups").toBool());
    EXPECT_EQ(QString("exact"), options.value("quality").toString());
    EXPECT_EQ(7, options.value("missing", 7).toInt());
  }

  TEST(ImportResult, CarriesSuccessfulSceneRootDiagnosticsAndSourceMetadata) {
    FakeImporter importer;

    const auto result = importer.importFile("example.fake", world::ImportOptions());

    EXPECT_TRUE(result.succeeded());
    EXPECT_FALSE(result.failed());
    ASSERT_NE(nullptr, result.root());
    ASSERT_NE(nullptr, result.sceneRoot());
    EXPECT_EQ(nullptr, result.groupRoot());
    EXPECT_EQ(QString("Imported fake scene"), result.sceneRoot()->name());
    EXPECT_TRUE(result.hasWarnings());
    EXPECT_FALSE(result.hasErrors());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isWarning());
    EXPECT_EQ(QString("example.fake"), result.diagnostics()[0].source);
    EXPECT_EQ(3, result.diagnostics()[0].line);
    EXPECT_EQ(QString("Fake"), result.source().importerName);
    EXPECT_EQ(QString("Fake scene"), result.source().formatName);
    EXPECT_EQ(QString("example.fake"), result.source().sourcePath);
    EXPECT_EQ(QString("fake"), result.source().properties["extension"].toString());
  }

  TEST(ImportResult, CarriesGroupRoots) {
    auto group = std::make_unique<Group>();
    group->setName("Imported assembly");

    world::ImportResult result(std::move(group));

    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(nullptr, result.sceneRoot());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(QString("Imported assembly"), result.groupRoot()->name());

    auto ownedRoot = result.takeRoot();
    EXPECT_FALSE(result.hasRoot());
    ASSERT_NE(nullptr, ownedRoot);
    EXPECT_EQ(QString("Imported assembly"), ownedRoot->name());
  }

  TEST(ImportResult, AttachesRootProvenanceFromSourceMetadata) {
    world::ImportSourceMetadata source;
    source.sourcePath = "model.ldr";

    auto group = std::make_unique<Group>();
    world::ImportResult result(std::move(group), source);

    auto provenance = world::ImportProvenance::fromSource(result.source());
    provenance.sourceId = "submodel.dat";
    provenance.recordId = "0 FILE submodel.dat";
    provenance.originalUnits = "ldu";
    provenance.category = QJsonObject{{"ldrawRecordType", 1}};
    result.setRootProvenance(provenance);

    const auto rootProvenance = result.rootProvenance();
    ASSERT_TRUE(rootProvenance.has_value());
    EXPECT_EQ(QString("model.ldr"), rootProvenance->sourceFile);
    EXPECT_EQ(QString("submodel.dat"), rootProvenance->sourceId);
    EXPECT_EQ(QString("0 FILE submodel.dat"), rootProvenance->recordId);
    EXPECT_EQ(QString("ldu"), rootProvenance->originalUnits);
    EXPECT_EQ(1, rootProvenance->category["ldrawRecordType"].toInt());
  }

  TEST(ImportResult, PreservesNestedGroupAndObjectProvenanceThroughSceneJson) {
    auto rootGroup = std::make_unique<Group>();
    rootGroup->setId("{90000000-0000-0000-0000-00000000a001}");

    world::ImportProvenance rootProvenance;
    rootProvenance.sourceFile = "nested.fake";
    rootProvenance.sourceId = "assembly/root";
    rootProvenance.category = QJsonObject{{"kind", "assembly"}};
    world::setImportProvenance(*rootGroup, rootProvenance);

    auto* childGroup = new Group;
    childGroup->setId("{90000000-0000-0000-0000-00000000a002}");
    world::ImportProvenance childProvenance;
    childProvenance.sourceFile = "nested.fake";
    childProvenance.sourceId = "assembly/child";
    childProvenance.lineStart = 20;
    childProvenance.lineEnd = 24;
    world::setImportProvenance(*childGroup, childProvenance);
    rootGroup->addChild(childGroup);

    auto* sphere = new Sphere;
    sphere->setId("{90000000-0000-0000-0000-00000000a003}");
    world::ImportProvenance sphereProvenance;
    sphereProvenance.sourceFile = "nested.fake";
    sphereProvenance.sourceId = "assembly/child/sphere";
    sphereProvenance.recordId = "sphere-7";
    sphereProvenance.originalUnits = "cm";
    sphereProvenance.category = QJsonObject{{"kind", "analytic-surface"}};
    world::setImportProvenance(*sphere, sphereProvenance);
    childGroup->addChild(sphere);

    world::ImportResult result(std::move(rootGroup));
    Scene scene;
    scene.addChild(result.takeRoot());

    QJsonObject json;
    scene.write(json);

    Scene decoded;
    decoded.read(json);

    auto* decodedRoot =
      dynamic_cast<Group*>(decoded.findById("{90000000-0000-0000-0000-00000000a001}"));
    auto* decodedChild =
      dynamic_cast<Group*>(decoded.findById("{90000000-0000-0000-0000-00000000a002}"));
    auto* decodedSphere =
      dynamic_cast<Sphere*>(decoded.findById("{90000000-0000-0000-0000-00000000a003}"));
    ASSERT_NE(nullptr, decodedRoot);
    ASSERT_NE(nullptr, decodedChild);
    ASSERT_NE(nullptr, decodedSphere);

    const auto decodedRootProvenance = world::importProvenance(*decodedRoot);
    const auto decodedChildProvenance = world::importProvenance(*decodedChild);
    const auto decodedSphereProvenance = world::importProvenance(*decodedSphere);
    ASSERT_TRUE(decodedRootProvenance.has_value());
    ASSERT_TRUE(decodedChildProvenance.has_value());
    ASSERT_TRUE(decodedSphereProvenance.has_value());
    EXPECT_EQ(QString("assembly/root"), decodedRootProvenance->sourceId);
    EXPECT_EQ(QString("assembly/child"), decodedChildProvenance->sourceId);
    EXPECT_EQ(20, decodedChildProvenance->lineStart);
    EXPECT_EQ(24, decodedChildProvenance->lineEnd);
    EXPECT_EQ(QString("sphere-7"), decodedSphereProvenance->recordId);
    EXPECT_EQ(QString("cm"), decodedSphereProvenance->originalUnits);
    EXPECT_EQ(QString("analytic-surface"),
              decodedSphereProvenance->category["kind"].toString());
  }

  TEST(ImportResult, CarriesFailedDiagnosticsWithoutRoot) {
    FakeImporter importer;
    world::ImportOptions options;
    options.setValue("fail", true);

    const auto result = importer.importFile("broken.fake", options);

    EXPECT_FALSE(result.succeeded());
    EXPECT_TRUE(result.failed());
    EXPECT_FALSE(result.hasRoot());
    EXPECT_EQ(nullptr, result.root());
    EXPECT_TRUE(result.hasErrors());
    EXPECT_FALSE(result.hasWarnings());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isError());
    EXPECT_EQ(QString("Unable to import fake scene"), result.diagnostics()[0].message);
    EXPECT_EQ(QString("broken.fake"), result.diagnostics()[0].source);
    EXPECT_EQ(12, result.diagnostics()[0].line);
    EXPECT_EQ(4, result.diagnostics()[0].column);
    EXPECT_EQ(QString("Fake"), result.source().importerName);
    EXPECT_EQ(QString("broken.fake"), result.source().sourcePath);
  }

  TEST(SceneImporterRegistry, CreatesImportersByFormatAndExtension) {
    auto byFormat = world::SceneImporterRegistry::self().createByFormat("json");
    auto byExtension = world::SceneImporterRegistry::self().createForFile("fixture.rtjson");

    ASSERT_NE(nullptr, byFormat);
    ASSERT_NE(nullptr, byExtension);
    EXPECT_EQ(QString("json"), byFormat->name());
    EXPECT_EQ(QString("json"), byExtension->name());
    EXPECT_EQ(nullptr, world::SceneImporterRegistry::self().createByFormat("missing"));
  }

  TEST(JsonSceneImporter, ImportsNativeSceneJson) {
    QTemporaryFile temp("raytracer-import-XXXXXX.rtjson");
    ASSERT_TRUE(temp.open());
    const QString path = temp.fileName();
    temp.write(R"({
      "id": "scene",
      "name": "Imported JSON",
      "type": "Scene",
      "children": [
        {
          "id": "camera",
          "name": "Camera",
          "position": [0.0, 0.0, -3.0],
          "target": [0.0, 0.0, 0.0],
          "distance": 5.0,
          "zoom": 1.0,
          "type": "PinholeCamera",
          "children": []
        }
      ]
    })");
    temp.close();

    world::JsonSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.sceneRoot());
    EXPECT_EQ(QString("Imported JSON"), result.sceneRoot()->name());
    EXPECT_NE(nullptr, qobject_cast<PinholeCamera*>(result.sceneRoot()->activeCamera()));
    EXPECT_EQ(QString("json"), result.source().importerName);
  }

  TEST(JsonSceneImporter, ReportsFatalDiagnosticsForMissingInput) {
    world::JsonSceneImporter importer;

    const auto result = importer.importFile("missing.rtjson");

    EXPECT_TRUE(result.failed());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isError());
    EXPECT_EQ(QString("Unable to read import source"), result.diagnostics()[0].message);
  }

  TEST(SceneImports, PassOptionsToRegisteredImporter) {
    world::SceneImporterRegistry::self().registerClass<OptionEchoImporter>("option-echo");

    Scene scene;
    scene.read(QJsonObject(
      {{"id", "scene"},
       {"type", "Scene"},
       {"imports",
        QJsonArray({QJsonObject({{"source", "fixture.echo"},
                                 {"format", "option-echo"},
                                 {"options", QJsonObject({{"groupName", "Imported Group"}})}})})},
       {"children", QJsonArray()}}));

    ASSERT_EQ(1, scene.childElements().size());
    auto* group = qobject_cast<Group*>(scene.childElements().front());
    ASSERT_NE(nullptr, group);
    EXPECT_EQ(QString("Imported Group"), group->name());
  }

}
