#include "gtest.h"
#include "widgets/SphericalCameraParameterWidget.h"
#include "raytracer/cameras/SphericalCamera.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Signal.h"
#include "test/helpers/Slot.h"

namespace SphericalCameraParameterWidgetTest {
  using namespace ::testing;
  using namespace raytracer;
  
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
    ASSERT_NEAR(widget.horizontalFieldOfView(), camera.horizontalFieldOfView().degrees(), 0.001);
    ASSERT_NEAR(widget.verticalFieldOfView(), camera.verticalFieldOfView().degrees(), 0.001);
  }
}
