#include <gtest/gtest.h>

#include "widgets/QtDisplay.h"
#include "engine/raytracer/Raytracer.h"

#include "test/helpers/GuiTestHelper.h"

namespace QtDisplayTest {
  class QtDisplayTest : public ::testing::GuiTest {};

  TEST_F(QtDisplayTest, ShouldInitialize) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
  }

  TEST_F(QtDisplayTest, ShouldDefaultToInteractive) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    EXPECT_TRUE(display.interactive());
  }

  TEST_F(QtDisplayTest, ShouldSetAndGetInteractive) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    display.setInteractive(false);
    EXPECT_FALSE(display.interactive());
  }

  TEST_F(QtDisplayTest, ShouldAcceptSetDistance) {
    // setDistance updates the internal interactive-camera distance the
    // wheel-event handler uses for zoom; no observable getter, so just
    // smoke-test the call.
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    display.setDistance(2.5);
  }
}
