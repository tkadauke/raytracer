#include <gtest/gtest.h>

#include "engine/graph/RenderExecutor.h"
#include "engine/graph/RenderGraphTypes.h"

#include <QJsonArray>
#include <QJsonObject>

#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace RenderGraphTypesTest {
  using namespace engine::graph;

  TEST(SceneSelector, FormatsDisplayText) {
    EXPECT_EQ("all", SceneSelector::all().displayText());
    EXPECT_EQ("object_name: Hero", SceneSelector::objectName("Hero").displayText());
  }

  TEST(RenderCameraRef, FormatsDisplayText) {
    RenderCameraRef empty;
    EXPECT_EQ("-", empty.displayText());

    RenderCameraRef sceneCamera;
    sceneCamera.sceneCameraId = "shot-camera";
    EXPECT_EQ("shot-camera", sceneCamera.displayText());

    sceneCamera.snapshot = CameraSnapshot{QJsonObject{}};
    EXPECT_EQ("shot-camera, snapshot", sceneCamera.displayText());
  }

  TEST(RenderCameraRef, ComparesExecutionEquivalence) {
    RenderCameraRef first;
    first.sceneCameraId = "shot-camera";
    first.snapshot = CameraSnapshot{QJsonObject{{"zoom", 1.0}}};
    RenderCameraRef same;
    same.sceneCameraId = "shot-camera";
    same.snapshot = CameraSnapshot{QJsonObject{{"zoom", 1.0}}};
    RenderCameraRef different;
    different.sceneCameraId = "other-camera";
    different.snapshot = CameraSnapshot{QJsonObject{{"zoom", 1.0}}};

    EXPECT_TRUE(first.equivalentTo(same));
    EXPECT_FALSE(first.equivalentTo(different));
  }

  TEST(RenderExecutor, SerializesWavefrontPreferenceAndKind) {
    EXPECT_EQ(std::string("wavefront"), toString(RenderExecutorPreference::Wavefront));
    EXPECT_EQ(std::string("wavefront"), toString(RenderExecutorKind::Wavefront));

    QJsonObject intentJson;
    intentJson["defaultExecutor"] = "wavefront";
    const RenderIntent intent = RenderIntent::fromJson(intentJson);
    EXPECT_EQ(RenderExecutorPreference::Wavefront, intent.defaultExecutor);
    EXPECT_EQ(RenderExecutorKind::Wavefront, intent.defaultExecutorKind());

    QJsonObject passJson;
    passJson["id"] = "wavefront_beauty";
    passJson["kind"] = "beauty";
    passJson["executor"] = "wavefront";
    passJson["writes"] = QJsonArray{"beauty_color"};
    const RenderPassNode pass = RenderPassNode::fromJson(passJson);
    EXPECT_EQ(RenderExecutorKind::Wavefront, pass.executor);
  }

  TEST(ShadingProfileRef, FormatsDisplayTextAndDetectsDefaultProfile) {
    ShadingProfileRef profile;
    EXPECT_TRUE(profile.isDefault());
    EXPECT_EQ("default", profile.displayText());

    profile.name = "toon";
    EXPECT_FALSE(profile.isDefault());
    EXPECT_EQ("toon", profile.displayText());

    profile.parameters.emplace("enabled", ShadingProfileParameterValue(true));
    profile.parameters.emplace("levels", ShadingProfileParameterValue(4.0));
    profile.parameters.emplace("ramp", ShadingProfileParameterValue(std::string("warm")));
    EXPECT_EQ("toon(enabled=true, levels=4, ramp=warm)", profile.displayText());

    const QJsonObject json = profile.toJson();
    const auto parameters = json["parameters"].toObject();
    EXPECT_TRUE(parameters["enabled"].toBool());
    EXPECT_EQ(4, parameters["levels"].toInt());
    EXPECT_EQ("warm", parameters["ramp"].toString().toStdString());

    const auto decoded = ShadingProfileRef::fromJson(json);
    EXPECT_EQ(profile.name, decoded.name);
    EXPECT_EQ(profile.parameters, decoded.parameters);
  }

  TEST(ShadingProfileRef, OwnsParameterMutationAndLookup) {
    ShadingProfileRef profile;

    EXPECT_EQ(nullptr, profile.parameter("levels"));
    profile.setParameter("levels", ShadingProfileParameterValue(3.0));
    profile.setParameter("enabled", ShadingProfileParameterValue(true));
    profile.setParameter("levels", ShadingProfileParameterValue(4.0));

    ASSERT_NE(nullptr, profile.parameter("levels"));
    EXPECT_EQ(ShadingProfileParameterValue(4.0), *profile.parameter("levels"));
    ASSERT_NE(nullptr, profile.parameter("enabled"));
    EXPECT_EQ(ShadingProfileParameterValue(true), *profile.parameter("enabled"));
  }

  TEST(ShadingProfileParameterValue, ParsesTextScalars) {
    EXPECT_EQ(ShadingProfileParameterValue(true), ShadingProfileParameterValue::fromText("true"));
    EXPECT_EQ(ShadingProfileParameterValue(false), ShadingProfileParameterValue::fromText("FALSE"));
    EXPECT_EQ(ShadingProfileParameterValue(4.25), ShadingProfileParameterValue::fromText("4.25"));
    EXPECT_EQ(ShadingProfileParameterValue(4.25),
              ShadingProfileParameterValue::fromText("  4.25  "));
    EXPECT_EQ(ShadingProfileParameterValue(std::string("warm")),
              ShadingProfileParameterValue::fromText("warm"));
  }

  TEST(RenderPassNode, OwnsFeatureQueries) {
    RenderPassNode pass;
    pass.features = {"main", "rasterizer", "render_to_texture"};

    EXPECT_TRUE(pass.hasFeature("rasterizer"));
    EXPECT_FALSE(pass.hasFeature("raytracer"));
    EXPECT_TRUE(pass.hasAnyFeature({"raytracer", "render_to_texture"}));
    EXPECT_FALSE(pass.hasAnyFeature({"raytracer", "wireframe"}));
    EXPECT_FALSE(pass.hasAnyFeature(std::set<RenderFeatureKind>{}));
    EXPECT_TRUE(pass.hasAllFeatures({"main", "render_to_texture"}));
    EXPECT_FALSE(pass.hasAllFeatures({"main", "raytracer"}));
    EXPECT_TRUE(pass.hasAllFeatures(std::set<RenderFeatureKind>{}));
  }

  TEST(RenderResourceDescriptor, OwnsFeatureQueries) {
    RenderResourceDescriptor resource;
    resource.features = {"subview", "subview_color_output", "render_to_texture"};

    EXPECT_TRUE(resource.hasFeature("subview_color_output"));
    EXPECT_FALSE(resource.hasFeature("subview_depth_output"));
    EXPECT_TRUE(resource.hasAllFeatures({"subview", "render_to_texture"}));
    EXPECT_FALSE(resource.hasAllFeatures({"subview", "subview_depth_output"}));
    EXPECT_TRUE(resource.hasAllFeatures(std::set<RenderFeatureKind>{}));
  }

  TEST(RenderIntent, SerializesToSceneJsonShape) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Depth;
    intent.defaultShadingProfile.name = "toon";
    intent.defaultShadingProfile.parameters.emplace("levels", ShadingProfileParameterValue(4.0));
    intent.defaultCamera = RenderCameraRef{"camera-a", std::nullopt};
    intent.enableAutomaticFeatures = false;
    intent.enableWireframeOverlay = true;
    intent.enableCurveOverlay = true;
    intent.enablePreviewShadows = true;
    intent.postProcessAA = RenderPostProcessAA::SMAA;
    intent.setMaxRenderToTextureRecursionDepth(3);
    intent.engineOptions.raytracer().setIntegrator("pathtracer");
    intent.engineOptions.raytracer().setSampler("Jittered");
    intent.engineOptions.raytracer().setSamplesPerPixel(8);
    intent.engineOptions.raytracer().setDenoiser("bilateral");
    intent.engineOptions.raytracer().setDenoiseRadius(2);
    intent.engineOptions.raytracer().setDenoiseColorSigma(0.2);
    intent.engineOptions.rasterizer().setBackend(engine::raster::RasterBackend::openGL());
    intent.engineOptions.rasterizer().setVisibilityCulling(RenderVisibilityCulling::Auto);
    intent.engineOptions.rasterizer().setLod(3);
    intent.engineOptions.rasterizer().setTessellationQuality("final");
    intent.engineOptions.rasterizer().setMaximumScreenSpaceError(0.25);
    intent.engineOptions.rasterizer().setMSAASamples(4);
    intent.engineOptions.rasterizer().setShadowMapSize(128);
    intent.engineOptions.wireframe().setLod(2);
    intent.exportedAOVs = {RenderViewMode::Depth, RenderViewMode::Normal,
                           RenderViewMode::RasterDepthTestCount};
    RenderViewOverride override;
    override.selector = SceneSelector::tag("debug");
    override.executor = RenderExecutorPreference::Wireframe;
    override.viewMode = RenderViewMode::Wireframe;
    intent.viewOverrides.push_back(override);

    const QJsonObject json = intent.toJson();

    EXPECT_EQ("rasterizer", json["defaultExecutor"].toString().toStdString());
    EXPECT_EQ("depth", json["defaultViewMode"].toString().toStdString());
    EXPECT_FALSE(json["enableAutomaticFeatures"].toBool());
    EXPECT_TRUE(json["enableWireframeOverlay"].toBool());
    EXPECT_TRUE(json["enableCurveOverlay"].toBool());
    EXPECT_TRUE(json["enablePreviewShadows"].toBool());
    EXPECT_EQ("smaa", json["postProcessAA"].toString().toStdString());
    EXPECT_EQ(3, json["maxRenderToTextureRecursionDepth"].toInt());
    const auto engineOptions = json["engineOptions"].toObject();
    EXPECT_EQ("pathtracer", engineOptions["raytracer"]
                              .toObject()["execution"]
                              .toObject()["integrator"]
                              .toString()
                              .toStdString());
    EXPECT_EQ("Jittered", engineOptions["raytracer"]
                            .toObject()["sampling"]
                            .toObject()["sampler"]
                            .toString()
                            .toStdString());
    EXPECT_EQ(
      8, engineOptions["raytracer"].toObject()["sampling"].toObject()["samplesPerPixel"].toInt());
    EXPECT_EQ(
      "bilateral",
      engineOptions["raytracer"].toObject()["denoise"].toObject()["type"].toString().toStdString());
    EXPECT_EQ(2, engineOptions["raytracer"].toObject()["denoise"].toObject()["radius"].toInt());
    EXPECT_DOUBLE_EQ(
      0.2, engineOptions["raytracer"].toObject()["denoise"].toObject()["colorSigma"].toDouble());
    EXPECT_EQ("opengl", engineOptions["rasterizer"]
                          .toObject()["execution"]
                          .toObject()["backend"]
                          .toString()
                          .toStdString());
    EXPECT_EQ("auto", engineOptions["rasterizer"]
                        .toObject()["geometry"]
                        .toObject()["visibilityCulling"]
                        .toString()
                        .toStdString());
    EXPECT_EQ(3, engineOptions["rasterizer"].toObject()["geometry"].toObject()["lod"].toInt());
    EXPECT_EQ("final", engineOptions["rasterizer"]
                         .toObject()["geometry"]
                         .toObject()["quality"]
                         .toString()
                         .toStdString());
    EXPECT_DOUBLE_EQ(0.25, engineOptions["rasterizer"]
                             .toObject()["geometry"]
                             .toObject()["maxScreenSpaceError"]
                             .toDouble());
    EXPECT_EQ(4,
              engineOptions["rasterizer"].toObject()["sampling"].toObject()["msaaSamples"].toInt());
    EXPECT_EQ(128, engineOptions["rasterizer"].toObject()["shadows"].toObject()["mapSize"].toInt());
    EXPECT_EQ(2, engineOptions["wireframe"].toObject()["lod"].toInt());
    const auto exportedAOVs = json["exportedAOVs"].toArray();
    ASSERT_EQ(3, exportedAOVs.size());
    EXPECT_EQ("depth", exportedAOVs.at(0).toString().toStdString());
    EXPECT_EQ("normal", exportedAOVs.at(1).toString().toStdString());
    EXPECT_EQ("raster_depth_test_count", exportedAOVs.at(2).toString().toStdString());

    const auto profile = json["defaultShadingProfile"].toObject();
    EXPECT_EQ("toon", profile["name"].toString().toStdString());
    EXPECT_EQ(4, profile["parameters"].toObject()["levels"].toInt());
    EXPECT_EQ("camera-a",
              json["defaultCamera"].toObject()["sceneCameraId"].toString().toStdString());

    const auto overrides = json["viewOverrides"].toArray();
    ASSERT_EQ(1, overrides.size());
    const auto overrideJson = overrides.at(0).toObject();
    EXPECT_EQ("tag", overrideJson["selector"].toObject()["kind"].toString().toStdString());
    EXPECT_EQ("debug", overrideJson["selector"].toObject()["value"].toString().toStdString());
    EXPECT_EQ("wireframe", overrideJson["executor"].toString().toStdString());
    EXPECT_EQ("wireframe", overrideJson["viewMode"].toString().toStdString());
  }

  TEST(RenderIntent, ReadsSceneJsonShapeWithDefaults) {
    QJsonObject json;
    json["defaultExecutor"] = "rasterizer";
    json["defaultShadingProfile"] = "toon";
    QJsonObject raytracerExecution;
    raytracerExecution["integrator"] = "pt";
    QJsonObject raytracerSampling;
    raytracerSampling["samplesPerPixel"] = 12;
    QJsonObject raytracerDenoise;
    raytracerDenoise["type"] = "bilateral";
    raytracerDenoise["radius"] = 5;
    raytracerDenoise["colorSigma"] = 0.3;
    QJsonObject raytracerOptions;
    raytracerOptions["execution"] = raytracerExecution;
    raytracerOptions["sampling"] = raytracerSampling;
    raytracerOptions["denoise"] = raytracerDenoise;
    QJsonObject rasterSampling;
    rasterSampling["msaaSamples"] = 4;
    QJsonObject rasterExecution;
    rasterExecution["backend"] = "gpu";
    QJsonObject rasterGeometry;
    rasterGeometry["visibilityCulling"] = "on";
    rasterGeometry["lod"] = 2;
    rasterGeometry["quality"] = "preview";
    rasterGeometry["maxScreenSpaceError"] = 4.5;
    QJsonObject rasterizerOptions;
    rasterizerOptions["execution"] = rasterExecution;
    rasterizerOptions["geometry"] = rasterGeometry;
    rasterizerOptions["sampling"] = rasterSampling;
    QJsonObject engineOptions;
    engineOptions["raytracer"] = raytracerOptions;
    engineOptions["rasterizer"] = rasterizerOptions;
    json["engineOptions"] = engineOptions;

    QJsonObject selector;
    selector["kind"] = "object_name";
    selector["value"] = "Monitor";
    QJsonObject viewOverride;
    viewOverride["selector"] = selector;
    viewOverride["executor"] = "wireframe";
    json["viewOverrides"] = QJsonArray{viewOverride};
    json["exportedAOVs"] =
      QJsonArray{"depth", "stencil", "world_position", "raster_color_write_count"};
    json["maxRenderToTextureRecursionDepth"] = 4;

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderExecutorPreference::Rasterizer, intent.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Beauty, intent.defaultViewMode);
    EXPECT_EQ("toon", intent.defaultShadingProfile.name);
    EXPECT_TRUE(intent.enableAutomaticFeatures);
    EXPECT_FALSE(intent.enableWireframeOverlay);
    EXPECT_FALSE(intent.enableCurveOverlay);
    EXPECT_EQ(RenderPostProcessAA::None, intent.postProcessAA);
    EXPECT_EQ(4, intent.maxRenderToTextureRecursionDepth);
    ASSERT_TRUE(intent.engineOptions.raytracer().integrator().has_value());
    EXPECT_EQ("pathtracer", *intent.engineOptions.raytracer().integrator());
    ASSERT_TRUE(intent.engineOptions.raytracer().samplesPerPixel().has_value());
    EXPECT_EQ(12, *intent.engineOptions.raytracer().samplesPerPixel());
    ASSERT_TRUE(intent.engineOptions.raytracer().denoiser().has_value());
    EXPECT_EQ("bilateral", *intent.engineOptions.raytracer().denoiser());
    ASSERT_TRUE(intent.engineOptions.raytracer().denoiseRadius().has_value());
    EXPECT_EQ(5, *intent.engineOptions.raytracer().denoiseRadius());
    ASSERT_TRUE(intent.engineOptions.raytracer().denoiseColorSigma().has_value());
    EXPECT_DOUBLE_EQ(0.3, *intent.engineOptions.raytracer().denoiseColorSigma());
    ASSERT_TRUE(intent.engineOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(4, *intent.engineOptions.rasterizer().msaaSamples());
    ASSERT_TRUE(intent.engineOptions.rasterizer().backend().has_value());
    EXPECT_TRUE(intent.engineOptions.rasterizer().backend()->isOpenGL());
    ASSERT_TRUE(intent.engineOptions.rasterizer().visibilityCulling().has_value());
    EXPECT_EQ(RenderVisibilityCulling::On, *intent.engineOptions.rasterizer().visibilityCulling());
    ASSERT_TRUE(intent.engineOptions.rasterizer().lod().has_value());
    EXPECT_EQ(2, *intent.engineOptions.rasterizer().lod());
    ASSERT_TRUE(intent.engineOptions.rasterizer().tessellationQuality().has_value());
    EXPECT_EQ("preview", *intent.engineOptions.rasterizer().tessellationQuality());
    ASSERT_TRUE(intent.engineOptions.rasterizer().maximumScreenSpaceError().has_value());
    EXPECT_DOUBLE_EQ(4.5, *intent.engineOptions.rasterizer().maximumScreenSpaceError());
    ASSERT_EQ(4u, intent.exportedAOVs.size());
    EXPECT_EQ(RenderViewMode::Depth, intent.exportedAOVs[0]);
    EXPECT_EQ(RenderViewMode::Stencil, intent.exportedAOVs[1]);
    EXPECT_EQ(RenderViewMode::WorldPosition, intent.exportedAOVs[2]);
    EXPECT_EQ(RenderViewMode::RasterColorWriteCount, intent.exportedAOVs[3]);
    ASSERT_EQ(1u, intent.viewOverrides.size());
    EXPECT_EQ(SceneSelector::Kind::ObjectName, intent.viewOverrides.front().selector.kind);
    EXPECT_EQ("Monitor", intent.viewOverrides.front().selector.value);
    ASSERT_TRUE(intent.viewOverrides.front().executor.has_value());
    EXPECT_EQ(RenderExecutorPreference::Wireframe, *intent.viewOverrides.front().executor);
  }

  TEST(RenderIntent, DefaultsRenderToTextureRecursionDepthToOneAndRejectsNegativeJson) {
    RenderIntent defaults;
    EXPECT_EQ(1, defaults.maxRenderToTextureRecursionDepth);
    defaults.setMaxRenderToTextureRecursionDepth(-7);
    EXPECT_EQ(0, defaults.maxRenderToTextureRecursionDepth);

    QJsonObject json;
    json["maxRenderToTextureRecursionDepth"] = -1;
    EXPECT_THROW(RenderIntent::fromJson(json), std::runtime_error);
  }

  TEST(RenderIntent, ReadsStencilCompositeViewMode) {
    QJsonObject json;
    json["defaultViewMode"] = "stencil_composite";

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderViewMode::StencilComposite, intent.defaultViewMode);
    EXPECT_EQ("stencil_composite", std::string(toString(intent.defaultViewMode)));
    EXPECT_EQ(RenderExecutorKind::Rasterizer, intent.defaultExecutorKind());
  }

  TEST(RenderIntent, OwnsAOVAndShadingProfileIntentMutation) {
    RenderIntent intent;

    intent.setDefaultExecutor(RenderExecutorPreference::Rasterizer);
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, intent.defaultExecutor);
    intent.setDefaultViewMode(RenderViewMode::Depth);
    EXPECT_EQ(RenderViewMode::Depth, intent.defaultViewMode);

    intent.setDefaultCamera(RenderCameraRef{"shot-camera", std::nullopt});
    ASSERT_TRUE(intent.defaultCamera.has_value());
    ASSERT_TRUE(intent.defaultCamera->sceneCameraId.has_value());
    EXPECT_EQ("shot-camera", *intent.defaultCamera->sceneCameraId);

    intent.setDefaultShadingProfile(ShadingProfileRef{"clay", {}});
    EXPECT_EQ("clay", intent.defaultShadingProfile.name);

    EXPECT_FALSE(intent.exportsAOV(RenderViewMode::Depth));
    intent.requestExportedAOV(RenderViewMode::Depth);
    intent.requestExportedAOV(RenderViewMode::Depth);
    intent.requestExportedAOV(RenderViewMode::Normal);
    ASSERT_EQ(2u, intent.exportedAOVs.size());
    EXPECT_TRUE(intent.exportsAOV(RenderViewMode::Depth));
    EXPECT_TRUE(intent.exportsAOV(RenderViewMode::Normal));

    intent.setDefaultShadingProfileParameter("levels", ShadingProfileParameterValue(5.0));
    ASSERT_NE(nullptr, intent.defaultShadingProfile.parameter("levels"));
    EXPECT_EQ(ShadingProfileParameterValue(5.0), *intent.defaultShadingProfile.parameter("levels"));

    intent.setPostProcessAA(RenderPostProcessAA::FXAA);
    EXPECT_EQ(RenderPostProcessAA::FXAA, intent.postProcessAA);
    intent.setAutomaticFeaturesEnabled(false);
    EXPECT_FALSE(intent.enableAutomaticFeatures);
    intent.setWireframeOverlayEnabled(true);
    EXPECT_TRUE(intent.enableWireframeOverlay);
    intent.setPreviewShadowsEnabled(true);
    EXPECT_TRUE(intent.enablePreviewShadows);
  }

  TEST(RenderIntent, RejectsNonAOVExportRequests) {
    RenderIntent intent;

    EXPECT_THROW(intent.requestExportedAOV(RenderViewMode::Beauty), std::runtime_error);

    QJsonObject json;
    json["exportedAOVs"] = QJsonArray{"beauty"};
    EXPECT_THROW(RenderIntent::fromJson(json), std::runtime_error);
  }

  TEST(RenderIntent, RejectsUnknownExecutorName) {
    QJsonObject json;
    json["defaultExecutor"] = "path_tracer";

    EXPECT_THROW(RenderIntent::fromJson(json), std::runtime_error);
  }

  TEST(RenderIntent, ReadsPostProcessAAFromSceneJson) {
    QJsonObject json;
    json["postProcessAA"] = "fxaa";

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderExecutorKind::Raytracer, intent.defaultExecutorKind());
    EXPECT_EQ(RenderPostProcessAA::FXAA, intent.postProcessAA);
    EXPECT_TRUE(intent.usesGraphImagePostProcessAA());
  }

  TEST(RenderIntent, AppliesWholeFrameViewOverridesToDefaults) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Raytracer;
    intent.defaultViewMode = RenderViewMode::Beauty;
    intent.defaultShadingProfile.name = "default";

    RenderViewOverride wholeFrame;
    wholeFrame.selector = SceneSelector::all();
    wholeFrame.executor = RenderExecutorPreference::Rasterizer;
    wholeFrame.viewMode = RenderViewMode::Depth;
    wholeFrame.shadingProfile = ShadingProfileRef{"clay", {}};
    wholeFrame.camera = RenderCameraRef{"inspection-camera", std::nullopt};
    wholeFrame.engineOptions.rasterizer().setMSAASamples(8);
    intent.viewOverrides.push_back(wholeFrame);

    RenderViewOverride tagOverride;
    tagOverride.selector = SceneSelector::tag("debug");
    tagOverride.executor = RenderExecutorPreference::Wireframe;
    intent.viewOverrides.push_back(tagOverride);

    RenderIntent directlyApplied;
    directlyApplied.applyWholeFrameOverride(wholeFrame);
    directlyApplied.applyWholeFrameOverride(tagOverride);
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, directlyApplied.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Depth, directlyApplied.defaultViewMode);

    const RenderIntent effective = intent.withWholeFrameOverridesApplied();

    EXPECT_TRUE(wholeFrame.appliesToWholeFrame());
    EXPECT_FALSE(tagOverride.appliesToWholeFrame());
    EXPECT_TRUE(intent.hasSelectorSpecificOverrides());
    const auto selectorSpecific = intent.selectorSpecificOverrides();
    ASSERT_EQ(1u, selectorSpecific.size());
    EXPECT_EQ(SceneSelector::Kind::Tag, selectorSpecific.front().selector.kind);
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, effective.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Depth, effective.defaultViewMode);
    EXPECT_EQ("clay", effective.defaultShadingProfile.name);
    ASSERT_TRUE(effective.defaultCamera.has_value());
    ASSERT_TRUE(effective.defaultCamera->sceneCameraId.has_value());
    EXPECT_EQ("inspection-camera", *effective.defaultCamera->sceneCameraId);
    ASSERT_TRUE(effective.defaultSceneView().camera.has_value());
    ASSERT_TRUE(effective.defaultSceneView().camera->sceneCameraId.has_value());
    EXPECT_EQ("inspection-camera", *effective.defaultSceneView().camera->sceneCameraId);
    ASSERT_TRUE(effective.defaultSceneView().shadingProfile.has_value());
    EXPECT_EQ("clay", effective.defaultSceneView().shadingProfile->name);
    ASSERT_TRUE(effective.engineOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(8, *effective.engineOptions.rasterizer().msaaSamples());
    ASSERT_EQ(1u, effective.viewOverrides.size());
    EXPECT_EQ(SceneSelector::Kind::Tag, effective.viewOverrides.front().selector.kind);
  }

  TEST(RenderIntent, RejectsSelectorSpecificOverridesForWholeFrameOnlyCallers) {
    RenderIntent intent;

    RenderViewOverride objectOverride;
    objectOverride.selector = SceneSelector::objectName("Monitor");
    objectOverride.executor = RenderExecutorPreference::Wireframe;
    intent.viewOverrides.push_back(objectOverride);

    try {
      intent.requireWholeFrameOnly("test compiler");
      FAIL() << "Expected selector-specific intent rejection";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(std::string::npos, message.find("test compiler"));
      EXPECT_NE(std::string::npos, message.find("object_name: Monitor"));
    }
  }

  TEST(RenderSubviewIntent, ResolvesInheritedAndIndependentEngineOptions) {
    RenderEngineOptions global;
    global.raytracer().setSamplesPerPixel(8);
    global.raytracer().setDenoiser("bilateral");
    global.raytracer().setDenoiseRadius(3);
    global.raytracer().setDenoiseColorSigma(0.25);
    global.rasterizer().setMSAASamples(4);

    RenderSubviewIntent inherited;
    inherited.name = "reflection_probe";
    inherited.view.engineOptions.raytracer().setDenoiseRadius(1);
    inherited.view.engineOptions.raytracer().setDenoiseColorSigma(0.1);
    inherited.view.engineOptions.rasterizer().setMSAASamples(1);

    const RenderEngineOptions inheritedOptions = inherited.resolvedEngineOptions(global);
    ASSERT_TRUE(inheritedOptions.raytracer().samplesPerPixel().has_value());
    ASSERT_TRUE(inheritedOptions.raytracer().denoiser().has_value());
    ASSERT_TRUE(inheritedOptions.raytracer().denoiseRadius().has_value());
    ASSERT_TRUE(inheritedOptions.raytracer().denoiseColorSigma().has_value());
    ASSERT_TRUE(inheritedOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(8, *inheritedOptions.raytracer().samplesPerPixel());
    EXPECT_EQ("bilateral", *inheritedOptions.raytracer().denoiser());
    EXPECT_EQ(1, *inheritedOptions.raytracer().denoiseRadius());
    EXPECT_DOUBLE_EQ(0.1, *inheritedOptions.raytracer().denoiseColorSigma());
    EXPECT_EQ(1, *inheritedOptions.rasterizer().msaaSamples());

    RenderSubviewIntent independent = inherited;
    independent.view.inheritEngineOptions = false;

    const RenderEngineOptions independentOptions = independent.resolvedEngineOptions(global);
    EXPECT_FALSE(independentOptions.raytracer().samplesPerPixel().has_value());
    EXPECT_FALSE(independentOptions.raytracer().denoiser().has_value());
    ASSERT_TRUE(independentOptions.raytracer().denoiseRadius().has_value());
    EXPECT_EQ(1, *independentOptions.raytracer().denoiseRadius());
    ASSERT_TRUE(independentOptions.raytracer().denoiseColorSigma().has_value());
    EXPECT_DOUBLE_EQ(0.1, *independentOptions.raytracer().denoiseColorSigma());
    ASSERT_TRUE(independentOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(1, *independentOptions.rasterizer().msaaSamples());
  }

  TEST(RenderIntent, SampleCountHintUsesDefaultExecutorOptionsFirst) {
    RenderIntent intent;
    intent.engineOptions.raytracer().setSamplesPerPixel(8);
    intent.engineOptions.rasterizer().setMSAASamples(4);

    EXPECT_EQ(8, intent.targetSampleCountHint(1));

    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    EXPECT_EQ(4, intent.targetSampleCountHint(1));

    intent.defaultExecutor = RenderExecutorPreference::Wavefront;
    EXPECT_EQ(8, intent.targetSampleCountHint(1));

    intent.defaultExecutor = RenderExecutorPreference::Wireframe;
    EXPECT_EQ(1, intent.targetSampleCountHint(1));
  }

  TEST(RenderExecutorDefinition, DescribesCompiledBeautyPasses) {
    const auto& raytracer = renderExecutorDefinition(RenderExecutorPreference::Raytracer);
    EXPECT_EQ(RenderExecutorKind::Raytracer, raytracer.kind());
    EXPECT_EQ("raytracer", raytracer.feature());
    EXPECT_EQ("raytrace_beauty", raytracer.beautyPassId());

    const auto& wavefront = renderExecutorDefinition(RenderExecutorPreference::Wavefront);
    EXPECT_EQ(RenderExecutorKind::Wavefront, wavefront.kind());
    EXPECT_EQ("wavefront", wavefront.feature());
    EXPECT_EQ("wavefront_beauty", wavefront.beautyPassId());

    const auto& rasterizer = *renderExecutorDefinition(RenderExecutorKind::Rasterizer);
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, rasterizer.preference());
    EXPECT_EQ("rasterizer", rasterizer.feature());
    EXPECT_EQ("Raster beauty", rasterizer.beautyPassName());
  }

  TEST(RenderIntent, RejectsUnknownPostProcessAA) {
    QJsonObject json;
    json["postProcessAA"] = "mlaa";

    EXPECT_THROW(RenderIntent::fromJson(json), std::runtime_error);
  }

  TEST(RenderPassNode, OwnsReadAndWriteEdgeMutation) {
    RenderPassNode pass;

    pass.addRead("beauty_color");
    pass.addRead("beauty_color");
    pass.addWrite("main_color");
    pass.addWrite("main_color");

    ASSERT_EQ(1u, pass.reads.size());
    EXPECT_EQ("beauty_color", pass.reads.front().resource);
    ASSERT_EQ(1u, pass.writes.size());
    EXPECT_EQ("main_color", pass.writes.front().resource);
    EXPECT_TRUE(pass.readsResource("beauty_color"));
    EXPECT_TRUE(pass.writesResource("main_color"));
    EXPECT_TRUE(pass.supportsResourceDomain(RenderResourceDomain::CPU));
    EXPECT_FALSE(pass.supportsResourceDomain(RenderResourceDomain::GPU));
  }

  TEST(RenderPassNode, SupportsDeclaredResourceDomains) {
    RenderPassNode pass;
    pass.supportedResourceDomains = {RenderResourceDomain::CPU, RenderResourceDomain::GPU};

    EXPECT_TRUE(pass.supportsResourceDomain(RenderResourceDomain::CPU));
    EXPECT_TRUE(pass.supportsResourceDomain(RenderResourceDomain::GPU));
  }
}
