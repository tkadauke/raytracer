#include <gtest/gtest.h>

#include "widgets/world/RenderWindow.h"
#include "world/objects/Scene.h"

#include "test/helpers/GuiTestHelper.h"

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
    // immediately calls scene->toRaytracerScene()) — SceneBrowser only
    // ever passes a real scene. A defensive null-guard would be a
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
}
