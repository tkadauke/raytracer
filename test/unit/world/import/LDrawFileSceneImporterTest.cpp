#include <gtest/gtest.h>

#include "render/primitives/Scene.h"
#include "world/import/ImportOptions.h"
#include "world/import/LDrawFileSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QTemporaryFile>

namespace LDrawFileSceneImporterTest {

  TEST(LDrawFileSceneImporter, RegistersForLDrawExtensions) {
    auto importer = world::SceneImporterRegistry::self().createForFile("model.mpd");

    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("ldraw"), importer->name());
    EXPECT_TRUE(importer->supportedExtensions().contains("ldr"));
    EXPECT_TRUE(importer->supportedExtensions().contains("dat"));
    EXPECT_TRUE(importer->supportedExtensions().contains("mpd"));
  }

  TEST(LDrawFileSceneImporter, ImportsCompleteSceneWithFramedCamera) {
    world::LDrawFileSceneImporter importer;
    world::ImportOptions options;
    options.setValue("library_root", "test/fixtures/ldraw/rendercli/library");

    auto result = importer.importFile("test/fixtures/ldraw/rendercli/model.ldr", options);

    ASSERT_TRUE(result.succeeded());
    auto* scene = result.sceneRoot();
    ASSERT_NE(nullptr, scene);
    EXPECT_EQ(QString("model"), scene->name());

    auto* camera = qobject_cast<PinholeCamera*>(scene->activeCamera());
    ASSERT_NE(nullptr, camera);
    EXPECT_NEAR(camera->target().x(), camera->position().x(), 1e-9);
    EXPECT_NEAR(camera->target().y(), camera->position().y(), 1e-9);
    EXPECT_LT(camera->position().z(), camera->target().z());
    EXPECT_EQ(Colord::white(), scene->background());
    EXPECT_EQ(Colord(0.8, 0.8, 0.8), scene->ambient());

    const auto runtime = scene->toRaytracerScene();
    EXPECT_EQ(1u, runtime->primitives().size());
    EXPECT_TRUE(runtime->boundingBox().isValid());
  }

  TEST(LDrawFileSceneImporter, ChoosesBVHForMultiPolygonImportedScene) {
    QTemporaryFile file("raytracer-ldraw-acceleration-XXXXXX.ldr");
    ASSERT_TRUE(file.open());
    file.write("3 16 -1 -1 0 0 1 0 1 -1 0\n"
               "3 16 -1 1 0 1 1 0 0 -1 0\n");
    const QString path = file.fileName();
    file.close();

    world::LDrawFileSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.sceneRoot());
    const auto runtime = result.sceneRoot()->toRaytracerScene();
    ASSERT_TRUE(runtime->accelerationDecision().has_value());
    EXPECT_EQ(render::SpatialIndexKind::BVH, runtime->accelerationDecision()->spatialIndexKind);
  }

  TEST(LDrawFileSceneImporter, AcceptsNamedAndHexSceneBackgroundColors) {
    world::LDrawFileSceneImporter importer;
    world::ImportOptions options;
    options.setValue("library_root", "test/fixtures/ldraw/rendercli/library");
    options.setValue("background_color", "336699");
    options.setValue("ambient_color", "black");

    auto result = importer.importFile("test/fixtures/ldraw/rendercli/model.ldr", options);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.sceneRoot());
    EXPECT_NEAR(0.2, result.sceneRoot()->background().r(), 1e-6);
    EXPECT_NEAR(0.4, result.sceneRoot()->background().g(), 1e-6);
    EXPECT_NEAR(0.6, result.sceneRoot()->background().b(), 1e-6);
    EXPECT_EQ(Colord::black(), result.sceneRoot()->ambient());
  }

  TEST(LDrawFileSceneImporter, ReportsInvalidOptionsAsImportErrors) {
    world::LDrawFileSceneImporter importer;
    world::ImportOptions options;
    options.setValue("missing_part_policy", "maybe");

    const auto result = importer.importFile("test/fixtures/ldraw/rendercli/model.ldr", options);

    EXPECT_TRUE(result.failed());
    ASSERT_FALSE(result.diagnostics().empty());
    EXPECT_TRUE(result.diagnostics().front().isError());
  }

  TEST(LDrawFileSceneImporter, ReportsInvalidSceneColorAsImportError) {
    world::LDrawFileSceneImporter importer;
    world::ImportOptions options;
    options.setValue("background_color", "definitely-not-a-color");

    const auto result = importer.importFile("test/fixtures/ldraw/rendercli/model.ldr", options);

    EXPECT_TRUE(result.failed());
    ASSERT_FALSE(result.diagnostics().empty());
    EXPECT_TRUE(result.diagnostics().front().isError());
  }
}
