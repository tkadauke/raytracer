#include <gtest/gtest.h>

#include "widgets/world/StringParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

#include <QLineEdit>

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

  TEST_F(StringParameterWidgetTest, ShouldNotResetTextOrCursorWhileEditing) {
    StringParameterWidget widget;
    widget.setValue(QVariant::fromValue(QString("[2, 0]")));

    auto* edit = widget.findChild<QLineEdit*>("stringEdit");
    ASSERT_NE(nullptr, edit);
    edit->setFocus();
    edit->setCursorPosition(2);
    edit->backspace();
    ASSERT_EQ(QString("[, 0]"), edit->text());
    ASSERT_EQ(1, edit->cursorPosition());

    widget.setValue(QVariant::fromValue(QString("[, 0]")));

    EXPECT_EQ(QString("[, 0]"), edit->text());
    EXPECT_EQ(1, edit->cursorPosition());
  }
}
