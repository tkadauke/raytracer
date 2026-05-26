#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Group.h"
#include "world/objects/Sphere.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"
#include "world/animation/AnimationTrack.h"
#include "world/animation/Timeline.h"
#include "render/primitives/Scene.h"
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryFile>
#include <QUuid>

// Q_DECLARE_METATYPE expands to a per-TU QMetaTypeId<T> specialisation;
// each TU that calls qRegisterMetaType<T> needs to see it. The library
// declares these in Element.cpp; mirror them here so the test TU's
// MetaTypeRegistrar below compiles.
Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace SceneTest {
  using ::testing::HasSubstr;

  // Register the custom metatypes the world::* Q_PROPERTYs use. Without
  // this, QMetaProperty::read silently returns an invalid QVariant for
  // Vector3d/Colord/Angled/Material*/Texture* properties, so JSON
  // roundtrip drops them. The production GUI app and rendercli register
  // these at startup (see tools/rendercli/rendercli.cpp ~L224); the test
  // binary doesn't link those entry points, so we mirror the calls here.
  // TODO: lift this into a library-level Init() so apps and tests don't
  // each redo it (latent app/library coupling — see #24 follow-up).
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Vector3d>();
      qRegisterMetaType<Angled>();
      qRegisterMetaType<Colord>();
      qRegisterMetaType<Material*>();
      qRegisterMetaType<Texture*>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  QJsonValue vectorValue(double x, double y, double z) {
    return QJsonValue(QJsonArray({x, y, z}));
  }

  QJsonValue colorValue(double r, double g, double b) {
    return QJsonValue(QJsonArray({r, g, b}));
  }

  TEST(Scene, ShouldDefaultToCannedNewSceneName) {
    Scene scene;
    EXPECT_EQ(QString("New Scene"), scene.name());
  }

  TEST(Scene, ShouldDefaultToWarmAmbient) {
    // (0.4, 0.4, 0.4) is the canned default the constructor sets — bright
    // enough that an unlit scene isn't pitch-black during scene editing.
    Scene scene;
    EXPECT_EQ(Colord(0.4, 0.4, 0.4), scene.ambient());
  }

  TEST(Scene, ShouldDefaultToSkyBlueBackground) {
    Scene scene;
    EXPECT_EQ(Colord(0.4, 0.8, 1), scene.background());
  }

  TEST(Scene, ShouldDefaultToUnchanged) {
    Scene scene;
    EXPECT_FALSE(scene.changed());
  }

  TEST(Scene, ShouldDefaultToNoAnimation) {
    Scene scene;
    EXPECT_FALSE(scene.hasAnimation());
    EXPECT_EQ(nullptr, scene.animation());
  }

  TEST(Scene, ShouldDefaultToNoExplicitRenderIntent) {
    Scene scene;
    EXPECT_FALSE(scene.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Raytracer,
              scene.renderIntent().defaultExecutor);
    EXPECT_FALSE(scene.renderIntent().enableWireframeOverlay);
  }

  TEST(Scene, ShouldSetAndGetAmbient) {
    Scene scene;
    scene.setAmbient(Colord(0.1, 0.2, 0.3));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), scene.ambient());
  }

  TEST(Scene, ShouldSetAndGetBackground) {
    Scene scene;
    scene.setBackground(Colord(0.5, 0.6, 0.7));
    EXPECT_EQ(Colord(0.5, 0.6, 0.7), scene.background());
  }

  TEST(Scene, ShouldSetAndGetChanged) {
    Scene scene;
    scene.setChanged(true);
    EXPECT_TRUE(scene.changed());
  }

  TEST(Scene, ShouldSetAndGetAnimation) {
    Scene scene;
    scene.setAnimation(std::make_unique<world::Timeline>(1, 10, 24.0));

    ASSERT_TRUE(scene.hasAnimation());
    ASSERT_NE(nullptr, scene.animation());
    EXPECT_EQ(1, scene.animation()->startFrame());
    EXPECT_EQ(10, scene.animation()->endFrame());
    EXPECT_DOUBLE_EQ(24.0, scene.animation()->fps());
  }

  TEST(Scene, ShouldSetAndGetRenderIntent) {
    Scene scene;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = engine::graph::RenderViewMode::Beauty;
    intent.enableWireframeOverlay = true;

    scene.setRenderIntent(intent);

    EXPECT_TRUE(scene.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Rasterizer,
              scene.renderIntent().defaultExecutor);
    EXPECT_TRUE(scene.renderIntent().enableWireframeOverlay);

    scene.clearRenderIntent();
    EXPECT_FALSE(scene.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Raytracer,
              scene.renderIntent().defaultExecutor);
  }

  TEST(Scene, ShouldAcceptAnyChild) {
    // Scene::canHaveChild returns true unconditionally — it's the root of
    // the editable hierarchy and has to take cameras, lights, surfaces,
    // textures, materials. The bare Element child here is the lightest
    // proof that the predicate doesn't discriminate.
    Scene scene;
    Element child;
    EXPECT_TRUE(scene.canHaveChild(&child));
  }

  TEST(Scene, ShouldReturnNullActiveCameraWhenEmpty) {
    Scene scene;
    EXPECT_EQ(nullptr, scene.activeCamera());
  }

  TEST(Scene, ShouldReturnSingleCameraAsActive) {
    Scene scene;
    auto* camera = new PinholeCamera;
    scene.addChild(camera);
    EXPECT_EQ(camera, scene.activeCamera());
  }

  TEST(Scene, ShouldReturnActiveCameraRenderGraphReference) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("shot-camera");
    scene.addChild(camera);

    const auto cameraRef = scene.activeRenderCameraRef();

    ASSERT_TRUE(cameraRef.has_value());
    ASSERT_TRUE(cameraRef->sceneCameraId.has_value());
    EXPECT_EQ("shot-camera", *cameraRef->sceneCameraId);
  }

  TEST(Scene, ShouldFrameActivePinholeCameraToContents) {
    Scene scene;
    auto* camera = new PinholeCamera;
    scene.addChild(camera);
    auto* sphere = new Sphere;
    sphere->setPosition(Vector3d(40.0, 2.0, -6.0));
    sphere->setRadius(3.0);
    scene.addChild(sphere);

    ASSERT_TRUE(scene.frameActivePinholeCameraToContents(Vector3d(0.0, 0.0, -1.0)));

    EXPECT_NEAR(40.0, camera->target().x(), 1e-9);
    EXPECT_NEAR(2.0, camera->target().y(), 1e-9);
    EXPECT_NEAR(-6.0, camera->target().z(), 1e-9);
    EXPECT_LT(camera->position().z(), camera->target().z());
  }

  TEST(Scene, ShouldReportWhenNoActivePinholeCameraCanBeFramed) {
    Scene scene;
    scene.addChild(new Sphere);

    EXPECT_FALSE(scene.frameActivePinholeCameraToContents(Vector3d(0.0, 0.0, -1.0)));
  }

  TEST(Scene, ShouldApplyActiveCameraToRenderIntentWhenMissingCamera) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("shot-camera");
    scene.addChild(camera);

    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    scene.setRenderIntent(intent);

    const auto effectiveIntent = scene.renderIntentWithActiveCameraDefault();

    EXPECT_EQ(engine::graph::RenderExecutorPreference::Rasterizer, effectiveIntent.defaultExecutor);
    ASSERT_TRUE(effectiveIntent.defaultCamera.has_value());
    ASSERT_TRUE(effectiveIntent.defaultCamera->sceneCameraId.has_value());
    EXPECT_EQ("shot-camera", *effectiveIntent.defaultCamera->sceneCameraId);
  }

  TEST(Scene, ShouldAnalyzeVisibleRenderGraphSceneFeatures) {
    Scene scene;
    scene.addChild(new Sphere);
    scene.addChild(new PointLight);

    const auto analysis = scene.renderGraphAnalysis();

    EXPECT_TRUE(analysis.hasKnownVisibleSurfaceCount());
    EXPECT_TRUE(analysis.hasKnownVisibleLightCount());
    EXPECT_EQ(1u, analysis.visibleSurfaceCount());
    EXPECT_EQ(1u, analysis.visibleLightCount());
    EXPECT_TRUE(analysis.hasVisibleSurfaces());
    EXPECT_TRUE(analysis.hasVisibleLights());
  }

  TEST(Scene, ShouldIgnoreHiddenElementsInRenderGraphAnalysis) {
    Scene scene;
    auto* hiddenSphere = new Sphere;
    hiddenSphere->hide();
    scene.addChild(hiddenSphere);
    auto* hiddenLight = new PointLight;
    hiddenLight->hide();
    scene.addChild(hiddenLight);

    const auto analysis = scene.renderGraphAnalysis();

    EXPECT_EQ(0u, analysis.visibleSurfaceCount());
    EXPECT_EQ(0u, analysis.visibleLightCount());
    EXPECT_FALSE(analysis.hasVisibleSurfaces());
    EXPECT_FALSE(analysis.hasVisibleLights());
  }

  TEST(Scene, ShouldRespectHiddenGroupsInRenderGraphAnalysis) {
    Scene scene;
    auto* group = new Group;
    group->hide();
    group->addChild(new Sphere);
    group->addChild(new PointLight);
    scene.addChild(group);

    const auto analysis = scene.renderGraphAnalysis();

    EXPECT_EQ(0u, analysis.visibleSurfaceCount());
    EXPECT_EQ(0u, analysis.visibleLightCount());
  }

  TEST(Scene, ShouldReturnLastCameraAsActiveWhenMultiple) {
    // activeCamera() walks all children and keeps the last camera it sees,
    // so the most-recently-added camera wins. Documenting that here so a
    // future change that picks the *first* camera (e.g. for stability) is
    // a deliberate behaviour change rather than an accident.
    Scene scene;
    auto* first = new PinholeCamera;
    auto* second = new PinholeCamera;
    scene.addChild(first);
    scene.addChild(second);
    EXPECT_EQ(second, scene.activeCamera());
  }

  TEST(Scene, ShouldIgnoreNonCameraChildrenInActiveCamera) {
    Scene scene;
    auto* nonCamera = new Element;
    scene.addChild(nonCamera);
    EXPECT_EQ(nullptr, scene.activeCamera());
  }

  TEST(Scene, ShouldRoundtripAmbientAndBackgroundViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    original.setAmbient(Colord(0.1, 0.2, 0.3));
    original.setBackground(Colord(0.4, 0.5, 0.6));
    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), decoded.ambient());
    EXPECT_EQ(Colord(0.4, 0.5, 0.6), decoded.background());

    QFile::remove(path);
  }

  TEST(Scene, ShouldClearChangedFlagOnSave) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene scene;
    scene.setChanged(true);
    ASSERT_TRUE(scene.save(path));
    EXPECT_FALSE(scene.changed());

    QFile::remove(path);
  }

  TEST(Scene, ShouldFailLoadOnMissingFile) {
    Scene scene;
    auto path = QDir::tempPath() + "/raytracer-scene-missing-" +
                QUuid::createUuid().toString(QUuid::WithoutBraces);
    EXPECT_FALSE(scene.load(path));
  }

  TEST(Scene, ShouldFailLoadOnNonObjectJson) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    ASSERT_EQ(temp.write("[]"), 2);
    temp.close();

    Scene scene;
    EXPECT_FALSE(scene.load(path));
  }

  TEST(Scene, ShouldRejectRenderGraphPlanJson) {
    Scene scene;
    QJsonObject json;
    json["resources"] = QJsonArray{};
    json["passes"] = QJsonArray{};

    try {
      scene.read(json);
      FAIL() << "expected render graph plan JSON to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("render graph plan JSON cannot be opened as a scene"));
      EXPECT_THAT(error.what(), HasSubstr("rendercli --render_graph_in"));
    }
  }

  TEST(Scene, ShouldFailSaveOnUnwritablePath) {
    Scene scene;
    // / is non-writable on all reasonable test environments (and the
    // file's name guarantees no collision); save returns false rather
    // than throwing.
    EXPECT_FALSE(scene.save("/raytracer-scene-unwritable-path.json"));
  }

  TEST(Scene, ShouldWriteValidJsonFile) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene scene;
    ASSERT_TRUE(scene.save(path));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    auto bytes = file.readAll();
    file.close();

    auto doc = QJsonDocument::fromJson(bytes);
    ASSERT_FALSE(doc.isNull());
    ASSERT_TRUE(doc.isObject());
    EXPECT_EQ(QString("Scene"), doc.object()["type"].toString());

    QFile::remove(path);
  }

  TEST(Scene, ShouldRoundtripChildrenViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    auto* camera = new PinholeCamera;
    original.addChild(camera);
    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_NE(nullptr, decoded.activeCamera());
    EXPECT_NE(nullptr, qobject_cast<PinholeCamera*>(decoded.activeCamera()));

    QFile::remove(path);
  }

  TEST(Scene, ShouldLoadImportedSceneSources) {
    QTemporaryFile imported("raytracer-imported-XXXXXX.rtjson");
    ASSERT_TRUE(imported.open());
    const QString importedPath = imported.fileName();
    imported.write(R"({
      "id": "imported-scene",
      "name": "Imported",
      "type": "Scene",
      "children": [
        {
          "id": "camera",
          "name": "Imported Camera",
          "position": [0.0, 0.0, -3.0],
          "target": [0.0, 0.0, 0.0],
          "distance": 5.0,
          "zoom": 1.0,
          "type": "PinholeCamera",
          "children": []
        }
      ]
    })");
    imported.close();

    QTemporaryFile sceneFile("raytracer-scene-XXXXXX.json");
    ASSERT_TRUE(sceneFile.open());
    const QString scenePath = sceneFile.fileName();
    const QByteArray sceneJson = QByteArray(R"({
        "id": "scene",
        "name": "Scene With Import",
        "type": "Scene",
        "imports": [
          {
            "source": ")") + importedPath.toUtf8() +
                                 QByteArray(R"(",
            "format": "json",
            "options": {"fixture": "minimal"}
          }
        ],
        "children": []
      })");
    sceneFile.write(sceneJson);
    sceneFile.close();

    Scene scene;
    ASSERT_TRUE(scene.load(scenePath));

    ASSERT_NE(nullptr, scene.activeCamera());
    EXPECT_EQ(QString("Imported Camera"), scene.activeCamera()->name());
  }

  TEST(Scene, ShouldRejectUnknownImportedSceneFormat) {
    Scene scene;
    QJsonObject json;
    scene.write(json);
    json["imports"] = QJsonArray({QJsonObject(
      {{"source", "fixture.unknown"}, {"format", "unknown"}, {"options", QJsonObject()}})});

    try {
      scene.read(json);
      FAIL() << "expected unknown import format to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("No scene importer registered"));
    }
  }

  TEST(Scene, ShouldRoundtripAnimationViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    auto* camera = new PinholeCamera;
    camera->setId("camera-id");
    original.addChild(camera);
    original.setAnimation(std::make_unique<world::Timeline>(
      1, 10, 24.0,
      std::vector<world::AnimationTrack>({world::AnimationTrack("camera-id", "position",
                                                                {
                                                                  {1, vectorValue(0.0, 0.0, 0.0)},
                                                                  {10, vectorValue(9.0, 0.0, 0.0)},
                                                                })})));

    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    ASSERT_TRUE(decoded.hasAnimation());
    ASSERT_NE(nullptr, decoded.animation());
    EXPECT_EQ(1, decoded.animation()->startFrame());
    EXPECT_EQ(10, decoded.animation()->endFrame());
    EXPECT_DOUBLE_EQ(24.0, decoded.animation()->fps());
    ASSERT_EQ(1u, decoded.animation()->tracks().size());

    const auto& track = decoded.animation()->tracks().front();
    EXPECT_EQ(QString("camera-id"), track.targetId());
    EXPECT_EQ(QString("position"), track.propertyName());
    ASSERT_EQ(2u, track.keyframes().size());
    EXPECT_EQ(10, track.keyframes()[1].frame);
    EXPECT_DOUBLE_EQ(9.0, track.keyframes()[1].value.toArray()[0].toDouble());

    QFile::remove(path);
  }

  TEST(Scene, ShouldRoundtripRenderIntentViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = engine::graph::RenderViewMode::Beauty;
    intent.defaultShadingProfile.name = "toon";
    intent.enableWireframeOverlay = true;
    engine::graph::RenderViewOverride override;
    override.selector = engine::graph::SceneSelector::objectName("Rig");
    override.executor = engine::graph::RenderExecutorPreference::Wireframe;
    override.viewMode = engine::graph::RenderViewMode::Wireframe;
    intent.viewOverrides.push_back(override);
    original.setRenderIntent(intent);

    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    ASSERT_TRUE(decoded.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Rasterizer,
              decoded.renderIntent().defaultExecutor);
    EXPECT_EQ(QString("toon"),
              QString::fromStdString(decoded.renderIntent().defaultShadingProfile.name));
    EXPECT_TRUE(decoded.renderIntent().enableWireframeOverlay);
    ASSERT_EQ(1u, decoded.renderIntent().viewOverrides.size());
    EXPECT_EQ(engine::graph::SceneSelector::Kind::ObjectName,
              decoded.renderIntent().viewOverrides.front().selector.kind);
    EXPECT_EQ(QString("Rig"),
              QString::fromStdString(decoded.renderIntent().viewOverrides.front().selector.value));
    ASSERT_TRUE(decoded.renderIntent().viewOverrides.front().executor.has_value());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Wireframe,
              *decoded.renderIntent().viewOverrides.front().executor);

    QFile::remove(path);
  }

  TEST(Scene, ShouldEvaluateAnimationAtFrame) {
    Scene scene;
    scene.setId("scene-id");
    scene.setBackground(Colord(0.0, 0.0, 0.0));
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({world::AnimationTrack("scene-id", "background",
                                                                {
                                                                  {1, colorValue(0.0, 0.0, 0.0)},
                                                                  {11, colorValue(1.0, 0.5, 0.0)},
                                                                })})));

    scene.evaluateAnimationAtFrame(6);

    EXPECT_EQ(Colord(0.5, 0.25, 0.0), scene.background());
  }

  TEST(Scene, ShouldLeaveStaticSceneUnchangedWhenEvaluatingAnimation) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));

    scene.evaluateAnimationAtFrame(50);

    EXPECT_EQ(Colord(0.1, 0.2, 0.3), scene.background());
  }

  TEST(Scene, ShouldReturnEvaluatedStaticSceneCopy) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("camera-id");
    scene.addChild(camera);

    const auto evaluated = scene.evaluatedAtFrame(12);

    ASSERT_NE(nullptr, evaluated);
    EXPECT_NE(&scene, evaluated.get());
    EXPECT_NE(nullptr, qobject_cast<PinholeCamera*>(evaluated->findById("camera-id")));
  }

  TEST(Scene, ShouldReturnEvaluatedAnimationCopyWithoutChangingAuthoringScene) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("camera-id");
    camera->setPosition(Vector3d(0.0, 0.0, 0.0));
    scene.addChild(camera);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({world::AnimationTrack("camera-id", "position",
                                                                {
                                                                  {1, vectorValue(0.0, 0.0, 0.0)},
                                                                  {11, vectorValue(10.0, 0.0, 0.0)},
                                                                })})));

    const auto evaluated = scene.evaluatedAtFrame(6);

    EXPECT_EQ(Vector3d(0.0, 0.0, 0.0), camera->position());
    auto* evaluatedCamera = qobject_cast<PinholeCamera*>(evaluated->findById("camera-id"));
    ASSERT_NE(nullptr, evaluatedCamera);
    EXPECT_EQ(Vector3d(5.0, 0.0, 0.0), evaluatedCamera->position());
  }

  TEST(Scene, ShouldRejectNonObjectAnimationJson) {
    Scene scene;
    QJsonObject json;
    scene.write(json);
    json["animation"] = true;

    try {
      scene.read(json);
      FAIL() << "expected invalid animation block to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("scene animation must be an object"));
    }
  }

  TEST(Scene, ShouldRejectNonObjectRenderIntentJson) {
    Scene scene;
    QJsonObject json;
    scene.write(json);
    json["renderIntent"] = true;

    try {
      scene.read(json);
      FAIL() << "expected invalid renderIntent block to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("scene renderIntent must be an object"));
    }
  }

  TEST(Scene, ShouldProduceRaytracerSceneWithMatchingAmbient) {
    Scene scene;
    scene.setAmbient(Colord(0.7, 0.8, 0.9));
    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(Colord(0.7, 0.8, 0.9), rt->ambient());
  }

  TEST(Scene, ShouldProduceRaytracerSceneWithMatchingBackground) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), rt->background());
  }

  TEST(Scene, ShouldProduceEmptyRaytracerSceneWithNoChildren) {
    Scene scene;
    auto rt = scene.toRaytracerScene();
    ASSERT_NE(nullptr, rt);
    EXPECT_EQ(0u, rt->lights().size());
  }
}
