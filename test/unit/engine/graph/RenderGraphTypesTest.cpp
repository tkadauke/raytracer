#include <gtest/gtest.h>

#include "engine/graph/RenderExecutor.h"
#include "engine/graph/RenderGraphTypes.h"

#include <QJsonArray>
#include <QJsonObject>

#include <optional>
#include <stdexcept>

namespace RenderGraphTypesTest {
  using namespace engine::graph;

  TEST(RenderIntent, SerializesToSceneJsonShape) {
    RenderIntent intent;
    intent.defaultExecutor = RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = RenderViewMode::Depth;
    intent.defaultShadingProfile.name = "toon";
    intent.defaultShadingProfile.parameters["levels"] = 4;
    intent.defaultCamera = RenderCameraRef{"camera-a", std::nullopt};
    intent.enableAutomaticFeatures = false;
    intent.enableWireframeOverlay = true;
    intent.enablePreviewShadows = true;
    intent.postProcessAA = RenderPostProcessAA::SMAA;
    intent.exportedAOVs = {RenderViewMode::Depth, RenderViewMode::Normal};
    intent.viewOverrides.push_back({SceneSelector::tag("debug"),
                                    RenderExecutorPreference::Wireframe, RenderViewMode::Wireframe,
                                    std::nullopt, std::nullopt});

    const QJsonObject json = intent.toJson();

    EXPECT_EQ("rasterizer", json["defaultExecutor"].toString().toStdString());
    EXPECT_EQ("depth", json["defaultViewMode"].toString().toStdString());
    EXPECT_FALSE(json["enableAutomaticFeatures"].toBool());
    EXPECT_TRUE(json["enableWireframeOverlay"].toBool());
    EXPECT_TRUE(json["enablePreviewShadows"].toBool());
    EXPECT_EQ("smaa", json["postProcessAA"].toString().toStdString());
    const auto exportedAOVs = json["exportedAOVs"].toArray();
    ASSERT_EQ(2, exportedAOVs.size());
    EXPECT_EQ("depth", exportedAOVs.at(0).toString().toStdString());
    EXPECT_EQ("normal", exportedAOVs.at(1).toString().toStdString());

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

    QJsonObject selector;
    selector["kind"] = "object_name";
    selector["value"] = "Monitor";
    QJsonObject viewOverride;
    viewOverride["selector"] = selector;
    viewOverride["executor"] = "wireframe";
    json["viewOverrides"] = QJsonArray{viewOverride};
    json["exportedAOVs"] = QJsonArray{"depth", "world_position"};

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderExecutorPreference::Rasterizer, intent.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Beauty, intent.defaultViewMode);
    EXPECT_EQ("toon", intent.defaultShadingProfile.name);
    EXPECT_TRUE(intent.enableAutomaticFeatures);
    EXPECT_FALSE(intent.enableWireframeOverlay);
    EXPECT_EQ(RenderPostProcessAA::None, intent.postProcessAA);
    ASSERT_EQ(2u, intent.exportedAOVs.size());
    EXPECT_EQ(RenderViewMode::Depth, intent.exportedAOVs[0]);
    EXPECT_EQ(RenderViewMode::WorldPosition, intent.exportedAOVs[1]);
    ASSERT_EQ(1u, intent.viewOverrides.size());
    EXPECT_EQ(SceneSelector::Kind::ObjectName, intent.viewOverrides.front().selector.kind);
    EXPECT_EQ("Monitor", intent.viewOverrides.front().selector.value);
    ASSERT_TRUE(intent.viewOverrides.front().executor.has_value());
    EXPECT_EQ(RenderExecutorPreference::Wireframe, *intent.viewOverrides.front().executor);
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
    intent.viewOverrides.push_back(wholeFrame);

    RenderViewOverride tagOverride;
    tagOverride.selector = SceneSelector::tag("debug");
    tagOverride.executor = RenderExecutorPreference::Wireframe;
    intent.viewOverrides.push_back(tagOverride);

    const RenderIntent effective = intent.withWholeFrameOverridesApplied();

    EXPECT_TRUE(wholeFrame.appliesToWholeFrame());
    EXPECT_FALSE(tagOverride.appliesToWholeFrame());
    EXPECT_EQ(RenderExecutorPreference::Rasterizer, effective.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Depth, effective.defaultViewMode);
    EXPECT_EQ("clay", effective.defaultShadingProfile.name);
    ASSERT_TRUE(effective.defaultCamera.has_value());
    ASSERT_TRUE(effective.defaultCamera->sceneCameraId.has_value());
    EXPECT_EQ("inspection-camera", *effective.defaultCamera->sceneCameraId);
    ASSERT_EQ(1u, effective.viewOverrides.size());
    EXPECT_EQ(SceneSelector::Kind::Tag, effective.viewOverrides.front().selector.kind);
  }

  TEST(RenderExecutorDefinition, DescribesCompiledBeautyPasses) {
    const auto& raytracer = renderExecutorDefinition(RenderExecutorPreference::Raytracer);
    EXPECT_EQ(RenderExecutorKind::Raytracer, raytracer.kind());
    EXPECT_EQ("raytracer", raytracer.feature());
    EXPECT_EQ("raytrace_beauty", raytracer.beautyPassId());

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
}
