#include "gtest.h"
#include "widgets/FishEyeCameraParameterWidget.h"
#include "cameras/FishEyeCamera.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Signal.h"
#include "test/helpers/Slot.h"

namespace FishEyeCameraParameterWidgetTest {
  using namespace ::testing;
  
  class FishEyeCameraParameterWidgetTest : public GuiTest {};
  
  TEST_F(FishEyeCameraParameterWidgetTest, ShouldInitialize) {
    FishEyeCameraParameterWidget widget;
  }
  
  TEST_F(FishEyeCameraParameterWidgetTest, ShouldReturnFieldOfView) {
    FishEyeCameraParameterWidget widget;
    ASSERT_EQ(45, widget.fieldOfView());
  }
  
  TEST_F(FishEyeCameraParameterWidgetTest, ShouldEmitChange) {
    FishEyeCameraParameterWidget widget;
    Signal sig;
    Slot slot;
    QObject::connect(&sig, SIGNAL(send()), &widget, SLOT(parameterChanged()));
    QObject::connect(&widget, SIGNAL(changed()), &slot, SLOT(receive()));
    sig.call();
    ASSERT_TRUE(slot.called());
  }
  
  TEST_F(FishEyeCameraParameterWidgetTest, ShouldApplyToFisheyeCamera) {
    FishEyeCameraParameterWidget widget;
    FishEyeCamera camera;
    widget.applyTo(&camera);
    ASSERT_EQ(widget.fieldOfView(), camera.fieldOfView());
  }
}