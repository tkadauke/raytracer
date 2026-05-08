#include <gtest/gtest.h>

#include "widgets/world/RenderSettingsWidget.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

#include <QComboBox>
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

  TEST_F(RenderSettingsWidgetTest, ShouldShowMSAAOnlyForRasterizer) {
    RenderSettingsWidget widget;

    auto engineType = widget.findChild<QComboBox*>("engineType");
    auto msaa = widget.findChild<QComboBox*>("rasterMsaaSamples");
    ASSERT_NE(nullptr, engineType);
    ASSERT_NE(nullptr, msaa);

    EXPECT_TRUE(msaa->isHidden());

    engineType->setCurrentText("Wireframe");
    EXPECT_TRUE(msaa->isHidden());

    engineType->setCurrentText("Rasterizer");
    EXPECT_FALSE(msaa->isHidden());
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
