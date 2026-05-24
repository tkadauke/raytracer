#include <gtest/gtest.h>

#include "widgets/ViewPlaneTypeWidget.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

namespace ViewPlaneTypeWidgetTest {
  class ViewPlaneTypeWidgetTest : public ::testing::GuiTest {};

  TEST_F(ViewPlaneTypeWidgetTest, ShouldInitialize) {
    ViewPlaneTypeWidget widget;
  }

  TEST_F(ViewPlaneTypeWidgetTest, ShouldReturnRegisteredViewPlaneType) {
    ViewPlaneTypeWidget widget;
    auto identifiers = render::ViewPlaneFactory::self().identifiers();
    EXPECT_NE(std::find(identifiers.begin(), identifiers.end(), widget.type()), identifiers.end());
  }

  TEST_F(ViewPlaneTypeWidgetTest, ShouldEmitChangedOnTypeChange) {
    ViewPlaneTypeWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(changed()), &slot, SLOT(receive()));
    QMetaObject::invokeMethod(&widget, "typeChanged", Qt::DirectConnection);
    EXPECT_TRUE(slot.called());
  }
}
