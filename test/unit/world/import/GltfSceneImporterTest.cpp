#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "render/primitives/MeshPrimitive.h"
#include "world/import/GltfSceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <QFile>
#include <QJsonArray>
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

    void expectVectorNear(const Vector2d& actual, const Vector2d& expected) {
      EXPECT_NEAR(expected.x(), actual.x(), 0.0001);
      EXPECT_NEAR(expected.y(), actual.y(), 0.0001);
    }

    render::MeshPrimitive* importedMeshPrimitive(const world::ImportResult& result) {
      auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
      auto* node = qobject_cast<Group*>(scene->childElements()[0]);
      auto* compiled = qobject_cast<CompiledPrimitive*>(node->childElements()[0]);
      if (!compiled)
        return nullptr;
      return dynamic_cast<render::MeshPrimitive*>(compiled->toRaytracerPrimitive().get());
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

  TEST(GltfSceneImporter, ImportsAnimatedNodeMetadataAndTimelineTracks) {
    world::GltfSceneImporter importer;
    const auto result = importer.importFile("test/fixtures/gltf/animated_node.gltf");

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.hasWarnings());
    ASSERT_NE(nullptr, result.sceneRoot());
    auto* scene = result.sceneRoot();
    ASSERT_TRUE(scene->hasAnimation());
    ASSERT_NE(nullptr, scene->animation());
    EXPECT_EQ(0, scene->animation()->startFrame());
    EXPECT_EQ(24, scene->animation()->endFrame());
    ASSERT_EQ(1u, scene->animation()->tracks().size());
    EXPECT_EQ(QString("position"), scene->animation()->tracks().front().propertyName());

    ASSERT_EQ(2, scene->childElements().size());
    auto* importRoot = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, importRoot);
    auto* sceneGroup = qobject_cast<Group*>(importRoot->childElements()[0]);
    ASSERT_NE(nullptr, sceneGroup);
    auto* animatedNode = qobject_cast<Group*>(sceneGroup->childElements()[0]);
    ASSERT_NE(nullptr, animatedNode);
    EXPECT_EQ(QString("Animated Node"), animatedNode->name());
    EXPECT_EQ(2, animatedNode->metadataValue("gltfAnimationChannelCount").toInt());

    const auto channels = animatedNode->metadataValue("gltfAnimationChannels").toArray();
    ASSERT_EQ(2, channels.size());
    EXPECT_TRUE(channels[0].toObject()["represented"].toBool());
    EXPECT_EQ(QString("position"), channels[0].toObject()["worldProperty"].toString());
    EXPECT_FALSE(channels[1].toObject()["represented"].toBool());
    EXPECT_EQ(QString("unsupported target path"), channels[1].toObject()["reason"].toString());

    scene->evaluateAnimationAtFrame(24);
    expectVectorNear(animatedNode->position(), Vector3d(1.0, 2.0, 3.0));
  }

  TEST(GltfSceneImporter, ImportsIndexedTrianglePrimitiveAttributesAndMaterialReference) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{
        "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAgD8AAIA/AAAAAAAAgD8AAAEAAgAAAAIAAwA=",
        "byteLength": 140
      }],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 48},
        {"buffer": 0, "byteOffset": 48, "byteLength": 48},
        {"buffer": 0, "byteOffset": 96, "byteLength": 32},
        {"buffer": 0, "byteOffset": 128, "byteLength": 12}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
        {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
        {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"}
      ],
      "materials": [{
        "name": "Red",
        "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1]}
      }],
      "meshes": [{
        "name": "Quad",
        "primitives": [{
          "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
          "indices": 3,
          "material": 0
        }]
      }],
      "scenes": [{"nodes": [0]}],
      "nodes": [{"name": "Mesh Node", "mesh": 0}]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto* meshPrimitive = importedMeshPrimitive(result);
    ASSERT_NE(nullptr, meshPrimitive);
    ASSERT_NE(nullptr, meshPrimitive->asset());
    ASSERT_EQ(4u, meshPrimitive->mesh()->vertices().size());
    ASSERT_EQ(2u, meshPrimitive->mesh()->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), meshPrimitive->mesh()->faces()[0]);
    EXPECT_EQ((Mesh::Face{0, 2, 3}), meshPrimitive->mesh()->faces()[1]);
    expectVectorNear(meshPrimitive->mesh()->vertices()[2].point, Vector3d(1.0, 1.0, 0.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[2].normal, Vector3d(0.0, 0.0, 1.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[2].uv, Vector2d(1.0, 1.0));
    ASSERT_EQ(2u, meshPrimitive->leaves().size());
    EXPECT_NE(nullptr, meshPrimitive->leaves()[0]->material());
    EXPECT_NE(nullptr, meshPrimitive->leaves()[1]->material());
  }

  TEST(GltfSceneImporter, ImportsNonIndexedTrianglesWithDeterministicFallbackAttributes) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{
        "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
        "byteLength": 36
      }],
      "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}
      ],
      "meshes": [{
        "primitives": [{
          "attributes": {"POSITION": 0}
        }]
      }],
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto* meshPrimitive = importedMeshPrimitive(result);
    ASSERT_NE(nullptr, meshPrimitive);
    ASSERT_EQ(3u, meshPrimitive->mesh()->vertices().size());
    ASSERT_EQ(1u, meshPrimitive->mesh()->faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), meshPrimitive->mesh()->faces()[0]);
    expectVectorNear(meshPrimitive->mesh()->vertices()[0].point, Vector3d(0.0, 0.0, 0.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[1].point, Vector3d(0.0, 1.0, 0.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[2].point, Vector3d(1.0, 0.0, 0.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[0].uv, Vector2d(0.0, 0.0));
    expectVectorNear(meshPrimitive->mesh()->vertices()[0].normal, Vector3d(0.0, 0.0, 1.0));
    ASSERT_EQ(1u, meshPrimitive->leaves().size());
  }

}
