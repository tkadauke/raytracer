#include "world/import/SceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"
#include "world/objects/SourceAsset.h"
#include "world/objects/Sphere.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>

#include <gtest/gtest.h>

namespace SourceAssetTest {
  class StubSourceAssetImporter : public world::SceneImporter {
  public:
    QString name() const override {
      return QStringLiteral("Source Asset Stub");
    }

    QStringList supportedExtensions() const override {
      return {"assetstub"};
    }

    world::ImportOptionSchemas optionSchema() const override {
      return {};
    }

    world::ImportResult importFile(const QString& filename,
                                   const world::ImportOptions& options) const override {
      QFile file(filename);
      world::ImportSourceMetadata source;
      source.importerName = name();
      source.sourcePath = filename;

      if (!file.open(QIODevice::ReadOnly)) {
        return world::ImportResult::failed(
          {world::ImportDiagnostic::error("Unable to read stub source asset", filename)}, source);
      }

      auto group = std::make_unique<Group>();
      group->setName(
        options.value("groupName", QStringLiteral("Compiled Source Asset")).toString());

      auto sphere = std::make_unique<Sphere>();
      sphere->setName(QStringLiteral("Compiled Sphere"));
      group->addChild(std::move(sphere));

      world::ImportResult result(std::move(group), source);
      result.addDiagnostic(world::ImportDiagnostic::warning(
        "Stub source asset imported with fallback tessellation", filename, 2, 3));
      return result;
    }
  };

  class EditableSourceAssetImporter : public world::SceneImporter {
  public:
    QString name() const override {
      return QStringLiteral("Editable Source Asset Stub");
    }

    QStringList supportedExtensions() const override {
      return {"editableasset"};
    }

    world::ImportOptionSchemas optionSchema() const override {
      return {};
    }

    world::ImportOptionSchemas
    editableSourceParameters(const QString&, const world::ImportOptions&) const override {
      return {
        {"length",
         world::ImportOptionType::Double,
         "Length",
         "Editable source length.",
         2.5,
         false,
         {}},
        {"enabled",
         world::ImportOptionType::Boolean,
         "Enabled",
         "Editable source enabled flag.",
         true,
         false,
         {}},
        {"wall_thickness",
         world::ImportOptionType::Double,
         "",
         "Editable wall thickness.",
         0.0,
         false,
         {}},
      };
    }

    world::ImportResult importFile(const QString& filename,
                                   const world::ImportOptions& options) const override {
      auto group = std::make_unique<Group>();
      const auto defines = options.value("define").toJsonObject();
      group->setName(QString("length=%1 enabled=%2")
                       .arg(defines.value("length").toDouble(2.5))
                       .arg(defines.value("enabled").toBool(true)));

      world::ImportSourceMetadata source;
      source.importerName = name();
      source.sourcePath = filename;
      return world::ImportResult(std::move(group), source);
    }
  };

  void registerStubImporter() {
    world::SceneImporterRegistry::self().registerClass<StubSourceAssetImporter>(
      "source-asset-stub");
  }

  void registerEditableImporter() {
    world::SceneImporterRegistry::self().registerClass<EditableSourceAssetImporter>(
      "editable-source-asset-stub");
  }

  QString writeTemporarySource(const QString& extension = QStringLiteral("assetstub")) {
    QTemporaryFile file(QDir::temp().filePath(QString("source-asset-XXXXXX.%1").arg(extension)));
    EXPECT_TRUE(file.open());
    file.write("stub");
    const QString filename = file.fileName();
    file.setAutoRemove(false);
    return filename;
  }

  TEST(SourceAsset, ShouldBeRegisteredWithElementFactory) {
    auto asset = ElementFactory::self().create("SourceAsset");

    ASSERT_NE(nullptr, asset);
    EXPECT_NE(nullptr, qobject_cast<SourceAsset*>(asset.get()));
  }

  TEST(SourceAsset, RoundTripsSourceOptionsCacheKeyAndDiagnosticsThroughSceneJson) {
    registerStubImporter();
    const QString source = writeTemporarySource();

    Scene scene;
    auto asset = std::make_unique<SourceAsset>();
    asset->setSourcePath(source);
    asset->setFormat("source-asset-stub");
    asset->setGeneratedOutputCacheKey("openscad-cache:v1");
    asset->setImportOptions(QJsonObject{{"groupName", "Generated Gear"}});
    scene.addChild(std::move(asset));

    QJsonObject json;
    scene.write(json);

    const auto children = json["children"].toArray();
    ASSERT_EQ(1, children.size());
    const auto assetJson = children[0].toObject();
    EXPECT_EQ(QString("SourceAsset"), assetJson["type"].toString());
    EXPECT_EQ(source, assetJson["sourcePath"].toString());
    EXPECT_EQ(QString("source-asset-stub"), assetJson["format"].toString());
    EXPECT_EQ(QString("openscad-cache:v1"), assetJson["generatedOutputCacheKey"].toString());
    EXPECT_EQ(QString("Generated Gear"),
              assetJson["importOptions"].toObject()["groupName"].toString());
    EXPECT_FALSE(assetJson.contains("children"));

    Scene decoded;
    decoded.read(json);
    ASSERT_EQ(2, decoded.childElements().size());
    auto* decodedAsset = qobject_cast<SourceAsset*>(decoded.childElements()[1]);
    ASSERT_NE(nullptr, decodedAsset);
    EXPECT_EQ(source, decodedAsset->sourcePath());
    EXPECT_EQ(QString("source-asset-stub"), decodedAsset->format());
    EXPECT_EQ(QString("openscad-cache:v1"), decodedAsset->generatedOutputCacheKey());
    EXPECT_EQ(QString("Generated Gear"), decodedAsset->importOptions()["groupName"].toString());
    ASSERT_EQ(1, decodedAsset->diagnostics().size());
    EXPECT_TRUE(decodedAsset->diagnostics()[0].isWarning());
    EXPECT_EQ(QString("Stub source asset imported with fallback tessellation"),
              decodedAsset->diagnostics()[0].message);
  }

  TEST(SourceAsset, ReportsMissingSourceDiagnosticsWithoutThrowing) {
    registerStubImporter();

    SourceAsset asset;
    asset.setSourcePath("missing-source.assetstub");
    asset.setFormat("source-asset-stub");

    EXPECT_NO_THROW(asset.rebuildGeneratedChildren());
    ASSERT_EQ(1, asset.diagnostics().size());
    EXPECT_TRUE(asset.diagnostics()[0].isError());
    EXPECT_EQ(QString("Unable to read stub source asset"), asset.diagnostics()[0].message);
    EXPECT_TRUE(asset.childElements().empty());
  }

  TEST(SourceAsset, InvokesImporterAndAttachesGeneratedOutput) {
    registerStubImporter();
    const QString source = writeTemporarySource();

    SourceAsset asset;
    asset.setSourcePath(source);
    asset.setFormat("source-asset-stub");
    asset.setImportOptions(QJsonObject{{"groupName", "Compiled Output"}});

    asset.rebuildGeneratedChildren();

    ASSERT_EQ(1, asset.childElements().size());
    Element* importedRoot = asset.childElements().front();
    EXPECT_TRUE(importedRoot->isGenerated());
    EXPECT_EQ(QString("Compiled Output"), importedRoot->name());
    ASSERT_EQ(1, importedRoot->childElements().size());
    EXPECT_TRUE(importedRoot->childElements().front()->isGenerated());
    EXPECT_EQ(QString("Compiled Sphere"), importedRoot->childElements().front()->name());
    ASSERT_EQ(1, asset.diagnostics().size());
    EXPECT_TRUE(asset.diagnostics()[0].isWarning());
  }

  TEST(SourceAsset, ExposesEditableImporterParametersAndRebuildsWhenTheyChange) {
    registerEditableImporter();
    const QString source = writeTemporarySource(QStringLiteral("editableasset"));

    SourceAsset asset;
    asset.setSourcePath(source);
    asset.setFormat("editable-source-asset-stub");
    asset.rebuildGeneratedChildren();

    EXPECT_EQ(QStringLiteral("Source Parameters"), asset.propertyGroup("length"));
    EXPECT_EQ(QStringLiteral("Length"), asset.propertyDisplayName("length"));
    EXPECT_EQ(QStringLiteral("Wall Thickness"), asset.propertyDisplayName("wall_thickness"));
    EXPECT_EQ(QStringLiteral("Editable source length."), asset.propertyDescription("length"));
    EXPECT_DOUBLE_EQ(2.5, asset.property("length").toDouble());
    EXPECT_TRUE(asset.property("enabled").toBool());
    ASSERT_EQ(1, asset.childElements().size());
    EXPECT_EQ(QString("length=2.5 enabled=1"), asset.childElements().front()->name());

    asset.setProperty("length", 4.0);
    asset.propertyEdited("length");

    const auto defines = asset.importOptions().value("define").toObject();
    EXPECT_DOUBLE_EQ(4.0, defines.value("length").toDouble());
    ASSERT_EQ(1, asset.childElements().size());
    EXPECT_EQ(QString("length=4 enabled=1"), asset.childElements().front()->name());

    QJsonObject json;
    asset.write(json);
    EXPECT_FALSE(json.contains("length"));
    EXPECT_DOUBLE_EQ(4.0,
                     json["importOptions"].toObject()["define"].toObject()["length"].toDouble());
  }
}
