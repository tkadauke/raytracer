#include "gtest.h"
#include "widgets/SphericalCameraParameterWidget.h"
#include "raytracer/cameras/SphericalCamera.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Signal.h"
#include "test/helpers/Slot.h"

namespace SphericalCameraParameterWidgetTest {
  using namespace ::testing;
  
  class SphericalCameraParameterWidgetTest : public GuiTest {};
  
  TEST_F(SphericalCameraParameterWidgetTest, ShouldInitialize) {
    SphericalCameraParameterWidget widget;
  }
  
  TEST_F(SphericalCameraParameterWidgetTest, ShouldReturnHorizontalFieldOfView) {
    SphericalCameraParameterWidget widget;
    ASSERT_EQ(180, widget.horizontalFieldOfView());
  }
  
  TEST_F(SphericalCameraParameterWidgetTest, ShouldReturnVerticalFieldOfView) {
    SphericalCameraParameterWidget widget;
    ASSERT_EQ(90, widget.verticalFieldOfView());
  }
  
  TEST_F(SphericalCameraParameterWidgetTest, ShouldEmitChange) {
    SphericalCameraParameterWidget widget;
    Signal sig;
    Slot slot;
    QObject::connect(&sig, SIGNAL(send()), &widget, SLOT(parameterChanged()));
    QObject::connect(&widget, SIGNAL(changed()), &slot, SLOT(receive()));
    sig.call();
    ASSERT_TRUE(slot.called());
  }
  
  TEST_F(SphericalCameraParameterWidgetTest, ShouldApplyToSphericalCamera) {
    SphericalCameraParameterWidget widget;
    SphericalCamera camera;
    widget.applyTo(&camera);
    ASSERT_EQ(widget.horizontalFieldOfView(), camera.horizontalFieldOfView());
    ASSERT_EQ(widget.verticalFieldOfView(), camera.verticalFieldOfView());
  }
}
