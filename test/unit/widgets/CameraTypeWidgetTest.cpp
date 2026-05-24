#include <gtest/gtest.h>

#include "widgets/CameraTypeWidget.h"
#include "render/cameras/CameraFactory.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Slot.h"

namespace CameraTypeWidgetTest {
  class CameraTypeWidgetTest : public ::testing::GuiTest {};

  TEST_F(CameraTypeWidgetTest, ShouldInitialize) {
    CameraTypeWidget widget;
  }

  TEST_F(CameraTypeWidgetTest, ShouldReturnRegisteredCameraType) {
    // The widget populates its combo box from CameraFactory::identifiers
    // at construction; the initial type() value should be one of those
    // registered identifiers (whichever the .ui file defaults to). Pin
    // membership rather than a specific name so adding a new camera
    // doesn't churn this test.
    CameraTypeWidget widget;
    auto identifiers = render::CameraFactory::self().identifiers();
    EXPECT_NE(std::find(identifiers.begin(), identifiers.end(), widget.type()), identifiers.end());
  }

  TEST_F(CameraTypeWidgetTest, ShouldEmitChangedOnTypeChange) {
    CameraTypeWidget widget;
    Slot slot;
    QObject::connect(&widget, SIGNAL(changed()), &slot, SLOT(receive()));

    // typeChanged() is a private slot; trigger it through Qt's meta-object
    // by name. Using QMetaObject::invokeMethod keeps the test from
    // reaching into the widget's privates.
    QMetaObject::invokeMethod(&widget, "typeChanged", Qt::DirectConnection);

    EXPECT_TRUE(slot.called());
  }
}
