#include <gtest/gtest.h>

#include "widgets/world/ColorParameterWidget.h"
#include "core/Color.h"

#include "test/helpers/GuiTestHelper.h"

#include <QDoubleSpinBox>

// Mirror Q_DECLARE_METATYPE(Colord) from the widget's TU so QVariant
// round-tripping via .value<Colord>() works in the test TU.
Q_DECLARE_METATYPE(Colord);

namespace ColorParameterWidgetTest {
  // ColorParameterWidget uses qRegisterMetaType<Colord> implicitly via
  // QVariant::fromValue / value<Colord>; register once for this test TU.
  // The static-bool init mirrors how SceneTest handles the same situation.
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Colord>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  class ColorParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(ColorParameterWidgetTest, ShouldInitialize) {
    ColorParameterWidget widget;
  }

  TEST_F(ColorParameterWidgetTest, ShouldRoundtripColorViaSetValue) {
    ColorParameterWidget widget;
    Colord c(0.25, 0.5, 0.75);
    widget.setValue(QVariant::fromValue(c));

    auto back = widget.value().value<Colord>();
    // QString::number's default 'g' format keeps ~6 sig figs — small
    // tolerance is generous for these clean fractions.
    EXPECT_NEAR(c.r(), back.r(), 1e-6);
    EXPECT_NEAR(c.g(), back.g(), 1e-6);
    EXPECT_NEAR(c.b(), back.b(), 1e-6);
  }

  TEST_F(ColorParameterWidgetTest, ShouldRoundtripBlackViaSetValue) {
    ColorParameterWidget widget;
    widget.setValue(QVariant::fromValue(Colord::black()));
    auto back = widget.value().value<Colord>();
    EXPECT_DOUBLE_EQ(0.0, back.r());
    EXPECT_DOUBLE_EQ(0.0, back.g());
    EXPECT_DOUBLE_EQ(0.0, back.b());
  }

  TEST_F(ColorParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    ColorParameterWidget widget;
    widget.setParameterName("specularColor");
    EXPECT_EQ(QString("specularColor"), widget.parameterName());
  }

  TEST_F(ColorParameterWidgetTest, ShouldUseNumericChannelEditors) {
    ColorParameterWidget widget;
    EXPECT_EQ(3, widget.findChildren<QDoubleSpinBox*>().size());
  }
}
