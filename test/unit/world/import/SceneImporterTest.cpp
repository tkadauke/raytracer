#include <gtest/gtest.h>

#include "world/import/SceneImporter.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <memory>

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

}
