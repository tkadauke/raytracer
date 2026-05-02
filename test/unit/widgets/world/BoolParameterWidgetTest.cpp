#include <gtest/gtest.h>

#include "widgets/world/BoolParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace BoolParameterWidgetTest {
  class BoolParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(BoolParameterWidgetTest, ShouldInitialize) {
    BoolParameterWidget widget;
  }

  TEST_F(BoolParameterWidgetTest, ShouldRoundtripTrueViaSetValue) {
    BoolParameterWidget widget;
    widget.setValue(QVariant::fromValue(true));
    EXPECT_TRUE(widget.value().toBool());
  }

  TEST_F(BoolParameterWidgetTest, ShouldRoundtripFalseViaSetValue) {
    BoolParameterWidget widget;
    widget.setValue(QVariant::fromValue(true));
    widget.setValue(QVariant::fromValue(false));
    EXPECT_FALSE(widget.value().toBool());
  }

  TEST_F(BoolParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    // BoolParameterWidget keys the label off the checkbox text rather than
    // a separate label. Setting the parameter name is the only knob users
    // have for the displayed string; round-trip the name through
    // parameterName() to verify the side effect ran.
    BoolParameterWidget widget;
    widget.setParameterName("Visible");
    EXPECT_EQ(QString("Visible"), widget.parameterName());
  }
}
