#include <gtest/gtest.h>

#include "world/import/GltfSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Group.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryFile>

namespace GltfSceneImporterTest {
  namespace {
    QString writeGltf(const QByteArray& source) {
      QTemporaryFile file;
      file.setAutoRemove(false);
      EXPECT_TRUE(file.open());
      file.write(source);
      const QString path = file.fileName() + ".gltf";
      file.close();
      EXPECT_TRUE(QFile::rename(file.fileName(), path));
      return path;
    }

    void expectVectorNear(const Vector3d& actual, const Vector3d& expected) {
      EXPECT_NEAR(expected.x(), actual.x(), 0.0001);
      EXPECT_NEAR(expected.y(), actual.y(), 0.0001);
      EXPECT_NEAR(expected.z(), actual.z(), 0.0001);
    }
  }

  TEST(GltfSceneImporter, RegistersForGltfAndGlbFiles) {
    auto gltf = world::SceneImporterRegistry::self().createForFile("asset.gltf");
    auto glb = world::SceneImporterRegistry::self().createForFile("asset.glb");

    ASSERT_NE(nullptr, gltf);
    ASSERT_NE(nullptr, glb);
    EXPECT_EQ(QString("gltf"), gltf->name());
    EXPECT_EQ(QString("gltf"), glb->name());
  }

  TEST(GltfSceneImporter, ImportsNestedNamedNodesAsGroupHierarchyWithMetadata) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"name": "Main Scene", "nodes": [0]}],
      "nodes": [
        {"name": "Armature", "translation": [1, 2, 3], "children": [1]},
        {"name": "Hand", "translation": [4, 0, 0], "scale": [2, 2, 2]}
      ]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(1, result.groupRoot()->childElements().size());
    auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, scene);
    EXPECT_EQ(QString("Main Scene"), scene->name());
    EXPECT_EQ(QString("scenes/0"), scene->metadataValue(GroupMetadata::sourceIdKey()).toString());
    EXPECT_TRUE(scene->metadataValue("defaultScene").toBool());

    ASSERT_EQ(1, scene->childElements().size());
    auto* armature = qobject_cast<Group*>(scene->childElements()[0]);
    ASSERT_NE(nullptr, armature);
    EXPECT_EQ(QString("Armature"), armature->name());
    EXPECT_EQ(QString("nodes/0"), armature->metadataValue(GroupMetadata::sourceIdKey()).toString());
    EXPECT_EQ(QString("glTF"),
              armature->metadataValue(GroupMetadata::sourceFormatKey()).toString());
    expectVectorNear(armature->position(), Vector3d(1.0, 2.0, 3.0));

    const auto armatureProvenance = world::importProvenance(*armature);
    ASSERT_TRUE(armatureProvenance.has_value());
    EXPECT_EQ(path, armatureProvenance->sourceFile);
    EXPECT_EQ(QString("nodes/0"), armatureProvenance->sourceId);

    ASSERT_EQ(1, armature->childElements().size());
    auto* hand = qobject_cast<Group*>(armature->childElements()[0]);
    ASSERT_NE(nullptr, hand);
    EXPECT_EQ(QString("Hand"), hand->name());
    expectVectorNear(hand->position(), Vector3d(4.0, 0.0, 0.0));
    expectVectorNear(hand->globalTransform().translationVector(), Vector3d(5.0, 2.0, 3.0));
    expectVectorNear(hand->scale(), Vector3d(2.0, 2.0, 2.0));
  }

  TEST(GltfSceneImporter, ImportsMultipleScenesAsSceneGroups) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "scene": 1,
      "scenes": [
        {"name": "First", "nodes": [0]},
        {"name": "Second", "nodes": [1]}
      ],
      "nodes": [
        {"name": "First Root", "translation": [1, 0, 0]},
        {"name": "Second Root", "translation": [0, 2, 0]}
      ]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    ASSERT_EQ(2, result.groupRoot()->childElements().size());

    auto* first = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    auto* second = qobject_cast<Group*>(result.groupRoot()->childElements()[1]);
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(QString("First"), first->name());
    EXPECT_EQ(QString("Second"), second->name());
    EXPECT_FALSE(first->metadataValue("defaultScene").toBool());
    EXPECT_TRUE(second->metadataValue("defaultScene").toBool());
    ASSERT_EQ(1, first->childElements().size());
    ASSERT_EQ(1, second->childElements().size());
    EXPECT_EQ(QString("First Root"), first->childElements()[0]->name());
    EXPECT_EQ(QString("Second Root"), second->childElements()[0]->name());
  }

  TEST(GltfSceneImporter, CanFlattenNodeHierarchyWhenRequested) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "scenes": [{"nodes": [0]}],
      "nodes": [
        {"name": "Parent", "translation": [1, 0, 0], "children": [1]},
        {"name": "Child", "translation": [0, 2, 0]}
      ]
    })JSON");
    world::ImportOptions options;
    options.setValue("preserve_hierarchy", false);

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path, options);

    ASSERT_TRUE(result.succeeded());
    auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, scene);
    ASSERT_EQ(2, scene->childElements().size());
    auto* child = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, child);
    EXPECT_EQ(QString("Child"), child->name());
    expectVectorNear(child->position(), Vector3d(1.0, 2.0, 0.0));
  }

}
