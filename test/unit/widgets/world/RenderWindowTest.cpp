#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RasterPassState.h"
#include "widgets/RenderWidget.h"
#include "widgets/world/RenderWindow.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include "test/helpers/GuiTestHelper.h"

#include <QCheckBox>
#include <QComboBox>
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
