#pragma once

#include "core/math/Rect.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/cameras/PinholeCamera.h"

#include <memory>

namespace test {
  // Install dimensions on a camera view plane before tests call projection helpers directly.
  inline void setupViewPlane(render::Camera& camera, int width = 100, int height = 100) {
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, width, height));
  }

  namespace helpers {
    // Standard test camera: eye at (0, 0, -5) looking at the origin.
    inline std::shared_ptr<render::PinholeCamera> standardCamera() {
      return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    }
  }
}
