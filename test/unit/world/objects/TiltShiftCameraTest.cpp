#include <gtest/gtest.h>

#include "world/objects/TiltShiftCamera.h"
#include "raytracer/cameras/TiltShiftCamera.h"

namespace TiltShiftCameraWorldTest {
  TEST(WorldTiltShiftCamera, ShouldDefaultToZeroTiltAndShift) {
    TiltShiftCamera camera;
    EXPECT_DOUBLE_EQ(0.0, camera.tilt().radians());
    EXPECT_DOUBLE_EQ(0.0, camera.shiftX());
    EXPECT_DOUBLE_EQ(0.0, camera.shiftY());
  }

  TEST(WorldTiltShiftCamera, ShouldInheritThinLensDefaults) {
    TiltShiftCamera camera;
    EXPECT_DOUBLE_EQ(5.0, camera.distance());
    EXPECT_DOUBLE_EQ(0.1, camera.apertureRadius());
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(WorldTiltShiftCamera, ShouldSetAndGetTilt) {
    TiltShiftCamera camera;
    camera.setTilt(Angled(20_degrees));
    EXPECT_DOUBLE_EQ(Angled(20_degrees).radians(), camera.tilt().radians());
  }

  TEST(WorldTiltShiftCamera, ShouldSetAndGetShiftComponents) {
    TiltShiftCamera camera;
    camera.setShiftX(0.3);
    camera.setShiftY(-0.2);
    EXPECT_DOUBLE_EQ( 0.3, camera.shiftX());
    EXPECT_DOUBLE_EQ(-0.2, camera.shiftY());
  }

  TEST(WorldTiltShiftCamera, ShouldProduceRaytracerTiltShiftCamera) {
    TiltShiftCamera camera;
    camera.setTilt(Angled(15_degrees));
    camera.setShiftX(0.1);
    camera.setShiftY(0.2);

    auto rt = std::dynamic_pointer_cast<raytracer::TiltShiftCamera>(camera.toRaytracer());
    ASSERT_NE(nullptr, rt);
    // Inherited ThinLens parameters propagate.
    EXPECT_DOUBLE_EQ(camera.distance(),       rt->distance());
    EXPECT_DOUBLE_EQ(camera.zoom(),           rt->zoom());
    EXPECT_DOUBLE_EQ(camera.apertureRadius(), rt->apertureRadius());
    EXPECT_DOUBLE_EQ(camera.focalDistance(),  rt->focalDistance());
    // Tilt-shift parameters propagate.
    EXPECT_DOUBLE_EQ(camera.tilt().radians(), rt->tilt().radians());
    EXPECT_DOUBLE_EQ(camera.shiftX(),         rt->shift().x());
    EXPECT_DOUBLE_EQ(camera.shiftY(),         rt->shift().y());
  }
}
