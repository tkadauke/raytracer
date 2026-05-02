#include <gtest/gtest.h>

#include "widgets/world/PreviewDisplayWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace PreviewDisplayWidgetTest {
  class PreviewDisplayWidgetTest : public ::testing::GuiTest {};

  TEST_F(PreviewDisplayWidgetTest, ShouldInitialize) {
    PreviewDisplayWidget widget(nullptr);
  }

  TEST_F(PreviewDisplayWidgetTest, ShouldReturnCannedSizeHint) {
    // sizeHint is hard-coded to 256×25 — narrow strip used in the
    // PropertyEditorWidget for preview thumbnails. Pin so a future
    // size change is deliberate.
    PreviewDisplayWidget widget(nullptr);
    EXPECT_EQ(QSize(256, 25), widget.sizeHint());
  }

  TEST_F(PreviewDisplayWidgetTest, ShouldAcceptClear) {
    // clear() asks the underlying QtDisplay to render with a null scene.
    // Smoke-test that the call doesn't crash; verifying the rendered
    // image is empty would couple to threading + paint timing.
    PreviewDisplayWidget widget(nullptr);
    widget.clear();
  }
}
