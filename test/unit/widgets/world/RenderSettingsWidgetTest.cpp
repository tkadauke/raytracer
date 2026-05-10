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
  }

  TEST_F(RenderSettingsWidgetTest, ShouldDefaultRasterShadowMapsToOffWithEngineDefaults) {
    RenderSettingsWidget widget;
    EXPECT_FALSE(widget.shadowMapsEnabled());
    EXPECT_EQ(256, widget.shadowMapSize());
    EXPECT_DOUBLE_EQ(0.001, widget.shadowBias());
    EXPECT_EQ(0, widget.shadowFilterRadius());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldReadRasterShadowMapControls) {
    RenderSettingsWidget widget;
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    auto shadowMapSize = widget.findChild<QSpinBox*>("rasterShadowMapSize");
    auto shadowBias = widget.findChild<QDoubleSpinBox*>("rasterShadowBias");
    auto filterRadius = widget.findChild<QSpinBox*>("rasterShadowFilterRadius");
    ASSERT_NE(nullptr, shadowMaps);
    ASSERT_NE(nullptr, shadowMapSize);
    ASSERT_NE(nullptr, shadowBias);
    ASSERT_NE(nullptr, filterRadius);

    shadowMaps->setChecked(true);
    shadowMapSize->setValue(1024);
    shadowBias->setValue(0.25);
    filterRadius->setValue(3);

    EXPECT_TRUE(widget.shadowMapsEnabled());
    EXPECT_EQ(1024, widget.shadowMapSize());
    EXPECT_DOUBLE_EQ(0.25, widget.shadowBias());
    EXPECT_EQ(3, widget.shadowFilterRadius());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowRasterControlsOnlyForRasterizer) {
    RenderSettingsWidget widget;

    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto msaa = widget.findChild<QComboBox*>("rasterMsaaSamples");
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, msaa);
    ASSERT_NE(nullptr, shadowMaps);

    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Wireframe");
    EXPECT_TRUE(msaa->isHidden());
    EXPECT_TRUE(shadowMaps->isHidden());

    engineType->setCurrentText("Rasterizer");
    EXPECT_FALSE(msaa->isHidden());
    EXPECT_FALSE(shadowMaps->isHidden());
  }

  TEST_F(RenderSettingsWidgetTest, ShouldShowShadowMapDetailsOnlyWhenEnabledForRasterizer) {
    RenderSettingsWidget widget;
    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto shadowMaps = widget.findChild<QCheckBox*>("rasterShadowMaps");
    auto shadowMapSize = widget.findChild<QSpinBox*>("rasterShadowMapSize");
    auto shadowBias = widget.findChild<QDoubleSpinBox*>("rasterShadowBias");
    auto filterRadius = widget.findChild<QSpinBox*>("rasterShadowFilterRadius");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, shadowMaps);
    ASSERT_NE(nullptr, shadowMapSize);
    ASSERT_NE(nullptr, shadowBias);
    ASSERT_NE(nullptr, filterRadius);

    engineType->setCurrentText("Rasterizer");
    EXPECT_TRUE(shadowMapSize->isHidden());
    EXPECT_TRUE(shadowBias->isHidden());
    EXPECT_TRUE(filterRadius->isHidden());

    shadowMaps->setChecked(true);
    EXPECT_FALSE(shadowMapSize->isHidden());
    EXPECT_FALSE(shadowBias->isHidden());
    EXPECT_FALSE(filterRadius->isHidden());
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
