#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"
#include "core/math/HitPoint.h"
#include "world/import/GltfSceneImporter.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/textures/ImageTexture.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Group.h"
#include "world/objects/OrthographicCamera.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"

#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <memory>

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

    std::shared_ptr<render::Material> importedMaterial(const world::ImportResult& result) {
      auto* meshPrimitive = importedMeshPrimitive(result);
      if (!meshPrimitive || meshPrimitive->leaves().empty())
        return nullptr;
      return meshPrimitive->leaves()[0]->material();
    }

    void expectColorNear(const Colord& actual, const Colord& expected) {
      EXPECT_NEAR(expected.r(), actual.r(), 0.0001);
      EXPECT_NEAR(expected.g(), actual.g(), 0.0001);
      EXPECT_NEAR(expected.b(), actual.b(), 0.0001);
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

    ASSERT_GE(scene->childElements().size(), 2);
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

  TEST(GltfSceneImporter, ConfiguresStandaloneSceneForProductView) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{
        "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
        "byteLength": 36
      }],
      "bufferViews": [{"buffer": 0, "byteLength": 36}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
      "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
      "scenes": [{"nodes": [0]}],
      "nodes": [{"name": "Triangle", "mesh": 0}]
    })JSON");

    world::GltfSceneImporter importer;
    auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    ASSERT_NE(nullptr, root);
    Element* importedRoot = root.get();
    Scene scene;
    scene.addChild(std::move(root));

    EXPECT_TRUE(importer.configureImportedScene(scene, *importedRoot, world::ImportOptions()));

    EXPECT_EQ(Colord::white(), scene.background());
    EXPECT_EQ(Colord(0.8, 0.8, 0.8), scene.ambient());
    auto* camera = qobject_cast<PinholeCamera*>(scene.activeCamera());
    ASSERT_NE(nullptr, camera);
    EXPECT_GT(camera->position().z(), camera->target().z());
    EXPECT_GT(camera->zoom(), 1.0);

    auto* group = qobject_cast<Group*>(importedRoot);
    ASSERT_NE(nullptr, group);
    EXPECT_NEAR(std::acos(-1.0), group->rotation().z(), 1e-9);
    const Vector3d mappedSourceUp = group->localTransform() * Vector4d(0, 1, 0);
    expectVectorNear(mappedSourceUp, Vector3d(0, -1, 0));
    EXPECT_EQ(QString("gltf_y_up_to_product_view_up"),
              group->metadataValue("coordinateConversion").toString());
    EXPECT_TRUE(scene.toRaytracerScene()->boundingBox().isValid());
  }

  TEST(GltfSceneImporter, ConfiguresImportedRootWithoutChangingScene) {
    world::GltfSceneImporter importer;
    Group group;
    Scene scene;
    const Colord originalBackground = scene.background();
    const Colord originalAmbient = scene.ambient();

    EXPECT_TRUE(importer.configureImportedRoot(group, world::ImportOptions()));

    EXPECT_EQ(originalBackground, scene.background());
    EXPECT_EQ(originalAmbient, scene.ambient());
    EXPECT_EQ(nullptr, scene.activeCamera());
    EXPECT_NEAR(std::acos(-1.0), group.rotation().z(), 1e-9);
    EXPECT_EQ(QString("gltf_y_up_to_product_view_up"),
              group.metadataValue("coordinateConversion").toString());
  }

  TEST(GltfSceneImporter, ConfiguresAnimatedSceneRootForProductView) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"name": "Animated Mesh Scene", "nodes": [0]}],
      "nodes": [{"name": "Animated Mesh Node", "mesh": 0}],
      "buffers": [
        {
          "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
          "byteLength": 36
        },
        {
          "uri": "data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAQAAAQEA=",
          "byteLength": 32
        }
      ],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 36},
        {"buffer": 1, "byteOffset": 0, "byteLength": 8},
        {"buffer": 1, "byteOffset": 8, "byteLength": 24}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": 1, "componentType": 5126, "count": 2, "type": "SCALAR"},
        {"bufferView": 2, "componentType": 5126, "count": 2, "type": "VEC3"}
      ],
      "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
      "animations": [{
        "samplers": [{"input": 1, "output": 2, "interpolation": "STEP"}],
        "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]
      }]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.sceneRoot());
    auto* scene = result.sceneRoot();
    EXPECT_EQ(Colord::white(), scene->background());
    EXPECT_EQ(Colord(0.8, 0.8, 0.8), scene->ambient());
    auto* camera = qobject_cast<PinholeCamera*>(scene->activeCamera());
    ASSERT_NE(nullptr, camera);
    EXPECT_GT(camera->position().z(), camera->target().z());
    EXPECT_GT(camera->zoom(), 1.0);

    ASSERT_GE(scene->childElements().size(), 2);
    auto* importRoot = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, importRoot);
    EXPECT_NEAR(std::acos(-1.0), importRoot->rotation().z(), 1e-9);
    EXPECT_EQ(QString("gltf_y_up_to_product_view_up"),
              importRoot->metadataValue("coordinateConversion").toString());
    EXPECT_TRUE(scene->toRaytracerScene()->boundingBox().isValid());
  }

  TEST(GltfSceneImporter, ImportsFixtureMeshMaterialTextureAndCamera) {
    world::GltfSceneImporter importer;
    const auto result = importer.importFile("test/fixtures/gltf/comprehensive_scene.gltf");

    ASSERT_TRUE(result.succeeded());
    ASSERT_FALSE(result.hasWarnings());
    ASSERT_NE(nullptr, result.groupRoot());
    auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, scene);
    auto* root = qobject_cast<Group*>(scene->childElements()[0]);
    ASSERT_NE(nullptr, root);
    auto* meshNode = qobject_cast<Group*>(root->childElements()[0]);
    ASSERT_NE(nullptr, meshNode);
    ASSERT_EQ(1, meshNode->childElements().size());

    auto* compiled = qobject_cast<CompiledPrimitive*>(meshNode->childElements()[0]);
    ASSERT_NE(nullptr, compiled);
    EXPECT_EQ(QString("Quad Mesh"), compiled->name());
    EXPECT_EQ(2, compiled->metadataValue("gltfTriangleCount").toInt());
    auto primitive =
      std::dynamic_pointer_cast<render::MeshPrimitive>(compiled->toRaytracerPrimitive());
    ASSERT_NE(nullptr, primitive);
    ASSERT_NE(nullptr, primitive->mesh());
    EXPECT_EQ(4u, primitive->mesh()->vertices().size());
    EXPECT_EQ(2u, primitive->mesh()->faces().size());
    ASSERT_EQ(2u, primitive->leaves().size());
    auto material =
      std::dynamic_pointer_cast<render::MatteMaterial>(primitive->leaves().front()->material());
    ASSERT_NE(nullptr, material);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::ImageTexture>(material->diffuseTexture()));

    auto* cameraNode = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, cameraNode);
    ASSERT_EQ(1, cameraNode->childElements().size());
    EXPECT_NE(nullptr, qobject_cast<PinholeCamera*>(cameraNode->childElements()[0]));
  }

  TEST(GltfSceneImporter, ImportsPerspectiveAndOrthographicCamerasFromNodes) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "scenes": [{"nodes": [0, 1]}],
      "cameras": [
        {"name": "Hero Perspective", "type": "perspective",
         "perspective": {"yfov": 0.927295218, "aspectRatio": 1.5, "znear": 0.1, "zfar": 200.0}},
        {"name": "Plan Orthographic", "type": "orthographic",
         "orthographic": {"xmag": 8.0, "ymag": 4.0, "znear": 0.5, "zfar": 50.0}}
      ],
      "nodes": [
        {"name": "Perspective Node", "camera": 0, "translation": [1, 2, 3]},
        {"name": "Orthographic Node", "camera": 1, "translation": [4, 5, 6]}
      ]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, scene);
    ASSERT_EQ(2, scene->childElements().size());

    auto* perspectiveNode = qobject_cast<Group*>(scene->childElements()[0]);
    ASSERT_NE(nullptr, perspectiveNode);
    ASSERT_EQ(1, perspectiveNode->childElements().size());
    auto* perspective = qobject_cast<PinholeCamera*>(perspectiveNode->childElements()[0]);
    ASSERT_NE(nullptr, perspective);
    EXPECT_EQ(QString("Hero Perspective"), perspective->name());
    expectVectorNear(perspective->position(), Vector3d(1.0, 2.0, 3.0));
    expectVectorNear(perspective->target(), Vector3d(1.0, 2.0, 2.0));
    EXPECT_NEAR(1.2, perspective->zoom(), 0.0001);
    EXPECT_EQ(QString("perspective"), perspective->metadataValue("gltfCameraType").toString());
    EXPECT_EQ(1.5, perspective->metadataValue("gltfAspectRatio").toDouble());

    auto* orthographicNode = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, orthographicNode);
    ASSERT_EQ(1, orthographicNode->childElements().size());
    auto* orthographic = qobject_cast<OrthographicCamera*>(orthographicNode->childElements()[0]);
    ASSERT_NE(nullptr, orthographic);
    EXPECT_EQ(QString("Plan Orthographic"), orthographic->name());
    expectVectorNear(orthographic->position(), Vector3d(4.0, 5.0, 6.0));
    expectVectorNear(orthographic->target(), Vector3d(4.0, 5.0, 5.0));
    EXPECT_NEAR(1.5, orthographic->zoom(), 0.0001);
    EXPECT_EQ(8.0, orthographic->metadataValue("gltfXmag").toDouble());
  }

  TEST(GltfSceneImporter, ImportsSupportedPunctualLightsAndWarnsForUnsupportedData) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "extensions": {
        "KHR_lights_punctual": {
          "lights": [
            {"name": "Sun", "type": "directional", "color": [1.0, 0.8, 0.6], "intensity": 2.5},
            {"name": "Bulb", "type": "point", "range": 10.0},
            {"name": "Cone", "type": "spot", "spot": {"outerConeAngle": 0.5}}
          ]
        }
      },
      "scenes": [{"nodes": [0, 1, 2]}],
      "nodes": [
        {"name": "Sun Node", "extensions": {"KHR_lights_punctual": {"light": 0}}},
        {"name": "Bulb Node", "translation": [1, 2, 3],
         "extensions": {"KHR_lights_punctual": {"light": 1}}},
        {"name": "Cone Node", "extensions": {"KHR_lights_punctual": {"light": 2}}}
      ]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto* scene = qobject_cast<Group*>(result.groupRoot()->childElements()[0]);
    ASSERT_NE(nullptr, scene);

    auto* sunNode = qobject_cast<Group*>(scene->childElements()[0]);
    ASSERT_NE(nullptr, sunNode);
    ASSERT_EQ(1, sunNode->childElements().size());
    auto* sun = qobject_cast<DirectionalLight*>(sunNode->childElements()[0]);
    ASSERT_NE(nullptr, sun);
    EXPECT_EQ(QString("Sun"), sun->name());
    EXPECT_EQ(Colord(1.0, 0.8, 0.6), sun->color());
    EXPECT_EQ(2.5, sun->intensity());
    expectVectorNear(sun->direction(), Vector3d(0.0, 0.0, -1.0));

    auto* bulbNode = qobject_cast<Group*>(scene->childElements()[1]);
    ASSERT_NE(nullptr, bulbNode);
    ASSERT_EQ(1, bulbNode->childElements().size());
    auto* bulb = qobject_cast<PointLight*>(bulbNode->childElements()[0]);
    ASSERT_NE(nullptr, bulb);
    EXPECT_EQ(QString("Bulb"), bulb->name());
    EXPECT_EQ(10.0, bulb->metadataValue("gltfRange").toDouble());

    auto* coneNode = qobject_cast<Group*>(scene->childElements()[2]);
    ASSERT_NE(nullptr, coneNode);
    EXPECT_TRUE(coneNode->childElements().empty());
    ASSERT_EQ(2u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isWarning());
    EXPECT_TRUE(result.diagnostics()[0].message.contains("range"));
    EXPECT_TRUE(result.diagnostics()[1].message.contains("spot light"));
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

  TEST(GltfSceneImporter, MapsPbrBaseColorFactorToMatteDiffuseTexture) {
    const QString path = writeGltf(R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{
        "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
        "byteLength": 36
      }],
      "bufferViews": [{"buffer": 0, "byteLength": 36}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
      "materials": [{
        "pbrMetallicRoughness": {
          "baseColorFactor": [0.2, 0.4, 0.6, 1],
          "metallicFactor": 0.5,
          "roughnessFactor": 0.25
        },
        "alphaMode": "MASK",
        "doubleSided": true
      }],
      "meshes": [{
        "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
      }],
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}]
    })JSON");

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(path);

    ASSERT_TRUE(result.succeeded());
    auto material = std::dynamic_pointer_cast<render::MatteMaterial>(importedMaterial(result));
    ASSERT_NE(nullptr, material);
    ASSERT_NE(nullptr, material->diffuseTexture());
    expectColorNear(material->diffuseTexture()->evaluate(Rayd::undefined, HitPoint::undefined()),
                    Colord(0.2, 0.4, 0.6));
    EXPECT_TRUE(result.hasWarnings());
  }

  TEST(GltfSceneImporter, MapsExternalBaseColorTextureResolvedBesideGltfFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    QImage image(2, 2, QImage::Format_RGBA8888);
    image.fill(Qt::black);
    image.setPixelColor(0, 0, QColor(255, 0, 0));
    image.setPixelColor(1, 0, QColor(0, 255, 0));
    image.setPixelColor(0, 1, QColor(0, 0, 255));
    image.setPixelColor(1, 1, QColor(255, 255, 255));
    ASSERT_TRUE(image.save(directory.filePath("base_color.png")));

    QFile file(directory.filePath("textured.gltf"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(R"JSON({
      "asset": {"version": "2.0"},
      "buffers": [{
        "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAA",
        "byteLength": 36
      }],
      "bufferViews": [{"buffer": 0, "byteLength": 36}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
      "images": [{"uri": "base_color.png", "mimeType": "image/png"}],
      "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
      "textures": [{"sampler": 0, "source": 0}],
      "materials": [{
        "pbrMetallicRoughness": {
          "baseColorFactor": [1, 1, 1, 1],
          "baseColorTexture": {"index": 0}
        }
      }],
      "meshes": [{
        "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
      }],
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}]
    })JSON");
    file.close();

    world::GltfSceneImporter importer;
    const auto result = importer.importFile(file.fileName());

    ASSERT_TRUE(result.succeeded())
      << (result.diagnostics().empty() ? std::string()
                                       : result.diagnostics().front().message.toStdString());
    auto material = std::dynamic_pointer_cast<render::MatteMaterial>(importedMaterial(result));
    ASSERT_NE(nullptr, material);
    auto texture = std::dynamic_pointer_cast<render::ImageTexture>(material->diffuseTexture());
    ASSERT_NE(nullptr, texture);
    EXPECT_EQ(render::ImageTextureFilter::Nearest, texture->filter());
    EXPECT_EQ(render::ImageTextureWrap::Clamp, texture->wrap());
    expectColorNear(texture->sample(0.1, 0.1), Colord::red());
    expectColorNear(texture->sample(0.75, 0.75), Colord::white());
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
