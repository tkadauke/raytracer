#include <gtest/gtest.h>

#include "world/objects/RenderIntentElement.h"
#include "world/objects/Scene.h"

#include <QJsonObject>

namespace RenderIntentElementTest {
  RenderIntentElement* renderIntentElement(Scene& scene) {
    for (Element* child : scene.childElements()) {
      if (auto* intent = qobject_cast<RenderIntentElement*>(child))
        return intent;
    }
    return nullptr;
  }

  TEST(RenderIntentElement, SceneOwnsGeneratedVisibleIntentEditor) {
    Scene scene;

    auto* intent = renderIntentElement(scene);

    ASSERT_NE(nullptr, intent);
    EXPECT_TRUE(intent->isGenerated());
    EXPECT_TRUE(intent->displayInSceneModel());
    EXPECT_FALSE(scene.hasRenderIntent());
  }

  TEST(RenderIntentElement, EditsSceneRenderIntent) {
    Scene scene;
    auto* intent = renderIntentElement(scene);
    ASSERT_NE(nullptr, intent);

    intent->setDefaultEngine("rasterizer");
    intent->setViewMode("depth");
    intent->setPreviewShadows(true);
    intent->setPostProcessAA("smaa");
    intent->setRasterizerMSAASamples(4);
    intent->setRasterizerShadowMapSize(128);
    intent->setRaytracerSampler("Jittered");
    intent->setRaytracerSamplesPerPixel(9);
    intent->setWireframeLod(2);

    ASSERT_TRUE(scene.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Rasterizer,
              scene.renderIntent().defaultExecutor);
    EXPECT_EQ(engine::graph::RenderViewMode::Depth, scene.renderIntent().defaultViewMode);
    EXPECT_TRUE(scene.renderIntent().enablePreviewShadows);
    EXPECT_EQ(engine::graph::RenderPostProcessAA::SMAA, scene.renderIntent().postProcessAA);
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(4, *scene.renderIntent().engineOptions.rasterizer().msaaSamples());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().shadowMapSize().has_value());
    EXPECT_EQ(128, *scene.renderIntent().engineOptions.rasterizer().shadowMapSize());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().sampler().has_value());
    EXPECT_EQ("Jittered", *scene.renderIntent().engineOptions.raytracer().sampler());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().samplesPerPixel().has_value());
    EXPECT_EQ(9, *scene.renderIntent().engineOptions.raytracer().samplesPerPixel());
    ASSERT_TRUE(scene.renderIntent().engineOptions.wireframe().lod().has_value());
    EXPECT_EQ(2, *scene.renderIntent().engineOptions.wireframe().lod());
  }

  TEST(RenderIntentElement, GeneratedEditorIsNotSerializedAsChild) {
    Scene scene;
    auto* intent = renderIntentElement(scene);
    ASSERT_NE(nullptr, intent);
    intent->setDefaultEngine("wireframe");

    QJsonObject json;
    scene.write(json);

    ASSERT_TRUE(json["renderIntent"].isObject());
    EXPECT_FALSE(json.contains("children"));
  }
}
