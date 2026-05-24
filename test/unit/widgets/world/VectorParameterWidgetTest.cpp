#include <gtest/gtest.h>

#include "widgets/world/VectorParameterWidget.h"
#include "core/math/Vector.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

Q_DECLARE_METATYPE(Vector3d);

namespace VectorParameterWidgetTest {
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Vector3d>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  class VectorParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(VectorParameterWidgetTest, ShouldInitialize) {
    VectorParameterWidget widget;
  }

  TEST_F(VectorParameterWidgetTest, ShouldRoundtripVectorViaSetValue) {
    VectorParameterWidget widget;
    widget.setValue(QVariant::fromValue(Vector3d(1.5, -2.5, 3.5)));
    auto back = widget.value().value<Vector3d>();
    ASSERT_VECTOR_NEAR(Vector3d(1.5, -2.5, 3.5), back, 1e-6);
  }

  TEST_F(VectorParameterWidgetTest, ShouldRoundtripOriginViaSetValue) {
    VectorParameterWidget widget;
    widget.setValue(QVariant::fromValue(Vector3d(7, 8, 9)));
    widget.setValue(QVariant::fromValue(Vector3d(0, 0, 0)));
    auto back = widget.value().value<Vector3d>();
    ASSERT_VECTOR_NEAR(Vector3d(0, 0, 0), back, 1e-9);
  }

  TEST_F(VectorParameterWidgetTest, ShouldUpdateLabelOnSetParameterName) {
    VectorParameterWidget widget;
    widget.setParameterName("position");
    EXPECT_EQ(QString("position"), widget.parameterName());
  }
}
