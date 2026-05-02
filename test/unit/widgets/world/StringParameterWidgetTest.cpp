#include <gtest/gtest.h>

#include "widgets/world/StringParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace StringParameterWidgetTest {
  class StringParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(StringParameterWidgetTest, ShouldInitialize) {
    StringParameterWidget widget;
  }

  TEST_F(StringParameterWidgetTest, ShouldRoundtripStringViaSetValue) {
    StringParameterWidget widget;
    widget.setValue(QVariant::fromValue(QString("hello world")));
    EXPECT_EQ(QString("hello world"), widget.value().toString());
  }

  TEST_F(StringParameterWidgetTest, ShouldRoundtripEmptyStringViaSetValue) {
    StringParameterWidget widget;
    widget.setValue(QVariant::fromValue(QString("nonempty")));
    widget.setValue(QVariant::fromValue(QString()));
    EXPECT_TRUE(widget.value().toString().isEmpty());
  }

  TEST_F(StringParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    StringParameterWidget widget;
    widget.setParameterName("label");
    EXPECT_EQ(QString("label"), widget.parameterName());
  }
}
