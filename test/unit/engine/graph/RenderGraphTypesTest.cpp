#include <gtest/gtest.h>

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

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderExecutorPreference::Rasterizer, intent.defaultExecutor);
    EXPECT_EQ(RenderViewMode::Beauty, intent.defaultViewMode);
    EXPECT_EQ("toon", intent.defaultShadingProfile.name);
    EXPECT_TRUE(intent.enableAutomaticFeatures);
    EXPECT_FALSE(intent.enableWireframeOverlay);
    EXPECT_EQ(RenderPostProcessAA::None, intent.postProcessAA);
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
    json["defaultExecutor"] = "rasterizer";
    json["postProcessAA"] = "fxaa";

    const RenderIntent intent = RenderIntent::fromJson(json);

    EXPECT_EQ(RenderPostProcessAA::FXAA, intent.postProcessAA);
    EXPECT_TRUE(intent.usesGraphImagePostProcessAA());
  }

  TEST(RenderIntent, RejectsUnknownPostProcessAA) {
    QJsonObject json;
    json["postProcessAA"] = "mlaa";

    EXPECT_THROW(RenderIntent::fromJson(json), std::runtime_error);
  }
}
