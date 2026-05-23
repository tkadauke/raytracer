#include <gtest/gtest.h>

#include "widgets/world/RenderSettingsWidget.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QThread>

namespace RenderSettingsWidgetTest {
  class RenderSettingsWidgetTest : public ::testing::GuiTest {};

  TEST_F(RenderSettingsWidgetTest, ShouldInitialize) {
    RenderSettingsWidget widget;
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultSamplerToRegular) {
    // The constructor calls setCurrentText("Regular") on the sampler combo
    // box; pin it so a future "default to MultiJittered" change is loud.
    RenderSettingsWidget widget;
    EXPECT_EQ(QString("Regular"), widget.sampler());
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

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultQueueSizeToEightTimesIdealThreadCount) {
    RenderSettingsWidget widget;
    EXPECT_EQ(QThread::idealThreadCount() * 8, widget.queueSize());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReturnPositiveSamplesPerPixel) {
    RenderSettingsWidget widget;
    EXPECT_GT(widget.samplesPerPixel(), 0);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReturnPositiveMaxRecursionDepth) {
    RenderSettingsWidget widget;
    EXPECT_GT(widget.maxRecursionDepth(), 0);
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterMSAAToOneSample) {
    RenderSettingsWidget widget;
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

    for (const QString& engine : {QString("Raytracer"), QString("Wireframe"), QString("Rasterizer")}) {
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
    auto shadingMode = widget.findChild<QComboBox*>("rasterMsaaShadingMode");
    auto postAA = widget.findChild<QComboBox*>("rasterPostProcessAA");
    ASSERT_NE(nullptr, shadingMode);
    ASSERT_NE(nullptr, postAA);

    shadingMode->setCurrentText("Per fragment");
    postAA->setCurrentText("SMAA");

    EXPECT_EQ(QString("Per fragment"), widget.msaaShadingMode());
    EXPECT_EQ(QString("SMAA"), widget.postProcessAA());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowRasterControlsOnlyForRasterizer) {
    RenderSettingsWidget widget;

    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto msaa = widget.findChild<QComboBox*>("rasterMsaaSamples");
    auto msaaShading = widget.findChild<QComboBox*>("rasterMsaaShadingMode");
    auto postAA = widget.findChild<QComboBox*>("rasterPostProcessAA");
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, msaa);
    ASSERT_NE(nullptr, msaaShading);
    ASSERT_NE(nullptr, postAA);
    ASSERT_NE(nullptr, shadowMaps);

    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(msaaShading->isHidden());
    EXPECT_TRUE(postAA->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Wireframe");
    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(msaaShading->isHidden());
    EXPECT_TRUE(postAA->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Rasterizer");
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
