#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "world/objects/Scene.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Group.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/Rectangle.h"
#include "world/objects/Sphere.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"
#include "world/animation/AnimationTrack.h"
#include "world/animation/Timeline.h"
#include "core/geometry/Mesh.h"
#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Scene.h"
#include "render/primitives/SpatialIndexFactory.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/materials/Material.h"
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

  std::shared_ptr<render::MeshPrimitive> twoTriangleMeshPrimitive() {
    Mesh mesh;
    mesh.addVertex(Vector3d(-1.0, -1.0, 0.0), Vector3d(0.0, 0.0, 1.0));
    mesh.addVertex(Vector3d(1.0, -1.0, 0.0), Vector3d(0.0, 0.0, 1.0));
    mesh.addVertex(Vector3d(1.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0));
    mesh.addVertex(Vector3d(-1.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0));
    mesh.addFace({0, 1, 2});
    mesh.addFace({0, 2, 3});
    return std::make_shared<render::MeshPrimitive>(std::move(mesh),
                                                   render::MeshPrimitive::NormalMode::Flat);
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

  TEST(Scene, ShouldDefaultToBlackEnvironmentRadiance) {
    Scene scene;
    EXPECT_EQ(Colord::black(), scene.environmentRadiance());
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

  TEST(Scene, ShouldDefaultToAutomaticAccelerationMode) {
    Scene scene;
    EXPECT_EQ(static_cast<int>(render::AccelerationMode::Automatic), scene.accelerationMode());
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

  TEST(Scene, ShouldSetAndGetEnvironmentRadiance) {
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.2, 0.3, 0.4));
    EXPECT_EQ(Colord(0.2, 0.3, 0.4), scene.environmentRadiance());
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

  TEST(Scene, ShouldSetAndGetAccelerationMode) {
    Scene scene;
    scene.setAccelerationMode(static_cast<int>(render::AccelerationMode::BVH));

    EXPECT_EQ(static_cast<int>(render::AccelerationMode::BVH), scene.accelerationMode());
  }

  TEST(Scene, ShouldResetUnknownAccelerationModeToAutomatic) {
    Scene scene;
    scene.setAccelerationMode(99);

    EXPECT_EQ(static_cast<int>(render::AccelerationMode::Automatic), scene.accelerationMode());
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

  TEST(Scene, ShouldReturnAllSceneCameras) {
    Scene scene;
    auto* first = new PinholeCamera;
    first->setId("first-camera");
    scene.addChild(first);
    auto* group = new Group;
    auto* nested = new PinholeCamera;
    nested->setId("nested-camera");
    group->addChild(nested);
    scene.addChild(group);

    const auto cameras = scene.cameras();

    ASSERT_EQ(2u, cameras.size());
    EXPECT_EQ(first, cameras[0]);
    EXPECT_EQ(nested, cameras[1]);
  }

  TEST(Scene, ShouldResolveSceneCameraById) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("shot-camera");
    scene.addChild(camera);
    auto* sphere = new Sphere;
    sphere->setId("not-a-camera");
    scene.addChild(sphere);

    EXPECT_EQ(camera, scene.cameraById("shot-camera"));
    EXPECT_EQ(nullptr, scene.cameraById("not-a-camera"));
    EXPECT_EQ(nullptr, scene.cameraById("missing"));
  }

  TEST(Scene, ShouldResolveRenderIntentCamera) {
    Scene scene;
    auto* first = new PinholeCamera;
    first->setId("shot-camera");
    scene.addChild(first);
    auto* active = new PinholeCamera;
    active->setId("active-camera");
    scene.addChild(active);

    engine::graph::RenderIntent intent;
    intent.setDefaultCamera(engine::graph::RenderCameraRef{"shot-camera", std::nullopt});

    EXPECT_EQ(first, scene.cameraForRenderIntent(intent));
  }

  TEST(Scene, ShouldApplyWholeFrameCameraOverrideWhenResolvingRenderIntentCamera) {
    Scene scene;
    auto* active = new PinholeCamera;
    active->setId("active-camera");
    scene.addChild(active);
    auto* overrideCamera = new PinholeCamera;
    overrideCamera->setId("override-camera");
    scene.addChild(overrideCamera);

    engine::graph::RenderIntent intent;
    intent.setDefaultCamera(engine::graph::RenderCameraRef{"active-camera", std::nullopt});
    engine::graph::RenderViewOverride viewOverride;
    viewOverride.selector = engine::graph::SceneSelector::all();
    viewOverride.camera = engine::graph::RenderCameraRef{"override-camera", std::nullopt};
    intent.viewOverrides.push_back(viewOverride);

    EXPECT_EQ(overrideCamera, scene.cameraForRenderIntent(intent));
  }

  TEST(Scene, ShouldFallBackToActiveCameraWhenRenderIntentHasNoCamera) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("active-camera");
    scene.addChild(camera);

    EXPECT_EQ(camera, scene.cameraForRenderIntent(engine::graph::RenderIntent()));
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

  TEST(Scene, ShouldRoundtripSurfaceSceneMarkersThroughJson) {
    Scene original;
    auto* portalReceiver = new Rectangle;
    portalReceiver->setId("portal-panel");
    portalReceiver->setName("Portal Panel");
    portalReceiver->setPortalReceiverMarker(true);
    original.addChild(portalReceiver);
    auto* mirror = new Rectangle;
    mirror->setId("mirror-panel");
    mirror->setName("Mirror Panel");
    mirror->setPlanarMirrorMarker(true);
    original.addChild(mirror);

    QJsonObject json;
    original.write(json);
    const auto children = json["children"].toArray();
    ASSERT_EQ(2, children.size());
    EXPECT_TRUE(children[0].toObject()["portalReceiverMarker"].toBool());
    EXPECT_FALSE(children[0].toObject()["planarMirrorMarker"].toBool());
    EXPECT_FALSE(children[1].toObject()["portalReceiverMarker"].toBool());
    EXPECT_TRUE(children[1].toObject()["planarMirrorMarker"].toBool());

    Scene decoded;
    decoded.read(json);
    decoded.resolveElementReferences();

    const auto* decodedPortal = qobject_cast<const Rectangle*>(decoded.findById("portal-panel"));
    const auto* decodedMirror = qobject_cast<const Rectangle*>(decoded.findById("mirror-panel"));
    ASSERT_NE(nullptr, decodedPortal);
    ASSERT_NE(nullptr, decodedMirror);
    EXPECT_TRUE(decodedPortal->portalReceiverMarker());
    EXPECT_FALSE(decodedPortal->planarMirrorMarker());
    EXPECT_FALSE(decodedMirror->portalReceiverMarker());
    EXPECT_TRUE(decodedMirror->planarMirrorMarker());
  }

  TEST(Scene, ShouldDiscoverSurfaceSceneMarkersInRenderGraphAnalysis) {
    Scene scene;
    auto* portalReceiver = new Rectangle;
    portalReceiver->setId("portal-panel");
    portalReceiver->setName("Portal Panel");
    portalReceiver->setPortalReceiverMarker(true);
    scene.addChild(portalReceiver);
    auto* mirror = new Rectangle;
    mirror->setId("mirror-panel");
    mirror->setName("Mirror Panel");
    mirror->setPlanarMirrorMarker(true);
    scene.addChild(mirror);

    const auto analysis = scene.renderGraphAnalysis();

    EXPECT_EQ(2u, analysis.visibleSurfaceCount());
    ASSERT_EQ(1u, analysis.portalReceiverSurfaceCount());
    ASSERT_EQ(1u, analysis.planarMirrorSurfaceCount());
    EXPECT_EQ("portal-panel", analysis.portalReceiverSurfaces()[0].surfaceId);
    EXPECT_EQ("Portal Panel", analysis.portalReceiverSurfaces()[0].surfaceName);
    EXPECT_EQ("mirror-panel", analysis.planarMirrorSurfaces()[0].surfaceId);
    EXPECT_EQ("Mirror Panel", analysis.planarMirrorSurfaces()[0].surfaceName);
  }

  TEST(Scene, ShouldRejectSurfaceWithConflictingSceneMarkers) {
    Scene scene;
    QJsonObject json;
    json["type"] = "Scene";
    json["children"] = QJsonArray{QJsonObject{{"type", "Rectangle"},
                                              {"id", "bad-panel"},
                                              {"name", "Bad Panel"},
                                              {"portalReceiverMarker", true},
                                              {"planarMirrorMarker", true}}};

    try {
      scene.read(json);
      FAIL() << "expected conflicting scene markers to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("bad-panel"));
      EXPECT_THAT(error.what(), HasSubstr("cannot be both a portal receiver and a planar mirror"));
    }
  }

  TEST(Scene, ShouldRejectNonPlanarSurfaceSceneMarker) {
    Scene scene;
    QJsonObject json;
    json["type"] = "Scene";
    json["children"] = QJsonArray{QJsonObject{{"type", "Sphere"},
                                              {"id", "curved-surface"},
                                              {"name", "Curved Surface"},
                                              {"planarMirrorMarker", true}}};

    try {
      scene.read(json);
      FAIL() << "expected non-planar marker to throw";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr("curved-surface"));
      EXPECT_THAT(error.what(), HasSubstr("planar mirror marker requires a planar surface"));
    }
  }

  TEST(Scene, ShouldAnalyzeRenderTextureReceivers) {
    Scene scene;
    auto* material = new MatteMaterial;
    material->setRenderTextureSubview("material-feed");
    scene.addChild(material);
    auto* sphere = new Sphere;
    sphere->setRenderTextureSubview("surface-feed");
    scene.addChild(sphere);

    const auto analysis = scene.renderGraphAnalysis();

    EXPECT_TRUE(analysis.renderTextureSubviewReceivers().count("material-feed"));
    EXPECT_TRUE(analysis.renderTextureSubviewReceivers().count("surface-feed"));
  }

  TEST(Scene, ShouldPreserveRenderTextureReceiversAcrossJsonRoundTrip) {
    Scene scene;
    auto* material = new MatteMaterial;
    material->setId("screen-material");
    material->setRenderTextureSubview("material-feed");
    scene.addChild(material);
    auto* sphere = new Sphere;
    sphere->setId("screen");
    sphere->setRenderTextureSubview("surface-feed");
    scene.addChild(sphere);

    QJsonObject json;
    scene.write(json);

    Scene decoded;
    decoded.read(json);

    auto* decodedMaterial = dynamic_cast<MatteMaterial*>(decoded.findById("screen-material"));
    auto* decodedSurface = dynamic_cast<Sphere*>(decoded.findById("screen"));

    ASSERT_NE(nullptr, decodedMaterial);
    ASSERT_NE(nullptr, decodedSurface);
    EXPECT_EQ(QString("material-feed"), decodedMaterial->renderTextureSubview());
    EXPECT_EQ(QString("surface-feed"), decodedSurface->renderTextureSubview());
  }

  TEST(Scene, ShouldAnalyzeSelectorTagAndLayerMetadataFromVisibleObjects) {
    Scene scene;
    auto* sphere = new Sphere;
    sphere->setId("hero-sphere");
    sphere->setName("Hero Sphere");
    sphere->setMetadata(QJsonObject{{"tags", QJsonArray{"hero", "primary"}},
                                    {"layer", "foreground"}});
    scene.addChild(sphere);
    auto* light = new PointLight;
    light->setId("key-light");
    light->setName("Key Light");
    light->setMetadata(QJsonObject{{"tag", "hero"}, {"layerIndex", 2}});
    scene.addChild(light);

    const auto analysis = scene.renderGraphAnalysis();

    const auto objectMatch =
      analysis.matchSelector(engine::graph::SceneSelector::objectId("hero-sphere"));
    ASSERT_TRUE(objectMatch.matched());
    EXPECT_EQ("object_id:hero-sphere", objectMatch.subset->id);
    EXPECT_EQ("Hero Sphere", objectMatch.subset->label);

    const auto tagMatch = analysis.matchSelector(engine::graph::SceneSelector::tag("hero"));
    ASSERT_TRUE(tagMatch.matched());
    EXPECT_EQ("tag:hero", tagMatch.subset->id);
    EXPECT_EQ(2u, tagMatch.subset->elementCount);

    const auto layerMatch =
      analysis.matchSelector(engine::graph::SceneSelector::layer("foreground"));
    ASSERT_TRUE(layerMatch.matched());
    EXPECT_EQ("layer:foreground", layerMatch.subset->id);

    const auto layerIndexMatch = analysis.matchSelector(engine::graph::SceneSelector::layer("2"));
    ASSERT_TRUE(layerIndexMatch.matched());
    EXPECT_EQ("layer:2", layerIndexMatch.subset->id);
  }

  TEST(Scene, ShouldRoundtripSelectorTagAndLayerMetadataThroughSceneJson) {
    Scene original;
    auto* sphere = new Sphere;
    sphere->setId("hero-sphere");
    sphere->setName("Hero Sphere");
    sphere->setMetadata(QJsonObject{{"tags", QJsonArray{"hero", "primary"}},
                                    {"layerName", "foreground"}});
    original.addChild(sphere);

    QJsonObject json;
    original.write(json);

    Scene decoded;
    decoded.read(json);

    const auto analysis = decoded.renderGraphAnalysis();
    EXPECT_TRUE(analysis.matchSelector(engine::graph::SceneSelector::tag("hero")).matched());
    EXPECT_TRUE(
      analysis.matchSelector(engine::graph::SceneSelector::layer("foreground")).matched());
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

  TEST(Scene, ShouldRoundtripAmbientBackgroundAndEnvironmentViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    original.setAmbient(Colord(0.1, 0.2, 0.3));
    original.setBackground(Colord(0.4, 0.5, 0.6));
    original.setEnvironmentRadiance(Colord(0.7, 0.8, 0.9));
    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), decoded.ambient());
    EXPECT_EQ(Colord(0.4, 0.5, 0.6), decoded.background());
    EXPECT_EQ(Colord(0.7, 0.8, 0.9), decoded.environmentRadiance());

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

  TEST(Scene, ShouldRoundtripAccelerationModeViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    original.setAccelerationMode(static_cast<int>(render::AccelerationMode::Linear));

    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_EQ(static_cast<int>(render::AccelerationMode::Linear), decoded.accelerationMode());

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

  TEST(Scene, ShouldCompileEligibleCameraTracksToRuntimeCamera) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("camera-id");
    scene.addChild(camera);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({
        world::AnimationTrack(
          "camera-id", "position",
          {{1, vectorValue(0.0, 0.0, -5.0)}, {11, vectorValue(10.0, 0.0, -5.0)}}),
        world::AnimationTrack("camera-id", "target",
                              {{1, vectorValue(0.0, 0.0, 0.0)}, {11, vectorValue(0.0, 5.0, 0.0)}}),
      })));

    const auto runtimeCamera = camera->toRaytracer();

    ASSERT_NE(nullptr, runtimeCamera->animationTrack("position"));
    EXPECT_EQ(Vector3d(5.0, 0.0, -5.0),
              runtimeCamera->animationTrack("position")->sample(6.0).get<Vector3d>());
    ASSERT_NE(nullptr, runtimeCamera->animationTrack("target"));
    EXPECT_EQ(Vector3d(0.0, 2.5, 0.0),
              runtimeCamera->animationTrack("target")->sample(6.0).get<Vector3d>());
    EXPECT_EQ("camera-id", runtimeCamera->metadataValue("world:id"));
  }

  TEST(Scene, ShouldCompileEligibleSurfaceTracksToRuntimeInstanceOnly) {
    Scene scene;
    auto* sphere = new Sphere;
    sphere->setId("sphere-id");
    scene.addChild(sphere);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({
        world::AnimationTrack("sphere-id", "position",
                              {{1, vectorValue(0.0, 0.0, 0.0)}, {11, vectorValue(10.0, 0.0, 0.0)}}),
        world::AnimationTrack("sphere-id", "visible",
                              {{1, QJsonValue(true)}, {11, QJsonValue(false)}},
                              core::math::interpolation::InterpolationMode::Step),
      })));

    auto runtimeScene = std::make_shared<render::Scene>();
    const auto primitive = sphere->toRaytracer(runtimeScene.get());
    const auto instance = std::dynamic_pointer_cast<render::Instance>(primitive);

    ASSERT_NE(nullptr, instance);
    ASSERT_NE(nullptr, instance->animationTrack("position"));
    EXPECT_EQ(Vector3d(5.0, 0.0, 0.0),
              instance->animationTrack("position")->sample(6.0).get<Vector3d>());
    EXPECT_EQ(nullptr, instance->animationTrack("visible"));
    EXPECT_EQ("1", instance->metadataValue("animation:evaluatedFrame"));
  }

  TEST(Scene, ShouldTagRuntimeSurfaceTracksWithEvaluatedFrame) {
    Scene scene;
    auto* sphere = new Sphere;
    sphere->setId("sphere-id");
    scene.addChild(sphere);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({world::AnimationTrack(
        "sphere-id", "position",
        {{1, vectorValue(0.0, 0.0, 0.0)}, {11, vectorValue(10.0, 0.0, 0.0)}})})));
    scene.evaluateAnimationAtFrame(6);

    auto runtimeScene = std::make_shared<render::Scene>();
    const auto primitive = sphere->toRaytracer(runtimeScene.get());
    const auto instance = std::dynamic_pointer_cast<render::Instance>(primitive);

    ASSERT_NE(nullptr, instance);
    EXPECT_EQ("6", instance->metadataValue("animation:evaluatedFrame"));
  }

  TEST(Scene, ShouldCompileEligibleLightTracksToRuntimeLight) {
    Scene scene;
    auto* light = new PointLight;
    light->setId("light-id");
    scene.addChild(light);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({
        world::AnimationTrack("light-id", "color",
                              {{1, colorValue(0.0, 0.0, 0.0)}, {11, colorValue(1.0, 0.5, 0.0)}}),
        world::AnimationTrack("light-id", "intensity",
                              {{1, QJsonValue(0.0)}, {11, QJsonValue(1.0)}}),
      })));

    const auto runtimeLight = light->toRaytracer();

    ASSERT_NE(nullptr, runtimeLight->animationTrack("color"));
    EXPECT_EQ(Colord(0.5, 0.25, 0.0),
              runtimeLight->animationTrack("color")->sample(6.0).get<Colord>());
    ASSERT_NE(nullptr, runtimeLight->animationTrack("intensity"));
    EXPECT_DOUBLE_EQ(0.5, runtimeLight->animationTrack("intensity")->sample(6.0).get<double>());
  }

  TEST(Scene, ShouldCompileEligibleMaterialTracksToRuntimeMaterial) {
    Scene scene;
    auto* material = new PhongMaterial;
    material->setId("material-id");
    scene.addChild(material);
    auto* sphere = new Sphere;
    sphere->setId("sphere-id");
    sphere->setMaterial(material);
    scene.addChild(sphere);
    scene.setAnimation(std::make_unique<world::Timeline>(
      1, 11, 24.0,
      std::vector<world::AnimationTrack>({
        world::AnimationTrack("material-id", "specularCoefficient",
                              {{1, QJsonValue(0.0)}, {11, QJsonValue(1.0)}}),
        world::AnimationTrack("material-id", "sidedness",
                              {{1, QJsonValue("Front")}, {11, QJsonValue("Back")}},
                              core::math::interpolation::InterpolationMode::Step),
      })));

    auto runtimeScene = std::make_shared<render::Scene>();
    const auto primitive = sphere->toRaytracer(runtimeScene.get());
    ASSERT_NE(nullptr, primitive);
    const auto runtimeMaterial = primitive->material();

    ASSERT_NE(nullptr, runtimeMaterial->animationTrack("specularCoefficient"));
    EXPECT_DOUBLE_EQ(
      0.5, runtimeMaterial->animationTrack("specularCoefficient")->sample(6.0).get<double>());
    EXPECT_EQ(nullptr, runtimeMaterial->animationTrack("sidedness"));
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

  TEST(Scene, ShouldProduceRaytracerSceneWithMatchingEnvironmentRadiance) {
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.4, 0.5, 0.6));
    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(Colord(0.4, 0.5, 0.6), rt->environmentRadiance());
  }

  TEST(Scene, ShouldProduceEmptyRaytracerSceneWithNoChildren) {
    Scene scene;
    auto rt = scene.toRaytracerScene();
    ASSERT_NE(nullptr, rt);
    EXPECT_EQ(0u, rt->lights().size());
  }

  TEST(Scene, ShouldUseLinearAccelerationForSinglePrimitiveAutoScene) {
    Scene scene;
    scene.addChild(new Sphere);

    auto rt = scene.toRaytracerScene();

    ASSERT_TRUE(rt->accelerationDecision().has_value());
    EXPECT_EQ(render::AccelerationMode::Automatic, rt->accelerationDecision()->requestedMode);
    EXPECT_EQ(render::SpatialIndexKind::Linear, rt->accelerationDecision()->spatialIndexKind);
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::Composite>(rt->primitives().front()));
  }

  TEST(Scene, ShouldUseBVHAccelerationForProceduralMultiPrimitiveAutoScene) {
    Scene scene;
    scene.addChild(new Sphere);
    auto* second = new Sphere;
    second->setPosition(Vector3d(4.0, 0.0, 0.0));
    scene.addChild(second);

    auto rt = scene.toRaytracerScene();

    ASSERT_TRUE(rt->accelerationDecision().has_value());
    EXPECT_EQ(render::AccelerationMode::Automatic, rt->accelerationDecision()->requestedMode);
    EXPECT_EQ(render::SpatialIndexKind::BVH, rt->accelerationDecision()->spatialIndexKind);
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::BVH>(rt->primitives().front()));
  }

  TEST(Scene, ShouldUseLeafCountWhenChoosingAccelerationForImportedMeshGroups) {
    Scene scene;
    auto* importRoot = new Group;
    importRoot->setName("Imported Mesh Root");
    importRoot->addChild(new CompiledPrimitive(twoTriangleMeshPrimitive()));
    scene.addChild(importRoot);

    auto rt = scene.toRaytracerScene();

    ASSERT_TRUE(rt->accelerationDecision().has_value());
    EXPECT_EQ(render::AccelerationMode::Automatic, rt->accelerationDecision()->requestedMode);
    EXPECT_EQ(render::SpatialIndexKind::BVH, rt->accelerationDecision()->spatialIndexKind);
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::BVH>(rt->primitives().front()));
  }

  TEST(Scene, ShouldUseManualLinearAccelerationOverride) {
    Scene scene;
    scene.setAccelerationMode(static_cast<int>(render::AccelerationMode::Linear));
    scene.addChild(new Sphere);

    auto rt = scene.toRaytracerScene();

    ASSERT_TRUE(rt->accelerationDecision().has_value());
    EXPECT_EQ(render::AccelerationMode::Linear, rt->accelerationDecision()->requestedMode);
    EXPECT_EQ(render::SpatialIndexKind::Linear, rt->accelerationDecision()->spatialIndexKind);
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::Composite>(rt->primitives().front()));
  }

  TEST(Scene, ShouldUseManualBVHAccelerationOverride) {
    Scene scene;
    scene.setAccelerationMode(static_cast<int>(render::AccelerationMode::BVH));
    scene.addChild(new Sphere);

    auto rt = scene.toRaytracerScene();

    ASSERT_TRUE(rt->accelerationDecision().has_value());
    EXPECT_EQ(render::AccelerationMode::BVH, rt->accelerationDecision()->requestedMode);
    EXPECT_EQ(render::SpatialIndexKind::BVH, rt->accelerationDecision()->spatialIndexKind);
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::BVH>(rt->primitives().front()));
  }
}
