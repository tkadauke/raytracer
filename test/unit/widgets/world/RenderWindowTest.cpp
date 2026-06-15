#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "render/cameras/Camera.h"
#include "widgets/RenderWidget.h"
#include "widgets/world/RenderGraphInspectorWidget.h"
#include "widgets/world/RenderWindow.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QSpinBox>

namespace RenderWindowTest {
  class RenderWindowTest : public ::testing::GuiTest {};

  TEST_F(RenderWindowTest, ShouldInitialize) {
    RenderWindow window;
  }

  TEST_F(RenderWindowTest, ShouldDefaultToNotBusy) {
    // RenderWindow's busy flag tracks whether a render thread is in
    // flight. At construction it's false (no render started yet); pin
    // because the busy state is what stop()/render() guard on.
    RenderWindow window;
    EXPECT_FALSE(window.isBusy());
  }

  TEST_F(RenderWindowTest, ShouldAcceptSetSceneWithRealScene) {
    // Note: setScene is documented as taking a non-null pointer (it
    // immediately calls scene->toRaytracerScene()) — the Modeler only ever
    // passes a real scene. A defensive null-guard would be a
    // reasonable hardening change but is out of scope for this test.
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);
  }

  TEST_F(RenderWindowTest, ShouldReturnNonZeroSizeHint) {
    RenderWindow window;
    auto hint = window.sizeHint();
    EXPECT_GT(hint.width(), 0);
    EXPECT_GT(hint.height(), 0);
  }

  TEST_F(RenderWindowTest, ShouldCompileRasterPostAAIntoRenderGraph) {
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* resolution = window.findChild<QComboBox*>("resolution");
    auto* postAA = window.findChild<QComboBox*>("rasterPostProcessAA");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, resolution);
    ASSERT_NE(nullptr, postAA);

    engineType->setCurrentText("Rasterizer");
    resolution->setCurrentText("40x30");
    postAA->setCurrentText("FXAA");
    window.render();

    auto* renderWidget = window.findChild<RenderWidget*>();
    ASSERT_NE(nullptr, renderWidget);
    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(renderWidget->renderEngine());
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->explicitPlan());
    EXPECT_NE(nullptr, graph->explicitPlan()->findPass("post_fxaa"));

    window.stop();
  }

  TEST_F(RenderWindowTest, ShouldCompileRasterBackendIntoRenderGraph) {
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* backend = window.findChild<QComboBox*>("rasterBackend");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, backend);

    engineType->setCurrentText("Rasterizer");
    backend->setCurrentText("OpenGL");
    QCoreApplication::processEvents();

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);

    const auto plan = graphInspector->effectivePlan();
    const auto* beautyPass = plan.findPass("raster_beauty");
    ASSERT_NE(nullptr, beautyPass);
    const auto* beautyState = engine::graph::RasterBeautyPassState::fromPass(*beautyPass);
    ASSERT_NE(nullptr, beautyState);
    EXPECT_TRUE(beautyState->execution().backend().isOpenGL());
  }

  TEST_F(RenderWindowTest, ShouldBindSceneCamerasForFinalGraphPasses) {
    RenderWindow window;
    Scene scene;
    auto* activeCamera = new PinholeCamera;
    activeCamera->setId("active-camera");
    activeCamera->setPosition(Vector3d(0, 0, -6));
    activeCamera->setTarget(Vector3d::null);
    scene.addChild(activeCamera);

    auto* passCamera = new PinholeCamera;
    passCamera->setId("pass-camera");
    passCamera->setPosition(Vector3d(4, 3, -9));
    passCamera->setTarget(Vector3d(1, 2, 0));
    scene.addChild(passCamera);

    window.setScene(&scene);

    auto* renderWidget = window.findChild<RenderWidget*>();
    ASSERT_NE(nullptr, renderWidget);
    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(renderWidget->renderEngine());
    ASSERT_NE(nullptr, graph);

    engine::graph::RenderPassNode pass;
    pass.sceneView.camera = engine::graph::RenderCameraRef{"pass-camera", std::nullopt};

    auto selectedCamera = graph->cameraForPass(pass);
    ASSERT_NE(nullptr, selectedCamera);
    ASSERT_VECTOR_NEAR(passCamera->position(), selectedCamera->position(), 1e-9);
    ASSERT_VECTOR_NEAR(passCamera->target(), selectedCamera->target(), 1e-9);
  }

  TEST_F(RenderWindowTest, ShouldShowFinalGraphBeforeRendering) {
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);
    EXPECT_NE(nullptr, graphInspector->effectivePlan().findPass("raytrace_beauty"));

    auto* engineType = window.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);
    engineType->setCurrentText("Wireframe");

    EXPECT_NE(nullptr, graphInspector->effectivePlan().findPass("wireframe_beauty"));
  }

  TEST_F(RenderWindowTest, ShouldCompilePathTracerDenoiserOverrideIntoRenderGraph) {
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* directLightSamples = window.findChild<QSpinBox*>("pathTracerDirectLightSamples");
    auto* tracingExecution = window.findChild<QComboBox*>("tracingExecution");
    auto* intersectionBackend = window.findChild<QComboBox*>("wavefrontIntersectionBackend");
    auto* denoiser = window.findChild<QComboBox*>("rayDenoiser");
    auto* radius = window.findChild<QSpinBox*>("rayDenoiseRadius");
    auto* colorSigma = window.findChild<QDoubleSpinBox*>("rayDenoiseColorSigma");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, directLightSamples);
    ASSERT_NE(nullptr, tracingExecution);
    ASSERT_NE(nullptr, intersectionBackend);
    ASSERT_NE(nullptr, denoiser);
    ASSERT_NE(nullptr, radius);
    ASSERT_NE(nullptr, colorSigma);

    engineType->setCurrentText("Path Tracer");
    directLightSamples->setValue(4);
    tracingExecution->setCurrentText("Hybrid");
    intersectionBackend->setCurrentText("GPU");
    denoiser->setCurrentText("Bilateral");
    radius->setValue(5);
    colorSigma->setValue(0.3);
    QCoreApplication::processEvents();

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);
    const auto plan = graphInspector->effectivePlan();
    const auto* beautyPass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, beautyPass);
    const auto* state = engine::graph::RaytracerBeautyPassState::fromPass(*beautyPass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->directLightSamples().has_value());
    EXPECT_EQ(4, *state->directLightSamples());
    ASSERT_TRUE(state->tracingBackend().has_value());
    EXPECT_STREQ("gpu", state->tracingBackend()->id());
    ASSERT_TRUE(state->tracingExecution().has_value());
    EXPECT_EQ(engine::graph::TracingExecutionPreference::Hybrid, *state->tracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("gpu", state->intersectionBackend()->id());
    ASSERT_TRUE(state->denoiser().has_value());
    EXPECT_EQ("bilateral", *state->denoiser());
    ASSERT_TRUE(state->denoiseRadius().has_value());
    EXPECT_EQ(5, *state->denoiseRadius());
    ASSERT_TRUE(state->denoiseColorSigma().has_value());
    EXPECT_DOUBLE_EQ(0.3, *state->denoiseColorSigma());
  }

  TEST_F(RenderWindowTest, ShouldCompileScalarPathTracerScheduleIntoRenderGraph) {
    RenderWindow window;
    Scene scene;
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* schedule = window.findChild<QComboBox*>("pathTracingSchedule");
    auto* directLightSamples = window.findChild<QSpinBox*>("pathTracerDirectLightSamples");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, schedule);
    ASSERT_NE(nullptr, directLightSamples);

    engineType->setCurrentText("Path Tracer");
    schedule->setCurrentText("Scalar");
    directLightSamples->setValue(3);
    QCoreApplication::processEvents();

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);
    const auto plan = graphInspector->effectivePlan();
    const auto* beautyPass = plan.findPass("raytrace_beauty");
    ASSERT_NE(nullptr, beautyPass);
    EXPECT_EQ(engine::graph::RenderExecutorKind::Raytracer, beautyPass->executor);
    EXPECT_EQ(nullptr, plan.findPass("wavefront_beauty"));
    const auto* state = engine::graph::RaytracerBeautyPassState::fromPass(*beautyPass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->directLightSamples().has_value());
    EXPECT_EQ(3, *state->directLightSamples());
  }

  TEST_F(RenderWindowTest, ShouldOverrideHiddenBackendForBroadTracingExecution) {
    RenderWindow window;
    Scene scene;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setIntersectionBackend("gpu");
    scene.setRenderIntent(intent);
    window.setScene(&scene);

    auto* tracingExecution = window.findChild<QComboBox*>("tracingExecution");
    ASSERT_NE(nullptr, tracingExecution);

    tracingExecution->setCurrentText("CPU");
    QCoreApplication::processEvents();

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);
    const auto plan = graphInspector->effectivePlan();
    const auto* beautyPass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, beautyPass);
    const auto* state = engine::graph::RaytracerBeautyPassState::fromPass(*beautyPass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->tracingExecution().has_value());
    EXPECT_EQ(engine::graph::TracingExecutionPreference::CPU, *state->tracingExecution());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("cpu", state->intersectionBackend()->id());
  }

  TEST_F(RenderWindowTest, ShouldInitializeFinalDialogAndGraphFromSceneRenderIntent) {
    RenderWindow window;
    Scene scene;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    intent.engineOptions.raytracer().setSampler("Jittered");
    intent.engineOptions.raytracer().setSamplesPerPixel(9);
    intent.engineOptions.raytracer().setDirectLightSamples(6);
    intent.engineOptions.raytracer().setIntersectionBackend("cpu");
    intent.engineOptions.raytracer().setDenoiser("box");
    intent.engineOptions.raytracer().setDenoiseRadius(3);
    scene.setRenderIntent(intent);
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* sampler = window.findChild<QComboBox*>("samplerType");
    auto* samples = window.findChild<QSpinBox*>("samplesPerPixel");
    auto* directLightSamples = window.findChild<QSpinBox*>("pathTracerDirectLightSamples");
    auto* intersectionBackend = window.findChild<QComboBox*>("wavefrontIntersectionBackend");
    auto* denoiser = window.findChild<QComboBox*>("rayDenoiser");
    auto* radius = window.findChild<QSpinBox*>("rayDenoiseRadius");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, sampler);
    ASSERT_NE(nullptr, samples);
    ASSERT_NE(nullptr, directLightSamples);
    ASSERT_NE(nullptr, intersectionBackend);
    ASSERT_NE(nullptr, denoiser);
    ASSERT_NE(nullptr, radius);
    EXPECT_EQ(QString("Path Tracer"), engineType->currentText());
    EXPECT_EQ(QString("Jittered"), sampler->currentText());
    EXPECT_EQ(9, samples->value());
    EXPECT_EQ(6, directLightSamples->value());
    EXPECT_EQ(QString("CPU"), intersectionBackend->currentText());
    EXPECT_EQ(QString("Box"), denoiser->currentText());
    EXPECT_EQ(3, radius->value());

    auto* graphInspector = window.findChild<RenderGraphInspectorWidget*>();
    ASSERT_NE(nullptr, graphInspector);
    const auto plan = graphInspector->effectivePlan();
    const auto* beautyPass = plan.findPass("wavefront_beauty");
    ASSERT_NE(nullptr, beautyPass);
    const auto* state = engine::graph::RaytracerBeautyPassState::fromPass(*beautyPass);
    ASSERT_NE(nullptr, state);
    ASSERT_TRUE(state->integrator().has_value());
    EXPECT_EQ("pathtracer", *state->integrator());
    ASSERT_TRUE(state->sampler().has_value());
    EXPECT_EQ("Jittered", *state->sampler());
    ASSERT_TRUE(state->samplesPerPixel().has_value());
    EXPECT_EQ(9, *state->samplesPerPixel());
    ASSERT_TRUE(state->directLightSamples().has_value());
    EXPECT_EQ(6, *state->directLightSamples());
    ASSERT_TRUE(state->intersectionBackend().has_value());
    EXPECT_STREQ("cpu", state->intersectionBackend()->id());
    ASSERT_TRUE(state->denoiser().has_value());
    EXPECT_EQ("box", *state->denoiser());
    ASSERT_TRUE(state->denoiseRadius().has_value());
    EXPECT_EQ(3, *state->denoiseRadius());
  }

  TEST_F(RenderWindowTest, ShouldCompileRasterShadowsIntoRenderGraph) {
    RenderWindow window;
    Scene scene;
    scene.addChild(new Sphere);
    scene.addChild(new PointLight);
    window.setScene(&scene);

    auto* engineType = window.findChild<QComboBox*>("engineType");
    auto* resolution = window.findChild<QComboBox*>("resolution");
    auto* shadowMaps = window.findChild<QCheckBox*>("rasterShadowMaps");
    auto* shadowMapSize = window.findChild<QSpinBox*>("rasterShadowMapSize");
    auto* cascadeCount = window.findChild<QSpinBox*>("rasterShadowCascadeCount");
    auto* shadowBias = window.findChild<QDoubleSpinBox*>("rasterShadowBias");
    auto* filterRadius = window.findChild<QSpinBox*>("rasterShadowFilterRadius");
    auto* filterMode = window.findChild<QComboBox*>("rasterShadowFilterMode");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, resolution);
    ASSERT_NE(nullptr, shadowMaps);
    ASSERT_NE(nullptr, shadowMapSize);
    ASSERT_NE(nullptr, cascadeCount);
    ASSERT_NE(nullptr, shadowBias);
    ASSERT_NE(nullptr, filterRadius);
    ASSERT_NE(nullptr, filterMode);

    engineType->setCurrentText("Rasterizer");
    resolution->setCurrentText("40x30");
    shadowMaps->setChecked(true);
    shadowMapSize->setValue(512);
    cascadeCount->setValue(3);
    shadowBias->setValue(0.125);
    filterRadius->setValue(2);
    filterMode->setCurrentText("PCSS");
    window.render();

    auto* renderWidget = window.findChild<RenderWidget*>();
    ASSERT_NE(nullptr, renderWidget);
    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(renderWidget->renderEngine());
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, graph->explicitPlan());

    const auto* shadowPass = graph->explicitPlan()->findPass("raster_preview_shadows");
    ASSERT_NE(nullptr, shadowPass);
    ASSERT_EQ(1u, shadowPass->writes.size());
    EXPECT_EQ("preview_shadow_map", shadowPass->writes[0].resource);

    const auto* beautyPass = graph->explicitPlan()->findPass("raster_beauty");
    ASSERT_NE(nullptr, beautyPass);
    ASSERT_EQ(1u, beautyPass->reads.size());
    EXPECT_EQ("preview_shadow_map", beautyPass->reads[0].resource);

    const auto* shadowState = engine::graph::RasterShadowPassState::fromPass(*shadowPass);
    ASSERT_NE(nullptr, shadowState);
    const QJsonObject shadows = shadowState->toJson().value("shadows").toObject();
    EXPECT_TRUE(shadows.value("enabled").toBool());
    EXPECT_EQ(512, shadows.value("mapSize").toInt());
    EXPECT_EQ(3, shadows.value("cascadeCount").toInt());
    EXPECT_EQ(0.125, shadows.value("bias").toDouble());
    EXPECT_EQ(2, shadows.value("filterRadius").toInt());
    EXPECT_EQ(QString("pcss"), shadows.value("filterMode").toString());

    window.stop();
  }
}
