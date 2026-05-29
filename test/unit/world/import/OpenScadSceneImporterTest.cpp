#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Difference.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "world/import/ImportResult.h"
#include "world/import/OpenScadCompiler.h"
#include "world/import/OpenScadSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Box.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Group.h"
#include "world/objects/Intersection.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/SourceAsset.h"
#include "world/objects/Sphere.h"
#include "world/objects/Union.h"
#include "test/helpers/VectorTestHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>

#include <cmath>
#include <memory>

namespace OpenScadSceneImporterTest {
  namespace {
    std::unique_ptr<QTemporaryFile> writeScad(const QByteArray& source) {
      auto file = std::make_unique<QTemporaryFile>("raytracer-openscad-XXXXXX.scad");
      EXPECT_TRUE(file->open());
      file->write(source);
      file->close();
      return file;
    }

    template<class T>
    bool containsPrimitive(const std::shared_ptr<render::Primitive>& primitive) {
      if (!primitive)
        return false;
      if (std::dynamic_pointer_cast<T>(primitive))
        return true;
      if (auto instance = std::dynamic_pointer_cast<render::Instance>(primitive))
        return containsPrimitive<T>(instance->primitive());
      if (auto composite = std::dynamic_pointer_cast<render::Composite>(primitive)) {
        for (const auto& child : composite->primitives()) {
          if (containsPrimitive<T>(child))
            return true;
        }
      }
      return false;
    }

    const char* fakeOpenScadScript() {
      return R"SH(#!/bin/sh
out=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = "-o" ]; then
    shift
    out="$1"
  fi
  shift
done
echo run >> "$OPENSCAD_FAKE_LOG"
case "$out" in
*.ply)
cat > "$out" <<'PLY'
ply
format ascii 1.0
element vertex 3
property float x
property float y
property float z
property float nx
property float ny
property float nz
element face 1
property list uchar int vertex_indices
end_header
0 0 0 0 0 1
1 0 0 0 0 1
0 1 0 0 0 1
3 0 1 2
PLY
;;
*)
cat > "$out" <<'STL'
solid openscad
  facet normal 0 0 0
    outer loop
      vertex 0 0 0
      vertex 1 0 0
      vertex 0 1 0
    endloop
  endfacet
endsolid openscad
STL
;;
esac
exit 0
)SH";
    }

    QString writeExecutable(QTemporaryDir& dir) {
      const QString path = dir.filePath("openscad-fake");
      QFile file(path);
      EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
      file.write(fakeOpenScadScript());
      file.close();
      EXPECT_TRUE(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                QFileDevice::ExeOwner));
      return path;
    }

    QString sourceFixture() {
      return QStringLiteral("test/fixtures/openscad/external-compiler/compiler_smoke.scad");
    }

    class CrashingOpenScadProcess : public world::OpenScadProcess {
    public:
      int run(const QString& executable, const QStringList& arguments,
              const QString& workingDirectory, QString* standardOutput,
              QString* standardError) const override {
        (void)executable;
        (void)arguments;
        (void)workingDirectory;
        if (standardOutput)
          standardOutput->clear();
        if (standardError)
          *standardError = "Abort trap: 6";
        return -1;
      }
    };

    QJsonObject optionsFor(const QString& executable, const QString& cacheDirectory) {
      return QJsonObject{{"executable", executable}, {"cacheDirectory", cacheDirectory}};
    }

    int logRunCount(const QString& path) {
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly))
        return 0;
      return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts).size();
    }
  }

  TEST(OpenScadSceneImporter, DocumentsFixtureSetForSupportedWorkflows) {
    const QStringList fixtures = {
      "test/fixtures/openscad/external-compiler/compiler_smoke.scad",
      "test/fixtures/openscad/native-subset/simple_primitives.scad",
      "test/fixtures/openscad/native-subset/transforms.scad",
      "test/fixtures/openscad/native-subset/booleans.scad",
    };

    for (const auto& fixture : fixtures) {
      EXPECT_TRUE(QFileInfo::exists(fixture)) << fixture.toStdString();
    }
  }

  TEST(OpenScadSceneImporter, RegistersForScadFiles) {
    auto importer = world::SceneImporterRegistry::self().createForFile("part.scad");

    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("openscad"), importer->name());
  }

  TEST(OpenScadSceneImporter, ListsTopLevelAssignmentsAsEditableSourceParameters) {
    QTemporaryFile source(QDir::temp().filePath("openscad-params-XXXXXX.scad"));
    ASSERT_TRUE(source.open());
    source.write(R"(
      width = 999;
      /*<!!start test_model!!>*/
      /* [General Cup] */
      // X dimension. grid units or mm.
      width = [2, 0]; //0.1
      mode = "normal"; //[normal, reduced, none:not stackable]
      sides = 6; //[4:square, 6:Hex, 64:circle]
      wall_thickness = 0; // .01
      /* [Subdivisions] */
      enabled = true;
      /* [Hidden] */
      hidden_value = 99;
      /*<!!end test_model!!>*/
      after = 123;
    )");
    source.close();

    world::OpenScadSceneImporter importer;
    const auto parameters = importer.editableSourceParameters(source.fileName(), {});

    ASSERT_EQ(5u, parameters.size());
    EXPECT_EQ(QString("width"), parameters[0].name);
    EXPECT_EQ(QString("General Cup"), parameters[0].group);
    EXPECT_EQ(world::ImportOptionType::String, parameters[0].type);
    EXPECT_EQ(QString("[2, 0]"), parameters[0].defaultValue.toString());
    EXPECT_EQ(QString("X dimension. grid units or mm."), parameters[0].description);
    EXPECT_DOUBLE_EQ(0.1, parameters[0].step.toDouble());

    EXPECT_EQ(QString("mode"), parameters[1].name);
    EXPECT_EQ(world::ImportOptionType::Choice, parameters[1].type);
    EXPECT_EQ(QString("\"normal\""), parameters[1].defaultValue.toString());
    EXPECT_EQ((QStringList{QStringLiteral("\"normal\""), QStringLiteral("\"reduced\""),
                           QStringLiteral("\"none\"")}),
              parameters[1].choices);

    EXPECT_EQ(QString("sides"), parameters[2].name);
    EXPECT_EQ(world::ImportOptionType::Choice, parameters[2].type);
    EXPECT_EQ((QStringList{QStringLiteral("4"), QStringLiteral("6"), QStringLiteral("64")}),
              parameters[2].choices);

    EXPECT_EQ(QString("wall_thickness"), parameters[3].name);
    EXPECT_DOUBLE_EQ(0.01, parameters[3].step.toDouble());

    EXPECT_EQ(QString("enabled"), parameters[4].name);
    EXPECT_EQ(QString("Subdivisions"), parameters[4].group);
    EXPECT_EQ(world::ImportOptionType::Boolean, parameters[4].type);
    EXPECT_TRUE(parameters[4].defaultValue.toBool());
  }

  TEST(SourceAsset, StoresOpenScadEditableParametersInDefineOptions) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    qputenv("OPENSCAD_FAKE_LOG", dir.filePath("openscad.log").toLocal8Bit());
    auto source = writeScad(R"(
      /*<!!start test_model!!>*/
      sides = 6; //[4:square, 6:Hex, 64:circle]
      /*<!!end test_model!!>*/
      sphere(r = 1);
    )");

    SourceAsset asset;
    asset.setSourcePath(source->fileName());
    asset.setFormat("openscad");
    asset.setImportOptions(optionsFor(executable, cacheDirectory));
    asset.rebuildGeneratedChildren();

    EXPECT_EQ(QStringList({QStringLiteral("4"), QStringLiteral("6"), QStringLiteral("64")}),
              asset.propertyChoices("sides"));
    EXPECT_EQ(QStringLiteral("6"), asset.property("sides").toString());

    asset.setProperty("sides", "64");
    asset.propertyEdited("sides");

    const auto options = asset.importOptions();
    EXPECT_EQ(QStringLiteral("64"), options.value("define").toObject().value("sides").toString());
    EXPECT_FALSE(options.contains("parameters"));
    ASSERT_EQ(1, asset.childElements().size());
  }

  TEST(OpenScadSceneImporter, ImportsSupportedPrimitivesAndTransforms) {
    const auto file = writeScad(R"(
      translate([1, 2, 3]) rotate([0, 0, 90]) scale([2, 3, 4])
        cube([4, 6, 8], center=true);
      sphere(d=4, $fn=32);
      cylinder(h=5, r=2, center=false);
    )");
    const QString path = file->fileName();

    world::OpenScadSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(3, result.groupRoot()->childElements().size());

    auto* translate = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, translate);
    EXPECT_EQ(Vector3d(1, 2, 3), translate->position());

    auto* rotate = qobject_cast<Group*>(translate->childElements()[0]);
    ASSERT_NE(nullptr, rotate);
    EXPECT_NEAR(M_PI / 2.0, rotate->rotation().z(), 1.0e-9);

    auto* scale = qobject_cast<Group*>(rotate->childElements()[0]);
    ASSERT_NE(nullptr, scale);
    EXPECT_EQ(Vector3d(2, 3, 4), scale->scale());

    auto* cube = qobject_cast<Box*>(scale->childElements()[0]);
    ASSERT_NE(nullptr, cube);
    EXPECT_EQ(Vector3d(2, 3, 4), cube->size());
    EXPECT_EQ(Vector3d::null, cube->position());

    auto* sphere = qobject_cast<Sphere*>(result.groupRoot()->childElements()[1]);
    ASSERT_NE(nullptr, sphere);
    EXPECT_DOUBLE_EQ(2.0, sphere->radius());

    auto* cylinder = qobject_cast<Cylinder*>(result.groupRoot()->childElements()[2]);
    ASSERT_NE(nullptr, cylinder);
    EXPECT_DOUBLE_EQ(5.0, cylinder->height());
    EXPECT_DOUBLE_EQ(2.0, cylinder->radius());
    EXPECT_NEAR(M_PI / 2.0, cylinder->rotation().x(), 1.0e-9);
    EXPECT_EQ(Vector3d(0, 0, 2.5), cylinder->position());

    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isWarning());
    EXPECT_EQ(QString("Ignoring OpenSCAD display parameter '$fn'"),
              result.diagnostics()[0].message);
  }

  TEST(OpenScadSceneImporter, ImportsBooleansAsEditableCsg) {
    const auto file = writeScad(R"(
      union() {
        cube(2, center=true);
        difference() {
          sphere(r=1.2);
          translate([0, 0, 0.5]) cylinder(h=2, r=0.35, center=true);
        }
        intersection() {
          cube([1, 2, 3], center=true);
          sphere(1);
        }
      }
    )");
    const QString path = file->fileName();

    world::OpenScadSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(1, result.groupRoot()->childElements().size());

    auto* boolean = qobject_cast<Union*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, boolean);
    ASSERT_EQ(3, boolean->childElements().size());
    EXPECT_NE(nullptr, qobject_cast<Box*>(boolean->childElements()[0]));
    EXPECT_NE(nullptr, qobject_cast<Difference*>(boolean->childElements()[1]));
    EXPECT_NE(nullptr, qobject_cast<Intersection*>(boolean->childElements()[2]));
  }

  TEST(OpenScadSceneImporter, ConvertsSimpleBooleanFixtureToRuntimeCsg) {
    const auto file = writeScad(R"(
      difference() {
        cube([2, 2, 2], center=true);
        sphere(r=0.8);
      }
    )");
    const QString path = file->fileName();

    world::OpenScadSceneImporter importer;
    auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    render::Scene scene;
    auto primitive = result.groupRoot()->toRaytracer(&scene);

    EXPECT_TRUE(containsPrimitive<render::Difference>(primitive));
  }

  TEST(OpenScadSceneImporter, ReportsUnsupportedConstructsWithSourceLocation) {
    const auto file = writeScad("cube(1);\n  color([1, 0, 0]) sphere(1);\n");
    const QString path = file->fileName();

    world::OpenScadSceneImporter importer;
    const auto result = importer.importFile(path);

    EXPECT_TRUE(result.failed());
    ASSERT_FALSE(result.diagnostics().empty());
    const auto& diagnostic = result.diagnostics().front();
    EXPECT_TRUE(diagnostic.isError());
    EXPECT_EQ(QString("Unsupported OpenSCAD construct 'color'"), diagnostic.message);
    EXPECT_EQ(path, diagnostic.source);
    EXPECT_EQ(2, diagnostic.line);
    EXPECT_EQ(3, diagnostic.column);
  }

  TEST(OpenScadSceneImporter, ReportsMissingExecutableAsNonFatalDiagnostic) {
    world::OpenScadSceneImporter importer;

    const auto result = importer.importFile(
      sourceFixture(),
      world::ImportOptions(QJsonObject{{"executable", "/definitely/missing/openscad"}}));

    EXPECT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_TRUE(result.groupRoot()->childElements().empty());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isWarning());
    EXPECT_TRUE(result.diagnostics()[0].message.contains("OpenSCAD executable was not found"));
  }

  TEST(OpenScadCompiler, ReportsCrashedProcessAsFailure) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);

    world::OpenScadCompileRequest request;
    request.sourcePath = sourceFixture();
    request.executablePath = executable;
    request.cacheDirectory = dir.filePath("cache");
    request.outputFormat = "stl";
    request.options = optionsFor(executable, request.cacheDirectory);

    const CrashingOpenScadProcess process;
    const world::OpenScadCompiler compiler(&process);
    const auto result = compiler.compile(request);

    EXPECT_FALSE(result.succeeded);
    ASSERT_EQ(1u, result.diagnostics.size());
    EXPECT_TRUE(result.diagnostics[0].isError());
    EXPECT_TRUE(result.diagnostics[0].message.contains("OpenSCAD crashed or failed to start"));
    EXPECT_TRUE(result.diagnostics[0].message.contains("Abort trap: 6"));
    EXPECT_FALSE(QFileInfo::exists(result.outputPath));
  }

  TEST(OpenScadSceneImporter, CompilesScadFixtureIntoMeshPrimitive) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    qputenv("OPENSCAD_FAKE_LOG", dir.filePath("openscad.log").toLocal8Bit());

    world::OpenScadSceneImporter importer;
    const auto result = importer.importFile(
      sourceFixture(), world::ImportOptions(optionsFor(executable, cacheDirectory)));

    EXPECT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(1, result.groupRoot()->childElements().size());
    auto* compiled = qobject_cast<CompiledPrimitive*>(result.groupRoot()->childElements().front());
    ASSERT_NE(nullptr, compiled);
    auto primitive =
      std::dynamic_pointer_cast<render::MeshPrimitive>(compiled->toRaytracerPrimitive());
    ASSERT_NE(nullptr, primitive);
    ASSERT_NE(nullptr, primitive->mesh());
    EXPECT_EQ(3u, primitive->mesh()->vertices().size());
    EXPECT_EQ(1u, primitive->mesh()->faces().size());
    EXPECT_NE(nullptr, primitive->material());
    EXPECT_TRUE(QFileInfo::exists(result.source().properties["generatedOutputPath"].toString()));
    EXPECT_FALSE(result.source().properties["generatedOutputCacheKey"].toString().isEmpty());

    const auto rootProvenance = world::importProvenance(*result.groupRoot());
    ASSERT_TRUE(rootProvenance.has_value());
    EXPECT_EQ(sourceFixture(), rootProvenance->sourceFile);
    EXPECT_EQ(QString("root"), rootProvenance->sourceId);
    EXPECT_EQ(QString("generated-mesh"), rootProvenance->category["kind"].toString());

    const auto meshProvenance = world::importProvenance(*compiled);
    ASSERT_TRUE(meshProvenance.has_value());
    EXPECT_EQ(sourceFixture(), meshProvenance->sourceFile);
    EXPECT_EQ(QString("generated-output"), meshProvenance->sourceId);
    EXPECT_EQ(QString("stl"), meshProvenance->category["generatedOutputFormat"].toString());
  }

  TEST(OpenScadSceneImporter, ConfiguresStandaloneSceneForProductView) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    qputenv("OPENSCAD_FAKE_LOG", dir.filePath("openscad.log").toLocal8Bit());
    const world::ImportOptions options(optionsFor(executable, cacheDirectory));

    world::OpenScadSceneImporter importer;
    auto result = importer.importFile(sourceFixture(), options);

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    ASSERT_NE(nullptr, root);
    Element* importedRoot = root.get();
    Scene scene;
    scene.addChild(std::move(root));

    EXPECT_TRUE(importer.configureImportedScene(scene, *importedRoot, options));

    EXPECT_EQ(Colord::white(), scene.background());
    EXPECT_EQ(Colord(0.8, 0.8, 0.8), scene.ambient());
    auto* camera = qobject_cast<PinholeCamera*>(scene.activeCamera());
    ASSERT_NE(nullptr, camera);
    EXPECT_NEAR(camera->target().x(), camera->position().x(), 1e-9);
    EXPECT_NEAR(camera->target().y(), camera->position().y(), 1e-9);
    EXPECT_LT(camera->position().z(), camera->target().z());

    auto* group = qobject_cast<Group*>(importedRoot);
    ASSERT_NE(nullptr, group);
    EXPECT_NEAR(std::acos(-1.0) / 2.0, group->rotation().x(), 1e-9);
    const Vector3d mappedSourceUp = group->localTransform() * Vector4d(0, 0, 1);
    ASSERT_VECTOR_NEAR(Vector3d(0, -1, 0), mappedSourceUp, 1e-9);
    EXPECT_EQ(QString("openscad_z_up_to_product_view_up"),
              group->metadataValue("coordinateConversion").toString());

    const auto runtime = scene.toRaytracerScene();
    EXPECT_EQ(1u, runtime->lights().size());
    EXPECT_TRUE(runtime->boundingBox().isValid());
  }

  TEST(OpenScadSceneImporter, ConfiguresImportedRootWithoutChangingScene) {
    world::OpenScadSceneImporter importer;
    SourceAsset asset;
    Scene scene;
    const Colord originalBackground = scene.background();
    const Colord originalAmbient = scene.ambient();

    EXPECT_TRUE(importer.configureImportedRoot(asset, world::ImportOptions()));

    EXPECT_EQ(originalBackground, scene.background());
    EXPECT_EQ(originalAmbient, scene.ambient());
    EXPECT_EQ(nullptr, scene.activeCamera());
    EXPECT_NEAR(std::acos(-1.0) / 2.0, asset.rotation().x(), 1e-9);
    EXPECT_EQ(QString("openscad_z_up_to_product_view_up"),
              asset.metadataValue("coordinateConversion").toString());
  }

  TEST(OpenScadSceneImporter, SelectsPlyReaderForGeneratedMeshOutput) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    qputenv("OPENSCAD_FAKE_LOG", dir.filePath("openscad.log").toLocal8Bit());

    QJsonObject options = optionsFor(executable, cacheDirectory);
    options["outputFormat"] = "ply";

    world::OpenScadSceneImporter importer;
    const auto result = importer.importFile(sourceFixture(), world::ImportOptions(options));

    EXPECT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(1, result.groupRoot()->childElements().size());
    auto* compiled = qobject_cast<CompiledPrimitive*>(result.groupRoot()->childElements().front());
    ASSERT_NE(nullptr, compiled);
    auto primitive =
      std::dynamic_pointer_cast<render::MeshPrimitive>(compiled->toRaytracerPrimitive());
    ASSERT_NE(nullptr, primitive);
    ASSERT_NE(nullptr, primitive->mesh());
    EXPECT_EQ(3u, primitive->mesh()->vertices().size());
    EXPECT_EQ(1u, primitive->mesh()->faces().size());
    EXPECT_TRUE(result.source().properties["generatedOutputPath"].toString().endsWith(".ply"));
    const auto meshProvenance = world::importProvenance(*compiled);
    ASSERT_TRUE(meshProvenance.has_value());
    EXPECT_EQ(QString("ply"), meshProvenance->category["generatedOutputFormat"].toString());
  }

  TEST(OpenScadSceneImporter, CachesGeneratedOutputBySourceAndOptionsIdentity) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    const QString logPath = dir.filePath("openscad.log");
    qputenv("OPENSCAD_FAKE_LOG", logPath.toLocal8Bit());

    world::OpenScadSceneImporter importer;
    const auto first = importer.importFile(
      sourceFixture(), world::ImportOptions(optionsFor(executable, cacheDirectory)));
    const auto second = importer.importFile(
      sourceFixture(), world::ImportOptions(optionsFor(executable, cacheDirectory)));

    EXPECT_TRUE(first.succeeded());
    EXPECT_TRUE(second.succeeded());
    EXPECT_EQ(1, logRunCount(logPath));
    EXPECT_EQ(first.source().properties["generatedOutputCacheKey"].toString(),
              second.source().properties["generatedOutputCacheKey"].toString());
    ASSERT_EQ(1u, second.diagnostics().size());
    EXPECT_TRUE(second.diagnostics()[0].message.contains("Reused cached OpenSCAD mesh output"));

    QJsonObject changedOptions = optionsFor(executable, cacheDirectory);
    changedOptions["define"] = QJsonObject{{"teeth", 12}};
    const auto changed = importer.importFile(sourceFixture(), world::ImportOptions(changedOptions));

    EXPECT_TRUE(changed.succeeded());
    EXPECT_EQ(2, logRunCount(logPath));
    EXPECT_NE(first.source().properties["generatedOutputCacheKey"].toString(),
              changed.source().properties["generatedOutputCacheKey"].toString());
  }

  TEST(SourceAsset, StoresOpenScadGeneratedOutputCacheKeyAfterRebuild) {
    QTemporaryDir dir;
    const QString executable = writeExecutable(dir);
    const QString cacheDirectory = dir.filePath("cache");
    qputenv("OPENSCAD_FAKE_LOG", dir.filePath("openscad.log").toLocal8Bit());

    SourceAsset asset;
    asset.setSourcePath(sourceFixture());
    asset.setFormat("openscad");
    asset.setImportOptions(optionsFor(executable, cacheDirectory));

    asset.rebuildGeneratedChildren();

    EXPECT_TRUE(asset.diagnostics().empty());
    EXPECT_FALSE(asset.generatedOutputCacheKey().isEmpty());
    ASSERT_EQ(1, asset.childElements().size());
  }
}
