#pragma once

#include "core/math/Matrix.h"
#include "core/math/Vector.h"

namespace render {
  class Camera;

  namespace detail {
    class PinholeProjection {
    public:
      PinholeProjection(const Camera& camera, double distance);

      Vector2d projectPoint(const Vector3d& worldPoint) const;
      Vector3d projectPointWithDepth(const Vector3d& worldPoint) const;
      Matrix4d projectionMatrix() const;
      Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const;
      double eyeRelativeDepth(const Vector3d& worldPoint) const;

    private:
      const Camera& m_camera;
      double m_distance;
    };
  }
}
