#pragma once

#include "core/math/Rect.h"
#include "render/cameras/Camera.h"

namespace test {
  // Install dimensions on a camera view plane before tests call projection helpers directly.
  inline void setupViewPlane(render::Camera& camera, int width = 100, int height = 100) {
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, width, height));
  }
}
