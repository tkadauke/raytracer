#include <gtest/gtest.h>

#include "render/primitives/Scene.h"
#include "world/import/ImportOptions.h"
#include "world/import/LDrawFileSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

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
    EXPECT_NE(Vector3d(0.0, 0.0, -1.0), camera->position());

    const auto runtime = scene->toRaytracerScene();
    EXPECT_EQ(1u, runtime->primitives().size());
    EXPECT_TRUE(runtime->boundingBox().isValid());
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

}
