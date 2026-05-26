#include <gtest/gtest.h>

#include "widgets/world/DoubleParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

#include <QDoubleSpinBox>

namespace DoubleParameterWidgetTest {
  class DoubleParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(DoubleParameterWidgetTest, ShouldInitialize) {
    DoubleParameterWidget widget;
  }

  TEST_F(DoubleParameterWidgetTest, ShouldRoundtripDoubleViaSetValue) {
    DoubleParameterWidget widget;
    widget.setValue(QVariant::fromValue(3.14));
    // setValue sends through QString::number → toDouble. Test for
    // approximate equality to allow for the printed-precision floor of
    // QString::number's default ('g' format, ~6 significant digits).
    EXPECT_NEAR(3.14, widget.value().toDouble(), 1e-6);
  }

  TEST_F(DoubleParameterWidgetTest, ShouldRoundtripNegativeDoubleViaSetValue) {
    DoubleParameterWidget widget;
    widget.setValue(QVariant::fromValue(-1.5));
    EXPECT_NEAR(-1.5, widget.value().toDouble(), 1e-9);
  }

  TEST_F(DoubleParameterWidgetTest, ShouldRoundtripZeroViaSetValue) {
    DoubleParameterWidget widget;
    widget.setValue(QVariant::fromValue(0.0));
    EXPECT_DOUBLE_EQ(0.0, widget.value().toDouble());
  }

  TEST_F(DoubleParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    DoubleParameterWidget widget;
    widget.setParameterName("intensity");
    EXPECT_EQ(QString("intensity"), widget.parameterName());
  }

  TEST_F(DoubleParameterWidgetTest, ShouldUseNumericEditor) {
    DoubleParameterWidget widget;
    EXPECT_NE(nullptr, widget.findChild<QDoubleSpinBox*>());
  }
}
