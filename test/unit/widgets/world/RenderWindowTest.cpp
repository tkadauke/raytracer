#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "widgets/RenderWidget.h"
#include "widgets/world/RenderWindow.h"
#include "world/objects/Scene.h"

#include "test/helpers/GuiTestHelper.h"

#include <QComboBox>

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
    EXPECT_NE(nullptr, graph->explicitPlan()->findPass("raster_fxaa"));

    window.stop();
  }
}
