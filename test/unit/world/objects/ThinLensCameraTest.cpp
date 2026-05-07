#include <gtest/gtest.h>

#include "world/objects/ThinLensCamera.h"
#include "render/cameras/ThinLensCamera.h"

namespace ThinLensCameraWorldTest {
  TEST(WorldThinLensCamera, ShouldDefaultToCannedValues) {
    ThinLensCamera camera;
    EXPECT_DOUBLE_EQ(5.0, camera.distance());
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
    EXPECT_DOUBLE_EQ(0.1, camera.apertureRadius());
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(WorldThinLensCamera, ShouldClampZeroOrNegativeZoomToOne) {
    ThinLensCamera camera;
    camera.setZoom(0);
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
    camera.setZoom(-2);
    EXPECT_DOUBLE_EQ(1.0, camera.zoom());
  }

  TEST(WorldThinLensCamera, ShouldClampNegativeApertureRadiusToZero) {
    ThinLensCamera camera;
    camera.setApertureRadius(-1);
    EXPECT_DOUBLE_EQ(0.0, camera.apertureRadius());
  }

  TEST(WorldThinLensCamera, ShouldRejectZeroOrNegativeFocalDistance) {
    // Same contract as the raytracer-side ThinLensCamera — non-positive
    // focal distance silently keeps the previous valid value.
    ThinLensCamera camera;
    camera.setFocalDistance(0);
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
    camera.setFocalDistance(-3);
    EXPECT_DOUBLE_EQ(5.0, camera.focalDistance());
  }

  TEST(WorldThinLensCamera, ShouldProduceRaytracerThinLensCamera) {
    ThinLensCamera camera;
    auto rt = std::dynamic_pointer_cast<render::ThinLensCamera>(camera.toRaytracer());
    ASSERT_NE(nullptr, rt);
    EXPECT_DOUBLE_EQ(camera.distance(), rt->distance());
    EXPECT_DOUBLE_EQ(camera.zoom(), rt->zoom());
    EXPECT_DOUBLE_EQ(camera.apertureRadius(), rt->apertureRadius());
    EXPECT_DOUBLE_EQ(camera.focalDistance(), rt->focalDistance());
  }
}
