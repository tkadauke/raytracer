#include <gtest/gtest.h>

#include "widgets/world/IntParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace IntParameterWidgetTest {
  class IntParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(IntParameterWidgetTest, ShouldInitialize) {
    IntParameterWidget widget;
  }

  TEST_F(IntParameterWidgetTest, ShouldRoundtripIntegerViaSetValue) {
    IntParameterWidget widget;
    widget.setValue(QVariant::fromValue(42));
    EXPECT_EQ(42, widget.value().toInt());
  }

  TEST_F(IntParameterWidgetTest, ShouldRoundtripNegativeIntegerViaSetValue) {
    IntParameterWidget widget;
    widget.setValue(QVariant::fromValue(-7));
    EXPECT_EQ(-7, widget.value().toInt());
  }

  TEST_F(IntParameterWidgetTest, ShouldRoundtripZeroViaSetValue) {
    IntParameterWidget widget;
    widget.setValue(QVariant::fromValue(123));
    widget.setValue(QVariant::fromValue(0));
    EXPECT_EQ(0, widget.value().toInt());
  }

  TEST_F(IntParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    IntParameterWidget widget;
    widget.setParameterName("count");
    EXPECT_EQ(QString("count"), widget.parameterName());
  }
}
