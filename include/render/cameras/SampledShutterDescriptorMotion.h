#pragma once

#include "core/math/Matrix.h"

#include <optional>

namespace render {
  class Camera;

  namespace detail {
    struct SampledShutterDescriptorMotion {
      Matrix4d matrixAtOpen;
      Matrix4d matrixAtClose;
    };

    [[nodiscard]] std::optional<SampledShutterDescriptorMotion>
    sampledStableBasisShutterMotion(const Camera& camera);
  }
}
