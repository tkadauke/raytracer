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
    EXPECT_EQ(QString("Render Settings"), intent->displayName());
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
    intent->setRasterizerBackend("opengl");
    intent->setRasterizerVisibilityCulling("auto");
    intent->setRasterizerDepthPrepass("auto");
    intent->setRasterizerLod(3);
    intent->setRasterizerTessellationQuality("final");
    intent->setRasterizerMaxScreenSpaceError(0.25);
    intent->setRasterizerMSAASamples(4);
    intent->setRasterizerShadowMapSize(128);
    intent->setRaytracerIntegrator("path_tracer");
    intent->setRaytracerSampler("Jittered");
    intent->setRaytracerSamplesPerPixel(9);
    intent->setWavefrontConvergence(true);
    intent->setWavefrontConvergenceActiveFraction(0.25);
    intent->setWavefrontConvergenceRmsDelta(0.005);
    intent->setWireframeLod(2);

    ASSERT_TRUE(scene.hasRenderIntent());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::Rasterizer,
              scene.renderIntent().defaultExecutor);
    EXPECT_EQ(engine::graph::RenderViewMode::Depth, scene.renderIntent().defaultViewMode);
    EXPECT_TRUE(scene.renderIntent().enablePreviewShadows);
    EXPECT_EQ(engine::graph::RenderPostProcessAA::SMAA, scene.renderIntent().postProcessAA);
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().backend().has_value());
    EXPECT_TRUE(scene.renderIntent().engineOptions.rasterizer().backend()->isOpenGL());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().visibilityCulling().has_value());
    EXPECT_EQ(engine::graph::RenderVisibilityCulling::Auto,
              *scene.renderIntent().engineOptions.rasterizer().visibilityCulling());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().depthPrepass().has_value());
    EXPECT_EQ("auto", *scene.renderIntent().engineOptions.rasterizer().depthPrepass());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().lod().has_value());
    EXPECT_EQ(3, *scene.renderIntent().engineOptions.rasterizer().lod());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().tessellationQuality().has_value());
    EXPECT_EQ("final", *scene.renderIntent().engineOptions.rasterizer().tessellationQuality());
    ASSERT_TRUE(
      scene.renderIntent().engineOptions.rasterizer().maximumScreenSpaceError().has_value());
    EXPECT_DOUBLE_EQ(0.25,
                     *scene.renderIntent().engineOptions.rasterizer().maximumScreenSpaceError());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().msaaSamples().has_value());
    EXPECT_EQ(4, *scene.renderIntent().engineOptions.rasterizer().msaaSamples());
    ASSERT_TRUE(scene.renderIntent().engineOptions.rasterizer().shadowMapSize().has_value());
    EXPECT_EQ(128, *scene.renderIntent().engineOptions.rasterizer().shadowMapSize());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().sampler().has_value());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().integrator().has_value());
    EXPECT_EQ("pathtracer", *scene.renderIntent().engineOptions.raytracer().integrator());
    EXPECT_EQ("Jittered", *scene.renderIntent().engineOptions.raytracer().sampler());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().samplesPerPixel().has_value());
    EXPECT_EQ(9, *scene.renderIntent().engineOptions.raytracer().samplesPerPixel());
    ASSERT_TRUE(scene.renderIntent().engineOptions.raytracer().convergenceEnabled().has_value());
    EXPECT_TRUE(*scene.renderIntent().engineOptions.raytracer().convergenceEnabled());
    ASSERT_TRUE(scene.renderIntent()
                  .engineOptions.raytracer()
                  .convergenceActiveSampleFractionThreshold()
                  .has_value());
    EXPECT_DOUBLE_EQ(
      0.25,
      *scene.renderIntent().engineOptions.raytracer().convergenceActiveSampleFractionThreshold());
    ASSERT_TRUE(scene.renderIntent()
                  .engineOptions.raytracer()
                  .convergenceRadianceDeltaRmsThreshold()
                  .has_value());
    EXPECT_DOUBLE_EQ(
      0.005,
      *scene.renderIntent().engineOptions.raytracer().convergenceRadianceDeltaRmsThreshold());
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

  TEST(RenderIntentElement, ExposesChoicesAndHumanReadableLabels) {
    Scene scene;
    auto* intent = renderIntentElement(scene);
    ASSERT_NE(nullptr, intent);

    EXPECT_EQ(QString("Default Engine"), intent->propertyDisplayName("defaultEngine"));
    EXPECT_TRUE(intent->propertyChoices("defaultEngine").contains("wavefront"));
    EXPECT_TRUE(intent->propertyChoices("defaultEngine").contains("rasterizer"));
    EXPECT_TRUE(intent->propertyChoices("viewMode").contains("stencil_composite"));
    EXPECT_TRUE(intent->propertyChoices("rasterizerBackend").contains("opengl"));
    EXPECT_TRUE(intent->propertyChoices("rasterizerVisibilityCulling").contains("auto"));
    EXPECT_TRUE(intent->propertyChoices("rasterizerDepthPrepass").contains("auto"));
    EXPECT_TRUE(intent->propertyChoices("rasterizerTessellationQuality").contains("final"));
    EXPECT_TRUE(intent->propertyChoices("raytracerIntegrator").contains("pathtracer"));
    EXPECT_FALSE(intent->propertyChoices("viewMode").contains("raster_depth_test_count"));
    intent->setDefaultEngine("rasterizer");
    EXPECT_TRUE(intent->propertyChoices("viewMode").contains("raster_depth_test_count"));
    EXPECT_EQ(QString("Stencil Composite"),
              intent->propertyChoiceDisplayName("viewMode", "stencil_composite"));
    EXPECT_EQ(QString("Raster Depth-Test Count"),
              intent->propertyChoiceDisplayName("viewMode", "raster_depth_test_count"));
    EXPECT_EQ(QString("OpenGL"), intent->propertyChoiceDisplayName("rasterizerBackend", "opengl"));
    EXPECT_EQ(QString("Visibility Culling"),
              intent->propertyDisplayName("rasterizerVisibilityCulling"));
    EXPECT_EQ(QString("Depth Prepass"), intent->propertyDisplayName("rasterizerDepthPrepass"));
    EXPECT_EQ(QString("Tessellation Quality"),
              intent->propertyDisplayName("rasterizerTessellationQuality"));
    EXPECT_EQ(QString("Integrator"), intent->propertyDisplayName("raytracerIntegrator"));
    EXPECT_EQ(QString("Convergence Stop"), intent->propertyDisplayName("wavefrontConvergence"));
    EXPECT_EQ(QString("Path Tracer"),
              intent->propertyChoiceDisplayName("raytracerIntegrator", "pathtracer"));
    EXPECT_EQ(QString("Wavefront"),
              intent->propertyChoiceDisplayName("defaultEngine", "wavefront"));
    EXPECT_EQ(QString("Final"),
              intent->propertyChoiceDisplayName("rasterizerTessellationQuality", "final"));
    EXPECT_EQ(QString("Auto"),
              intent->propertyChoiceDisplayName("rasterizerVisibilityCulling", "auto"));
    EXPECT_EQ(QString("Auto"), intent->propertyChoiceDisplayName("rasterizerDepthPrepass", "auto"));
    EXPECT_EQ((QList<int>{1, 2, 4, 8}), intent->propertyIntChoices("rasterizerMSAASamples"));
  }

  TEST(RenderIntentElement, ExposesNumericRangesForEditorControls) {
    Scene scene;
    auto* intent = renderIntentElement(scene);
    ASSERT_NE(nullptr, intent);

    ASSERT_TRUE(intent->propertyIntRange("raytracerSamplesPerPixel").has_value());
    EXPECT_EQ(1, intent->propertyIntRange("raytracerSamplesPerPixel")->first);
    ASSERT_TRUE(intent->propertyIntRange("rasterizerShadowMapSize").has_value());
    EXPECT_EQ(8192, intent->propertyIntRange("rasterizerShadowMapSize")->second);
    ASSERT_TRUE(intent->propertyDoubleRange("rasterizerShadowBias").has_value());
    EXPECT_DOUBLE_EQ(0.0, intent->propertyDoubleRange("rasterizerShadowBias")->first);
    ASSERT_TRUE(intent->propertyDoubleRange("rasterizerMaxScreenSpaceError").has_value());
    EXPECT_DOUBLE_EQ(128.0, intent->propertyDoubleRange("rasterizerMaxScreenSpaceError")->second);
    ASSERT_TRUE(intent->propertyDoubleRange("wavefrontConvergenceActiveFraction").has_value());
    EXPECT_DOUBLE_EQ(1.0,
                     intent->propertyDoubleRange("wavefrontConvergenceActiveFraction")->second);
    ASSERT_TRUE(intent->propertyDoubleRange("wavefrontConvergenceRmsDelta").has_value());
    EXPECT_DOUBLE_EQ(0.0, intent->propertyDoubleRange("wavefrontConvergenceRmsDelta")->first);
  }

  TEST(RenderIntentElement, FiltersEngineSpecificProperties) {
    Scene scene;
    auto* intent = renderIntentElement(scene);
    ASSERT_NE(nullptr, intent);

    EXPECT_TRUE(intent->isPropertyVisible("raytracerSampler"));
    EXPECT_TRUE(intent->isPropertyVisible("raytracerIntegrator"));
    EXPECT_FALSE(intent->isPropertyVisible("raytracerViewPlane"));
    EXPECT_FALSE(intent->isPropertyVisible("raytracerThreads"));
    EXPECT_FALSE(intent->isPropertyVisible("raytracerQueueSize"));
    EXPECT_FALSE(intent->isPropertyVisible("wavefrontConvergence"));
    EXPECT_FALSE(intent->isPropertyVisible("rasterizerLod"));

    intent->setDefaultEngine("rasterizer");
    EXPECT_FALSE(intent->isPropertyVisible("raytracerSampler"));
    EXPECT_FALSE(intent->isPropertyVisible("raytracerIntegrator"));
    EXPECT_TRUE(intent->isPropertyVisible("rasterizerLod"));
    EXPECT_TRUE(intent->isPropertyVisible("rasterizerVisibilityCulling"));
    EXPECT_TRUE(intent->isPropertyVisible("rasterizerDepthPrepass"));
    EXPECT_TRUE(intent->isPropertyVisible("previewShadows"));
    EXPECT_FALSE(intent->isPropertyVisible("rasterizerShadowMapSize"));

    intent->setDefaultEngine("wavefront");
    EXPECT_TRUE(intent->isPropertyVisible("raytracerSampler"));
    EXPECT_TRUE(intent->isPropertyVisible("raytracerIntegrator"));
    EXPECT_TRUE(intent->isPropertyVisible("wavefrontConvergence"));
    EXPECT_FALSE(intent->isPropertyVisible("wavefrontConvergenceRmsDelta"));
    EXPECT_FALSE(intent->isPropertyVisible("rasterizerLod"));
    intent->setWavefrontConvergence(true);
    EXPECT_TRUE(intent->isPropertyVisible("wavefrontConvergenceRmsDelta"));

    intent->setDefaultEngine("rasterizer");
    intent->setPreviewShadows(true);
    EXPECT_TRUE(intent->isPropertyVisible("rasterizerShadowMapSize"));
  }
}
