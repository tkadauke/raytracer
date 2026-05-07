#include <gtest/gtest.h>

#include "world/objects/EquirectangularCamera.h"
#include "render/cameras/EquirectangularCamera.h"

namespace EquirectangularCameraWorldTest {
  TEST(WorldEquirectangularCamera, ShouldInitialize) {
    EquirectangularCamera camera;
  }

  TEST(WorldEquirectangularCamera, ShouldProduceRaytracerEquirectangularCamera) {
    EquirectangularCamera camera;
    auto rt = std::dynamic_pointer_cast<render::EquirectangularCamera>(camera.toRaytracer());
    EXPECT_NE(nullptr, rt);
  }
}
