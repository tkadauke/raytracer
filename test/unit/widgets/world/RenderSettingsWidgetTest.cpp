#include <gtest/gtest.h>

#include "engine/graph/RenderGraphTypes.h"
#include "render/RayFamilyQueuePolicy.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSpinBox>
#include <QThread>

namespace RenderSettingsWidgetTest {
  class RenderSettingsWidgetTest : public ::testing::GuiTest {};

  TEST_F(RenderSettingsWidgetTest, ShouldInitialize) {
    RenderSettingsWidget widget;
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultSamplerToRegular) {
    // The constructor starts in Raytracer mode; pin the ray-family baseline
    // default separately from the Path Tracer-specific default below.
    RenderSettingsWidget widget;
    EXPECT_EQ(QString("Regular"), widget.sampler());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldExposeHaltonSampler) {
    RenderSettingsWidget widget;
    auto samplerType = widget.findChild<QComboBox*>("samplerType");
    ASSERT_NE(nullptr, samplerType);
    EXPECT_NE(-1, samplerType->findText("Halton"));
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultPathTracerSamplerToHalton) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);

    engineType->setCurrentText("Path Tracer");

    EXPECT_EQ(QString("Halton"), widget.sampler());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldRestoreRaytracerSamplerDefaultWhenUnmodified) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);

    engineType->setCurrentText("Path Tracer");
    engineType->setCurrentText("Raytracer");

    EXPECT_EQ(QString("Regular"), widget.sampler());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldPreserveManualSamplerChoiceAcrossEngineChanges) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto samplerType = widget.findChild<QComboBox*>("samplerType");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, samplerType);

    samplerType->setCurrentText("Jittered");
    engineType->setCurrentText("Path Tracer");
    engineType->setCurrentText("Raytracer");

    EXPECT_EQ(QString("Jittered"), widget.sampler());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultViewPlaneToPointInterlaced) {
    RenderSettingsWidget widget;
    EXPECT_EQ(QString("PointInterlacedViewPlane"), widget.viewPlane());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRenderThreadsToIdealThreadCount) {
    // Defaults track the host CPU's reported ideal thread count rather
    // than a hard-coded number, so the test can't pin a specific value
    // — assert consistency with QThread::idealThreadCount() at the time
    // of widget construction.
    RenderSettingsWidget widget;
    EXPECT_EQ(QThread::idealThreadCount(), widget.renderThreads());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultQueueSizeToRayFamilyPolicy) {
    RenderSettingsWidget widget;
    const QSize size = widget.resolution();
    EXPECT_EQ(render::RayFamilyQueuePolicy(size.width(), size.height(), widget.samplesPerPixel(),
                                           widget.renderThreads())
                .queueSize(),
              widget.queueSize());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldHideInternalExecutionControls) {
    RenderSettingsWidget widget;

    auto* viewPlane = widget.findChild<QComboBox*>("viewPlaneType");
    auto* threads = widget.findChild<QSpinBox*>("renderThreads");
    auto* queueSize = widget.findChild<QSpinBox*>("queueSize");
    ASSERT_NE(nullptr, viewPlane);
    ASSERT_NE(nullptr, threads);
    ASSERT_NE(nullptr, queueSize);
    EXPECT_TRUE(viewPlane->isHidden());
    EXPECT_TRUE(threads->isHidden());
    EXPECT_TRUE(queueSize->isHidden());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReturnPositiveSamplesPerPixel) {
    RenderSettingsWidget widget;
    EXPECT_GT(widget.samplesPerPixel(), 0);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultPathTracerSamplesPerPixelToSixtyFour) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);

    engineType->setCurrentText("Path Tracer");

    EXPECT_EQ(64, widget.samplesPerPixel());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldRestoreRaytracerSamplesPerPixelDefaultWhenUnmodified) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);

    engineType->setCurrentText("Path Tracer");
    engineType->setCurrentText("Raytracer");

    EXPECT_EQ(1, widget.samplesPerPixel());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldPreserveManualSamplesPerPixelAcrossEngineChanges) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto samplesPerPixel = widget.findChild<QSpinBox*>("samplesPerPixel");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, samplesPerPixel);

    samplesPerPixel->setValue(7);
    engineType->setCurrentText("Path Tracer");
    engineType->setCurrentText("Raytracer");

    EXPECT_EQ(7, widget.samplesPerPixel());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReturnPositiveMaxRecursionDepth) {
    RenderSettingsWidget widget;
    EXPECT_GT(widget.maxRecursionDepth(), 0);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRayDenoiserToSceneSettings) {
    RenderSettingsWidget widget;

    EXPECT_FALSE(widget.denoiserOverrideEnabled());
    EXPECT_EQ(QString("Scene settings"), widget.denoiser());
    EXPECT_EQ(2, widget.denoiseRadius());
    EXPECT_DOUBLE_EQ(0.1, widget.denoiseColorSigma());
    EXPECT_EQ(QString("Auto"), widget.tracingExecution());
    EXPECT_EQ(QString("Auto"), widget.wavefrontIntersectionBackend());
    EXPECT_EQ(1, widget.directLightSamples());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldExposeTracingExecutionValues) {
    RenderSettingsWidget widget;
    auto tracingExecution = widget.findChild<QComboBox*>("tracingExecution");
    ASSERT_NE(nullptr, tracingExecution);

    EXPECT_NE(-1, tracingExecution->findText("Auto"));
    EXPECT_NE(-1, tracingExecution->findText("CPU"));
    EXPECT_NE(-1, tracingExecution->findText("Hybrid"));
    EXPECT_NE(-1, tracingExecution->findText("GPU"));

    tracingExecution->setCurrentText("Hybrid");
    EXPECT_EQ(QString("Hybrid"), widget.tracingExecution());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReadRayDenoiserControls) {
    RenderSettingsWidget widget;
    auto denoiser = widget.findChild<QComboBox*>("rayDenoiser");
    auto radius = widget.findChild<QSpinBox*>("rayDenoiseRadius");
    auto colorSigma = widget.findChild<QDoubleSpinBox*>("rayDenoiseColorSigma");
    ASSERT_NE(nullptr, denoiser);
    ASSERT_NE(nullptr, radius);
    ASSERT_NE(nullptr, colorSigma);

    denoiser->setCurrentText("Bilateral");
    radius->setValue(4);
    colorSigma->setValue(0.25);

    EXPECT_TRUE(widget.denoiserOverrideEnabled());
    EXPECT_EQ(QString("Bilateral"), widget.denoiser());
    EXPECT_EQ(4, widget.denoiseRadius());
    EXPECT_DOUBLE_EQ(0.25, widget.denoiseColorSigma());

    denoiser->setCurrentText("None");

    EXPECT_TRUE(widget.denoiserOverrideEnabled());
    EXPECT_EQ(QString("None"), widget.denoiser());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowPathTracerControlsBySchedule) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto schedule = widget.findChild<QComboBox*>("pathTracingSchedule");
    auto tracingExecution = widget.findChild<QComboBox*>("tracingExecution");
    auto intersectionBackend = widget.findChild<QComboBox*>("wavefrontIntersectionBackend");
    auto directLightSamples = widget.findChild<QSpinBox*>("pathTracerDirectLightSamples");
    auto denoiser = widget.findChild<QComboBox*>("rayDenoiser");
    auto radius = widget.findChild<QSpinBox*>("rayDenoiseRadius");
    auto colorSigma = widget.findChild<QDoubleSpinBox*>("rayDenoiseColorSigma");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, schedule);
    ASSERT_NE(nullptr, tracingExecution);
    ASSERT_NE(nullptr, intersectionBackend);
    ASSERT_NE(nullptr, directLightSamples);
    ASSERT_NE(nullptr, denoiser);
    ASSERT_NE(nullptr, radius);
    ASSERT_NE(nullptr, colorSigma);

    EXPECT_TRUE(directLightSamples->isHidden());
    EXPECT_TRUE(tracingExecution->isHidden());
    EXPECT_TRUE(intersectionBackend->isHidden());
    EXPECT_TRUE(denoiser->isHidden());
    EXPECT_TRUE(radius->isHidden());
    EXPECT_TRUE(colorSigma->isHidden());

    engineType->setCurrentText("Path Tracer");
    EXPECT_FALSE(schedule->isHidden());
    EXPECT_FALSE(directLightSamples->isHidden());
    EXPECT_FALSE(tracingExecution->isHidden());
    EXPECT_TRUE(intersectionBackend->isHidden());
    EXPECT_FALSE(denoiser->isHidden());
    EXPECT_TRUE(radius->isHidden());
    EXPECT_TRUE(colorSigma->isHidden());

    tracingExecution->setCurrentText("Hybrid");
    EXPECT_FALSE(intersectionBackend->isHidden());

    tracingExecution->setCurrentText("CPU");
    EXPECT_TRUE(intersectionBackend->isHidden());

    tracingExecution->setCurrentText("GPU");
    EXPECT_TRUE(intersectionBackend->isHidden());

    tracingExecution->setCurrentText("Hybrid");
    denoiser->setCurrentText("Box");
    EXPECT_FALSE(radius->isHidden());
    EXPECT_TRUE(colorSigma->isHidden());

    denoiser->setCurrentText("Bilateral");
    EXPECT_FALSE(radius->isHidden());
    EXPECT_FALSE(colorSigma->isHidden());

    schedule->setCurrentText("Scalar");
    EXPECT_FALSE(directLightSamples->isHidden());
    EXPECT_FALSE(tracingExecution->isHidden());
    EXPECT_TRUE(intersectionBackend->isHidden());
    EXPECT_TRUE(denoiser->isHidden());
    EXPECT_TRUE(radius->isHidden());
    EXPECT_TRUE(colorSigma->isHidden());

    engineType->setCurrentText("Raytracer");
    EXPECT_TRUE(schedule->isHidden());
    EXPECT_TRUE(directLightSamples->isHidden());
    EXPECT_TRUE(tracingExecution->isHidden());
    EXPECT_TRUE(intersectionBackend->isHidden());
    EXPECT_TRUE(denoiser->isHidden());
    EXPECT_TRUE(radius->isHidden());
    EXPECT_TRUE(colorSigma->isHidden());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldInitializeRayControlsFromRenderIntent) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    auto& options = intent.engineOptions.raytracer();
    options.setSampler("Jittered");
    options.setSamplesPerPixel(9);
    options.setMaximumRecursionDepth(12);
    options.setDirectLightSamples(5);
    options.setTracingExecution(engine::graph::TracingExecutionPreference::Hybrid);
    options.setIntersectionBackend("gpu");
    options.setDenoiser("bilateral");
    options.setDenoiseRadius(4);
    options.setDenoiseColorSigma(0.25);

    widget.setRenderIntent(intent);

    EXPECT_EQ(QString("Path Tracer"), widget.engine());
    EXPECT_EQ(QString("Wavefront"), widget.pathTracingSchedule());
    EXPECT_EQ(QString("Jittered"), widget.sampler());
    EXPECT_EQ(9, widget.samplesPerPixel());
    EXPECT_EQ(12, widget.maxRecursionDepth());
    EXPECT_EQ(5, widget.directLightSamples());
    EXPECT_EQ(QString("Hybrid"), widget.tracingExecution());
    EXPECT_EQ(QString("GPU"), widget.wavefrontIntersectionBackend());
    EXPECT_TRUE(widget.denoiserOverrideEnabled());
    EXPECT_EQ(QString("Bilateral"), widget.denoiser());
    EXPECT_EQ(4, widget.denoiseRadius());
    EXPECT_DOUBLE_EQ(0.25, widget.denoiseColorSigma());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldRestoreEngineManagedRayDefaultsWhenIntentOmitsThem) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent explicitIntent;
    explicitIntent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    explicitIntent.engineOptions.raytracer().setSampler("Jittered");
    explicitIntent.engineOptions.raytracer().setSamplesPerPixel(7);
    explicitIntent.engineOptions.raytracer().setDirectLightSamples(4);
    widget.setRenderIntent(explicitIntent);

    engine::graph::RenderIntent defaultIntent;
    defaultIntent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    widget.setRenderIntent(defaultIntent);

    EXPECT_EQ(QString("Path Tracer"), widget.engine());
    EXPECT_EQ(QString("Halton"), widget.sampler());
    EXPECT_EQ(64, widget.samplesPerPixel());
    EXPECT_EQ(1, widget.directLightSamples());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldInitializeScalarPathTracerFromRenderIntent) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Raytracer;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");

    widget.setRenderIntent(intent);

    EXPECT_EQ(QString("Path Tracer"), widget.engine());
    EXPECT_EQ(QString("Scalar"), widget.pathTracingSchedule());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldMapWavefrontPathTracerIntentToPathTracerSchedule) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Wavefront;
    intent.engineOptions.raytracer().setIntegrator("pathtracer");

    widget.setRenderIntent(intent);

    EXPECT_EQ(QString("Path Tracer"), widget.engine());
    EXPECT_EQ(QString("Wavefront"), widget.pathTracingSchedule());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldInitializeImplicitDenoiserParametersFromRenderIntent) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent boxIntent;
    boxIntent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    boxIntent.engineOptions.raytracer().setDenoiser("box");

    widget.setRenderIntent(boxIntent);

    EXPECT_EQ(QString("Box"), widget.denoiser());
    EXPECT_EQ(1, widget.denoiseRadius());

    engine::graph::RenderIntent bilateralIntent;
    bilateralIntent.defaultExecutor = engine::graph::RenderExecutorPreference::PathTracer;
    bilateralIntent.engineOptions.raytracer().setDenoiser("bilateral");

    widget.setRenderIntent(bilateralIntent);

    EXPECT_EQ(QString("Bilateral"), widget.denoiser());
    EXPECT_EQ(2, widget.denoiseRadius());
    EXPECT_DOUBLE_EQ(0.1, widget.denoiseColorSigma());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldInitializeRasterControlsFromRenderIntent) {
    RenderSettingsWidget widget;
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    intent.enablePreviewShadows = true;
    intent.postProcessAA = engine::graph::RenderPostProcessAA::SMAA;
    auto& options = intent.engineOptions.rasterizer();
    options.setBackend(engine::raster::RasterBackend::openGL());
    options.setLod(4);
    options.setMSAASamples(8);
    options.setMSAAShadingMode("per_fragment");
    options.setShadowMapSize(1024);
    options.setShadowCascadeCount(3);
    options.setShadowCascadeSplitLambda(0.75);
    options.setShadowBias(0.125);
    options.setShadowSlopeBias(0.03);
    options.setShadowFilterRadius(2);
    options.setShadowFilterMode("pcss");

    widget.setRenderIntent(intent);

    EXPECT_EQ(QString("Rasterizer"), widget.engine());
    EXPECT_EQ(QString("OpenGL"), widget.rasterBackend());
    EXPECT_EQ(4, widget.lod());
    EXPECT_EQ(8, widget.msaaSamples());
    EXPECT_EQ(QString("Per fragment"), widget.msaaShadingMode());
    EXPECT_EQ(QString("SMAA"), widget.postProcessAA());
    EXPECT_TRUE(widget.shadowMapsEnabled());
    EXPECT_EQ(1024, widget.shadowMapSize());
    EXPECT_EQ(3, widget.shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.75, widget.shadowCascadeSplitLambda());
    EXPECT_DOUBLE_EQ(0.125, widget.shadowBias());
    EXPECT_DOUBLE_EQ(0.03, widget.shadowSlopeBias());
    EXPECT_EQ(2, widget.shadowFilterRadius());
    EXPECT_EQ(QString("PCSS"), widget.shadowFilterMode());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterMSAAToOneSample) {
    RenderSettingsWidget widget;
    EXPECT_EQ(QString("CPU"), widget.rasterBackend());
    EXPECT_EQ(1, widget.msaaSamples());
    EXPECT_EQ(QString("Per sample"), widget.msaaShadingMode());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterPostAAToNone) {
    RenderSettingsWidget widget;
    EXPECT_EQ(QString("None"), widget.postProcessAA());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterShadowMapsToOffWithEngineDefaults) {
    RenderSettingsWidget widget;
    EXPECT_FALSE(widget.shadowMapsEnabled());
    EXPECT_EQ(256, widget.shadowMapSize());
    EXPECT_EQ(1, widget.shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.5, widget.shadowCascadeSplitLambda());
    EXPECT_DOUBLE_EQ(0.001, widget.shadowBias());
    EXPECT_DOUBLE_EQ(0.0, widget.shadowSlopeBias());
    EXPECT_EQ(0, widget.shadowFilterRadius());
    EXPECT_EQ(QString("PCF"), widget.shadowFilterMode());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRaytracerToPeriodicProgressDisplay) {
    RenderSettingsWidget widget;
    EXPECT_EQ(RenderWidget::DisplayMode::PeriodicUpdate, widget.displayMode());
    EXPECT_TRUE(widget.showProgressIndicators());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterizerToDoubleBufferedDisplay) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    ASSERT_NE(nullptr, engineType);

    engineType->setCurrentText("Rasterizer");

    EXPECT_EQ(RenderWidget::DisplayMode::DoubleBuffer, widget.displayMode());
    EXPECT_FALSE(widget.showProgressIndicators());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldExposeAllDisplayModesForAnyEngine) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto displayMode = widget.findChild<QComboBox*>("displayUpdateMode");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, displayMode);
    EXPECT_EQ(-1, engineType->findText("Wavefront"));
    EXPECT_NE(-1, engineType->findText("Path Tracer"));
    EXPECT_EQ(nullptr, widget.findChild<QComboBox*>("rayIntegrator"));

    for (const QString& engine : {QString("Raytracer"), QString("Path Tracer"),
                                  QString("Wireframe"), QString("Rasterizer")}) {
      engineType->setCurrentText(engine);
      EXPECT_FALSE(displayMode->isHidden()) << engine.toStdString();
    }

    displayMode->setCurrentText("Periodic update");
    EXPECT_EQ(RenderWidget::DisplayMode::PeriodicUpdate, widget.displayMode());

    displayMode->setCurrentText("Completed tiles");
    EXPECT_EQ(RenderWidget::DisplayMode::CompletedTilePublishing, widget.displayMode());

    displayMode->setCurrentText("Double buffer");
    EXPECT_EQ(RenderWidget::DisplayMode::DoubleBuffer, widget.displayMode());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReadRasterShadowMapControls) {
    RenderSettingsWidget widget;
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    auto shadowMapSize = widget.findChild<QSpinBox*>("rasterShadowMapSize");
    auto cascadeCount = widget.findChild<QSpinBox*>("rasterShadowCascadeCount");
    auto cascadeSplit = widget.findChild<QDoubleSpinBox*>("rasterShadowCascadeSplitLambda");
    auto shadowBias = widget.findChild<QDoubleSpinBox*>("rasterShadowBias");
    auto shadowSlopeBias = widget.findChild<QDoubleSpinBox*>("rasterShadowSlopeBias");
    auto filterRadius = widget.findChild<QSpinBox*>("rasterShadowFilterRadius");
    auto filterMode = widget.findChild<QComboBox*>("rasterShadowFilterMode");
    ASSERT_NE(nullptr, shadowMaps);
    ASSERT_NE(nullptr, shadowMapSize);
    ASSERT_NE(nullptr, cascadeCount);
    ASSERT_NE(nullptr, cascadeSplit);
    ASSERT_NE(nullptr, shadowBias);
    ASSERT_NE(nullptr, shadowSlopeBias);
    ASSERT_NE(nullptr, filterRadius);
    ASSERT_NE(nullptr, filterMode);

    shadowMaps->setChecked(true);
    shadowMapSize->setValue(1024);
    cascadeCount->setValue(3);
    cascadeSplit->setValue(0.75);
    shadowBias->setValue(0.25);
    shadowSlopeBias->setValue(0.03);
    filterRadius->setValue(3);
    filterMode->setCurrentText("PCSS");

    EXPECT_TRUE(widget.shadowMapsEnabled());
    EXPECT_EQ(1024, widget.shadowMapSize());
    EXPECT_EQ(3, widget.shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.75, widget.shadowCascadeSplitLambda());
    EXPECT_DOUBLE_EQ(0.25, widget.shadowBias());
    EXPECT_DOUBLE_EQ(0.03, widget.shadowSlopeBias());
    EXPECT_EQ(3, widget.shadowFilterRadius());
    EXPECT_EQ(QString("PCSS"), widget.shadowFilterMode());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReadRasterAAControls) {
    RenderSettingsWidget widget;
    auto backend = widget.findChild<QComboBox*>("rasterBackend");
    auto shadingMode = widget.findChild<QComboBox*>("rasterMsaaShadingMode");
    auto postAA = widget.findChild<QComboBox*>("rasterPostProcessAA");
    ASSERT_NE(nullptr, backend);
    ASSERT_NE(nullptr, shadingMode);
    ASSERT_NE(nullptr, postAA);

    backend->setCurrentText("OpenGL");
    shadingMode->setCurrentText("Per fragment");
    postAA->setCurrentText("SMAA");

    EXPECT_EQ(QString("OpenGL"), widget.rasterBackend());
    EXPECT_EQ(QString("Per fragment"), widget.msaaShadingMode());
    EXPECT_EQ(QString("SMAA"), widget.postProcessAA());

    postAA->setCurrentText("TAA");

    EXPECT_EQ(QString("TAA"), widget.postProcessAA());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldAnnotateOpenGLRasterBackend) {
    RenderSettingsWidget widget;
    auto* engineType = widget.findChild<QComboBox*>("engineType");
    auto* backend = widget.findChild<QComboBox*>("rasterBackend");
    auto* status = widget.findChild<QLabel*>("rasterBackendStatus");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, backend);
    ASSERT_NE(nullptr, status);

    engineType->setCurrentText("Rasterizer");
    EXPECT_TRUE(status->isHidden());

    backend->setCurrentText("OpenGL");

    EXPECT_FALSE(status->isHidden());
    EXPECT_TRUE(status->wordWrap());
    EXPECT_TRUE(status->text().contains(QStringLiteral("OpenGL raster backend")));
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowRasterControlsOnlyForRasterizer) {
    RenderSettingsWidget widget;

    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto backend = widget.findChild<QComboBox*>("rasterBackend");
    auto msaa = widget.findChild<QComboBox*>("rasterMsaaSamples");
    auto msaaShading = widget.findChild<QComboBox*>("rasterMsaaShadingMode");
    auto postAA = widget.findChild<QComboBox*>("rasterPostProcessAA");
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, backend);
    ASSERT_NE(nullptr, msaa);
    ASSERT_NE(nullptr, msaaShading);
    ASSERT_NE(nullptr, postAA);
    ASSERT_NE(nullptr, shadowMaps);

    EXPECT_TRUE(backend->isHidden());
    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(msaaShading->isHidden());
    EXPECT_TRUE(postAA->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Wireframe");
    EXPECT_TRUE(backend->isHidden());
    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(msaaShading->isHidden());
    EXPECT_TRUE(postAA->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Path Tracer");
    EXPECT_TRUE(backend->isHidden());
    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(msaaShading->isHidden());
    EXPECT_TRUE(postAA->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Rasterizer");
    EXPECT_FALSE(backend->isHidden());
    EXPECT_FALSE(msaa->isHidden());
    EXPECT_FALSE(msaaShading->isHidden());
    EXPECT_FALSE(postAA->isHidden());
    EXPECT_FALSE(shadowMaps->isHidden());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowShadowMapDetailsOnlyWhenEnabledForRasterizer) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    auto shadowMapSize = widget.findChild<QSpinBox*>("rasterShadowMapSize");
    auto cascadeCount = widget.findChild<QSpinBox*>("rasterShadowCascadeCount");
    auto cascadeSplit = widget.findChild<QDoubleSpinBox*>("rasterShadowCascadeSplitLambda");
    auto shadowBias = widget.findChild<QDoubleSpinBox*>("rasterShadowBias");
    auto shadowSlopeBias = widget.findChild<QDoubleSpinBox*>("rasterShadowSlopeBias");
    auto filterRadius = widget.findChild<QSpinBox*>("rasterShadowFilterRadius");
    auto filterMode = widget.findChild<QComboBox*>("rasterShadowFilterMode");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, shadowMaps);
    ASSERT_NE(nullptr, shadowMapSize);
    ASSERT_NE(nullptr, cascadeCount);
    ASSERT_NE(nullptr, cascadeSplit);
    ASSERT_NE(nullptr, shadowBias);
    ASSERT_NE(nullptr, shadowSlopeBias);
    ASSERT_NE(nullptr, filterRadius);
    ASSERT_NE(nullptr, filterMode);

    engineType->setCurrentText("Rasterizer");
    EXPECT_TRUE(shadowMapSize->isHidden());
    EXPECT_TRUE(cascadeCount->isHidden());
    EXPECT_TRUE(cascadeSplit->isHidden());
    EXPECT_TRUE(shadowBias->isHidden());
    EXPECT_TRUE(shadowSlopeBias->isHidden());
    EXPECT_TRUE(filterRadius->isHidden());
    EXPECT_TRUE(filterMode->isHidden());

    shadowMaps->setChecked(true);
    EXPECT_FALSE(shadowMapSize->isHidden());
    EXPECT_FALSE(cascadeCount->isHidden());
    EXPECT_FALSE(cascadeSplit->isHidden());
    EXPECT_FALSE(shadowBias->isHidden());
    EXPECT_FALSE(shadowSlopeBias->isHidden());
    EXPECT_FALSE(filterRadius->isHidden());
    EXPECT_FALSE(filterMode->isHidden());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReturnNonZeroResolution) {
    // Resolution comes from a "WIDTHxHEIGHT" combo box entry — pin that
    // both dimensions are non-zero so a misformatted .ui default trips
    // the check (a malformed entry would parse to 0×0).
    RenderSettingsWidget widget;
    auto res = widget.resolution();
    EXPECT_GT(res.width(), 0);
    EXPECT_GT(res.height(), 0);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldEmitRenderClickedOnRender) {
    RenderSettingsWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(renderClicked()), &slot, SLOT(receive()));
    QMetaObject::invokeMethod(&widget, "render", Qt::DirectConnection);
    EXPECT_TRUE(slot.called());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldEmitStopClickedOnStop) {
    RenderSettingsWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(stopClicked()), &slot, SLOT(receive()));
    QMetaObject::invokeMethod(&widget, "stop", Qt::DirectConnection);
    EXPECT_TRUE(slot.called());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldAcceptSetBusy) {
    // setBusy toggles enabled state on a list of input controls and
    // swaps the visible label to show progress. Just verify the call
    // doesn't crash — granular assertions on enabled state would couple
    // the test to the .ui layout.
    RenderSettingsWidget widget;
    widget.setBusy(true);
    widget.setBusy(false);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldAcceptSetElapsedTime) {
    RenderSettingsWidget widget;
    widget.setElapsedTime(1234);
  }
}
