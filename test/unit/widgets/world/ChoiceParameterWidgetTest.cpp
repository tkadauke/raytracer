#include <gtest/gtest.h>

#include "widgets/world/ChoiceParameterWidget.h"

#include "test/helpers/GuiTestHelper.h"

#include <QComboBox>

namespace ChoiceParameterWidgetTest {
  class ChoiceParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(ChoiceParameterWidgetTest, ShouldInitialize) {
    ChoiceParameterWidget widget(QStringList{"raytracer", "rasterizer"});
  }

  TEST_F(ChoiceParameterWidgetTest, ShouldRoundtripChoiceViaSetValue) {
    ChoiceParameterWidget widget(QStringList{"raytracer", "rasterizer"});
    widget.setParameterName("defaultEngine");

    widget.setValue(QVariant::fromValue(QString("rasterizer")));

    EXPECT_EQ(QString("rasterizer"), widget.value().toString());
  }

  TEST_F(ChoiceParameterWidgetTest, ShouldExposeChoicesInComboBox) {
    ChoiceParameterWidget widget(QStringList{"raytracer", "rasterizer"});
    widget.setParameterName("defaultEngine");

    auto* comboBox = widget.findChild<QComboBox*>("choiceComboBox");
    ASSERT_NE(nullptr, comboBox);
    EXPECT_EQ(2, comboBox->count());
    EXPECT_EQ(QString("raytracer"), comboBox->itemData(0).toString());
    EXPECT_EQ(QString("rasterizer"), comboBox->itemData(1).toString());
  }
}
