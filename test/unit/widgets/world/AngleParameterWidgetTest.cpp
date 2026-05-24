#include <gtest/gtest.h>

#include "widgets/world/AngleParameterWidget.h"
#include "core/math/Angle.h"

#include "test/helpers/GuiTestHelper.h"

Q_DECLARE_METATYPE(Angled);

namespace AngleParameterWidgetTest {
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Angled>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  class AngleParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(AngleParameterWidgetTest, ShouldInitialize) {
    AngleParameterWidget widget;
  }

  TEST_F(AngleParameterWidgetTest, ShouldRoundtripAngleViaSetValue) {
    // Default unit in the dropdown is "Radians" (first entry of the
    // QComboBox in the .ui file). Round-trip a value through that path.
    // The setter formats via QString::number ('g' format → ~6 sig figs)
    // and the getter parses with toDouble; allow a small tolerance.
    AngleParameterWidget widget;
    auto original = 1.5_radians;
    widget.setValue(QVariant::fromValue(original));
    auto back = widget.value().value<Angled>();
    EXPECT_NEAR(original.radians(), back.radians(), 1e-5);
  }

  TEST_F(AngleParameterWidgetTest, ShouldRoundtripDegreesViaSetValue) {
    // Setting an angle in degrees should still round-trip correctly even
    // though the displayed unit defaults to radians — Angled is unit-
    // agnostic internally.
    AngleParameterWidget widget;
    auto original = 45_degrees;
    widget.setValue(QVariant::fromValue(original));
    auto back = widget.value().value<Angled>();
    EXPECT_NEAR(original.radians(), back.radians(), 1e-5);
  }

  TEST_F(AngleParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    AngleParameterWidget widget;
    widget.setParameterName("rotation");
    EXPECT_EQ(QString("rotation"), widget.parameterName());
  }
}
