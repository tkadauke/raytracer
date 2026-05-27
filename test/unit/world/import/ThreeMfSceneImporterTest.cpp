#include <gtest/gtest.h>

#include "render/materials/MatteMaterial.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/State.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "test/helpers/ImporterTestHelper.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/import/ThreeMfSceneImporter.h"
#include "world/objects/Group.h"

namespace ThreeMfSceneImporterTest {
  namespace {
    Group* childGroup(Group& root, int index) {
      auto* group = qobject_cast<Group*>(root.childElements()[index]);
      EXPECT_NE(nullptr, group);
      return group;
    }
  }

  TEST(ThreeMfSceneImporter, RegistersForThreeMfExtension) {
    auto importer = world::SceneImporterRegistry::self().createByFormat("3mf");

    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("3mf"), importer->name());
    EXPECT_TRUE(world::SceneImporterRegistry::self().hasExtension("3mf"));
  }

  TEST(ThreeMfSceneImporter, ImportsMinimalCorePackageAsTransformedMeshGroup) {
    const QString fixture = test::importers::importerFixturePath("3mf/minimal.3mf");

    const auto result = world::ThreeMfSceneImporter().importFile(fixture);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(QString("3mf"), result.source().importerName);

    Group* root = result.groupRoot();
    EXPECT_EQ(QString("minimal"), root->name());
    EXPECT_EQ(QString("3MF"), root->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString("millimeter"), root->metadataValue("originalUnits").toString());
    EXPECT_DOUBLE_EQ(0.001, root->metadataValue("unitScaleInMeters").toDouble());
    ASSERT_EQ(1, root->childElements().size());

    auto* item = childGroup(*root, 0);
    ASSERT_NE(nullptr, item);
    EXPECT_EQ(QString("red triangle"), item->name());
    EXPECT_EQ(7, item->metadataValue("objectId").toInt());
    EXPECT_EQ(0, item->metadataValue("buildIndex").toInt());
    ASSERT_EQ(1, item->childElements().size());
    EXPECT_TRUE(item->childElements().front()->isGenerated());

    render::Scene renderScene;
    const auto primitive = root->toRaytracer(&renderScene);
    ASSERT_NE(nullptr, primitive);
    const auto& bounds = primitive->boundingBox();
    EXPECT_NEAR(0.1, bounds.min().x(), 1e-9);
    EXPECT_NEAR(0.2, bounds.min().y(), 1e-9);
    EXPECT_NEAR(0.3, bounds.min().z(), 1e-9);
    EXPECT_NEAR(0.11, bounds.max().x(), 1e-9);
    EXPECT_NEAR(0.22, bounds.max().y(), 1e-9);
    EXPECT_NEAR(0.3, bounds.max().z(), 1e-9);

    render::State state;
    HitPointInterval hitPoints;
    const auto* hit = primitive->intersect(
      Rayd(Vector3d(0.101, 0.201, 1.0), Vector3d(0.0, 0.0, -1.0)), hitPoints, state);
    ASSERT_NE(nullptr, hit);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::MatteMaterial>(hit->material()));
  }

}
