#include <gtest/gtest.h>

#include "render/primitives/Composite.h"
#include "render/primitives/Difference.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "world/import/OpenScadSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Box.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Difference.h"
#include "world/objects/Group.h"
#include "world/objects/Intersection.h"
#include "world/objects/Sphere.h"
#include "world/objects/Union.h"

#include <QTemporaryFile>

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
  }

  TEST(OpenScadSceneImporter, IsRegisteredForScadFiles) {
    auto importer = world::SceneImporterRegistry::self().createForFile("fixture.scad");

    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("openscad"), importer->name());
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
}
