#pragma once

#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include <optional>

namespace render {
  class Camera;

  namespace detail {
    struct SampledShutterLookAtDescriptorMotion {
      Vector3d positionAtOpen;
      Vector3d positionAtClose;
      Vector3d targetAtOpen;
      Vector3d targetAtClose;

      [[nodiscard]] Vector3d positionDelta() const;
      [[nodiscard]] Vector3d targetDelta() const;
    };

    struct SampledShutterDescriptorMotion {
      Matrix4d matrixAtOpen;
      Matrix4d matrixAtClose;
    };

    [[nodiscard]] std::optional<SampledShutterLookAtDescriptorMotion>
    sampledLookAtShutterMotion(const Camera& camera);

    [[nodiscard]] std::optional<SampledShutterDescriptorMotion>
    sampledStableBasisShutterMotion(const Camera& camera);
  }
}
