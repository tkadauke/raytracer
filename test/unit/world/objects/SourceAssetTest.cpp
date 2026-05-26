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

  void registerStubImporter() {
    world::SceneImporterRegistry::self().registerClass<StubSourceAssetImporter>(
      "source-asset-stub");
  }

  QString writeTemporarySource() {
    QTemporaryFile file(QDir::temp().filePath("source-asset-XXXXXX.assetstub"));
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
}
