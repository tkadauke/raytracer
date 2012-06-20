#include "gtest.h"
#include "widgets/PinholeCameraParameterWidget.h"
#include "cameras/PinholeCamera.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Signal.h"
#include "test/helpers/Slot.h"

namespace PinholeCameraParameterWidgetTest {
  using namespace ::testing;
  
  class PinholeCameraParameterWidgetTest : public GuiTest {};
  
  TEST_F(PinholeCameraParameterWidgetTest, ShouldInitialize) {
    PinholeCameraParameterWidget widget;
  }
  
  TEST_F(PinholeCameraParameterWidgetTest, ShouldReturnDistance) {
    PinholeCameraParameterWidget widget;
    ASSERT_EQ(5, widget.distance());
  }
  
  TEST_F(PinholeCameraParameterWidgetTest, ShouldReturnZoom) {
    PinholeCameraParameterWidget widget;
    ASSERT_EQ(1.0, widget.zoom());
  }
  
  TEST_F(PinholeCameraParameterWidgetTest, ShouldEmitChange) {
    PinholeCameraParameterWidget widget;
    Signal sig;
    Slot slot;
    QObject::connect(&sig, SIGNAL(send()), &widget, SLOT(parameterChanged()));
    QObject::connect(&widget, SIGNAL(changed()), &slot, SLOT(receive()));
    sig.call();
    ASSERT_TRUE(slot.called());
  }
  
  TEST_F(PinholeCameraParameterWidgetTest, ShouldApplyToPinholeCamera) {
    PinholeCameraParameterWidget widget;
    PinholeCamera camera;
    widget.applyTo(&camera);
    ASSERT_EQ(widget.distance(), camera.distance());
    ASSERT_EQ(widget.zoom(), camera.zoom());
  }
}
