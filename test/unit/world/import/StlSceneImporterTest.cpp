#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "render/primitives/MeshPrimitive.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/import/StlSceneImporter.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"

#include "test/helpers/ImporterTestHelper.h"

#include <memory>

namespace StlSceneImporterTest {
  using test::importers::ExpectedDiagnostic;
  using test::importers::expectDiagnostics;

  TEST(StlSceneImporter, RegistersForStlExtension) {
    EXPECT_TRUE(world::SceneImporterRegistry::self().hasFormat("stl"));
    EXPECT_TRUE(world::SceneImporterRegistry::self().hasExtension("stl"));
    EXPECT_NE(nullptr, world::SceneImporterRegistry::self().createForFile("part.stl"));
  }

  TEST(StlSceneImporter, ImportsAsciiFixtureAsCompiledMeshPrimitive) {
    const QString path = "test/fixtures/stl/triangle_ascii.stl";
    world::StlSceneImporter importer;

    auto result = importer.importFile(path);

    EXPECT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(QString("triangle_ascii"), result.groupRoot()->name());
    EXPECT_EQ(QString("STL"), result.groupRoot()->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString("scene units (STL is unitless)"),
              result.groupRoot()->metadataValue("units").toString());
    EXPECT_EQ(QString("flat"), result.groupRoot()->metadataValue("normalMode").toString());
    EXPECT_EQ(1, result.groupRoot()->metadataValue("triangleCount").toInt());

    ASSERT_EQ(1, result.groupRoot()->childElements().size());
    auto* compiled = qobject_cast<CompiledPrimitive*>(result.groupRoot()->childElements().front());
    ASSERT_NE(nullptr, compiled);
    auto meshPrimitive =
      std::dynamic_pointer_cast<render::MeshPrimitive>(compiled->toRaytracerPrimitive());
    ASSERT_NE(nullptr, meshPrimitive);
    ASSERT_NE(nullptr, meshPrimitive->mesh());
    EXPECT_EQ(1u, meshPrimitive->mesh()->faces().size());
    EXPECT_EQ(render::MeshPrimitive::NormalMode::Flat, meshPrimitive->normalMode());

    expectDiagnostics(result.diagnostics(),
                      {ExpectedDiagnostic::warning(
                         "STL is unitless; coordinates are imported as scene units.", path),
                       ExpectedDiagnostic::warning(
                         "STL carries no material data; imported mesh uses the default material.",
                         path)});
  }

  TEST(StlSceneImporter, ImportsBinaryFixtureAndReportsEncoding) {
    const QString path = "test/fixtures/stl/triangle_binary.stl";
    world::StlSceneImporter importer;

    auto result = importer.importFile(path);

    EXPECT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(2, result.groupRoot()->metadataValue("triangleCount").toInt());
    EXPECT_EQ(QString("binary"), result.source().properties["encoding"].toString());
    EXPECT_EQ(2, result.source().properties["triangleCount"].toInt());
  }

  TEST(StlSceneImporter, ReturnsErrorDiagnosticForMalformedFixture) {
    const QString path = "test/fixtures/stl/malformed_ascii.stl";
    world::StlSceneImporter importer;

    auto result = importer.importFile(path);

    EXPECT_TRUE(result.failed());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isError());
    EXPECT_EQ(path, result.diagnostics()[0].source);
  }
}
