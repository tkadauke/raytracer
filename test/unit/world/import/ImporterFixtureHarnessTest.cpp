#include <gtest/gtest.h>

#include "core/formats/AssetResolver.h"
#include "test/helpers/ImporterTestHelper.h"
#include "world/import/SceneImporter.h"
#include "world/objects/Group.h"

#include <memory>

#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

namespace ImporterFixtureHarnessTest {
  namespace {
    class MinimalFixtureImporter : public world::SceneImporter {
    public:
      QString name() const override {
        return "minimal-fixture";
      }

      QStringList supportedExtensions() const override {
        return {"minimport"};
      }

      world::ImportOptionSchemas optionSchema() const override {
        return {{"includeHidden", world::ImportOptionType::Boolean, "Include hidden groups",
                 "Import hidden source groups instead of dropping them.", true, false, {}}};
      }

      world::ImportResult importFile(const QString& filename,
                                     const world::ImportOptions& options) const override {
        QFile sourceFile(filename);
        if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
          return world::ImportResult::failed(
            {world::ImportDiagnostic::error("Unable to read minimal importer fixture", filename)});
        }

        auto root = std::make_unique<Group>();
        world::ImportResult result(std::move(root), metadataFor(filename));

        QTextStream stream(&sourceFile);
        while (!stream.atEnd()) {
          const QString line = stream.readLine();
          if (line.startsWith("root=")) {
            result.groupRoot()->setName(line.mid(QString("root=").size()));
          } else if (line.startsWith("asset=")) {
            attachAssetMetadata(*result.groupRoot(), filename, line.mid(QString("asset=").size()));
          } else if (line.startsWith("group=")) {
            addGroup(*result.groupRoot(), line.mid(QString("group=").size()), options);
          } else if (line.startsWith("warning=")) {
            addWarning(result, filename, line.mid(QString("warning=").size()));
          }
        }

        return result;
      }

    private:
      static world::ImportSourceMetadata metadataFor(const QString& filename) {
        world::ImportSourceMetadata source;
        source.importerName = "minimal-fixture";
        source.formatName = "Minimal fixture importer";
        source.sourcePath = filename;
        source.properties = {{"fixture", true}};
        return source;
      }

      static void attachAssetMetadata(Group& root,
                                      const QString& sourceFilename,
                                      const QString& requestedAsset) {
        const core::AssetResolver resolver;
        const auto resolved =
          resolver.resolve(requestedAsset.toStdString(), sourceFilename.toStdString());

        QFile assetFile(QString::fromStdString(resolved.path.string()));
        ASSERT_TRUE(assetFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const auto assetJson = QJsonDocument::fromJson(assetFile.readAll()).object();

        root.setMetadataValue("requestedAsset", requestedAsset);
        root.setMetadataValue("assetKind", assetJson["assetKind"]);
        root.setMetadataValue("sourceLibrary", assetJson["sourceLibrary"]);
        root.setMetadataValue("assetIdentity", QString::fromStdString(resolved.identity));
      }

      static void addGroup(Group& root, const QString& payload, const world::ImportOptions& options) {
        const QStringList fields = payload.split('|');
        ASSERT_EQ(3, fields.size());

        const bool visible = fields[1] == "true";
        if (!visible && !options.value("includeHidden", true).toBool())
          return;

        auto group = std::make_unique<Group>();
        group->setName(fields[0]);
        group->setVisible(visible);
        group->setMetadataValue("sourceId", fields[2]);
        root.addChild(std::move(group));
      }

      static void addWarning(world::ImportResult& result,
                             const QString& sourceFilename,
                             const QString& payload) {
        const QStringList fields = payload.split('|');
        ASSERT_EQ(3, fields.size());
        result.addDiagnostic(world::ImportDiagnostic::warning(
          fields[0], sourceFilename, fields[1].toInt(), fields[2].toInt()));
      }
    };
  }

  TEST(ImporterFixtureHarness, AssertsDiagnosticsGroupStructureAndSidecarAssets) {
    const QString fixture = test::importers::importerFixturePath("minimal/scene.minimport");

    MinimalFixtureImporter importer;
    const auto result = importer.importFile(fixture, world::ImportOptions());

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(QString("minimal-fixture"), result.source().importerName);
    EXPECT_EQ(fixture, result.source().sourcePath);

    test::importers::expectDiagnostics(
      result.diagnostics(),
      {test::importers::ExpectedDiagnostic::warning("Ignored fixture annotation", fixture, 5, 2)});

    test::importers::expectGroupTree(
      *result.groupRoot(),
      {"Fixture Assembly",
       true,
       {{"requestedAsset", "assets/provenance.json"},
        {"assetKind", "palette"},
        {"sourceLibrary", "fixture-sidecar"}},
       {{"Visible Part", true, {{"sourceId", "fixture/visible-part"}}, {}},
        {"Hidden Part", false, {{"sourceId", "fixture/hidden-part"}}, {}}}});

    EXPECT_TRUE(result.groupRoot()->metadataValue("assetIdentity").toString().endsWith(
      "test/fixtures/importers/minimal/assets/provenance.json"));
  }

  TEST(ImporterFixtureHarness, OptionsCanDriveExpectedGroupShape) {
    const QString fixture = test::importers::importerFixturePath("minimal/scene.minimport");
    world::ImportOptions options;
    options.setValue("includeHidden", false);

    MinimalFixtureImporter importer;
    const auto result = importer.importFile(fixture, options);

    ASSERT_TRUE(result.succeeded());
    test::importers::expectGroupTree(
      *result.groupRoot(),
      {"Fixture Assembly",
       true,
       {{"requestedAsset", "assets/provenance.json"}},
       {{"Visible Part", true, {{"sourceId", "fixture/visible-part"}}, {}}}});
  }

}
