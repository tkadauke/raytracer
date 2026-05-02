#include <gtest/gtest.h>

#include "widgets/RenderWidget.h"
#include "raytracer/Raytracer.h"

#include "test/helpers/GuiTestHelper.h"

namespace RenderWidgetTest {
  class RenderWidgetTest : public ::testing::GuiTest {};

  TEST_F(RenderWidgetTest, ShouldInitializeWithNullScene) {
    // RenderWidget owns a Raytracer (held shared) but the scene pointer
    // can be null — the render path short-circuits when there's nothing
    // to draw. Smoke-test construction with a no-scene Raytracer.
    auto rt = std::make_shared<raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
  }

  TEST_F(RenderWidgetTest, ShouldAcceptSetBufferSize) {
    auto rt = std::make_shared<raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.setBufferSize(QSize(100, 50));
  }

  TEST_F(RenderWidgetTest, ShouldAcceptSetShowProgressIndicators) {
    auto rt = std::make_shared<raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.setShowProgressIndicators(true);
    widget.setShowProgressIndicators(false);
  }

  TEST_F(RenderWidgetTest, ShouldAcceptStopBeforeRender) {
    // Calling stop() with no in-flight render thread must not crash —
    // covers the "user cancels before starting" race that would
    // otherwise dereference a null thread pointer.
    auto rt = std::make_shared<raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.stop();
  }
}
