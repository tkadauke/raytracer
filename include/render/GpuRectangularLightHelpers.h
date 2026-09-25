#pragma once

#include "core/math/Vector.h"
#include "render/GpuTracingScene.h"

#include <algorithm>
#include <cmath>

namespace render {

  inline Vector3d rectangularLightPoint(const GpuTracingLightRecord& light,
                                        const Vector2d& sample) {
    return Vector3d(light.positionOrDirection) + Vector3d(light.u) * (sample.x() - 0.5) +
           Vector3d(light.v) * (sample.y() - 0.5);
  }

  inline double rectangularLightArea(const GpuTracingLightRecord& light) {
    return (Vector3d(light.u) ^ Vector3d(light.v)).length();
  }

  inline Vector3d rectangularLightNormal(const GpuTracingLightRecord& light, double tolerance) {
    return (Vector3d(light.u) ^ Vector3d(light.v)).normalizedOrZero(tolerance);
  }

  inline double rectangularLightSurfaceCosine(const GpuTracingLightRecord& light,
                                              const Vector3d& directionToLight, double tolerance) {
    return std::max(0.0, rectangularLightNormal(light, tolerance) * -directionToLight);
  }

  /// Tests whether `point` projects onto the `[-0.5, 0.5]^2` rectangle
  /// spanned by `edgeU`/`edgeV` around `center`, solving the 2x2 linear
  /// system for the projection's `u`/`v` coordinates. Shared by the CPU
  /// `RectangularAreaLight` and the GPU-compiled light path so both agree
  /// on what counts as "on the light".
  inline bool rectangularContainsPoint(const Vector3d& center, const Vector3d& edgeU,
                                       const Vector3d& edgeV, const Vector3d& point,
                                       double tolerance) {
    const Vector3d local = point - center;
    const double uu = edgeU * edgeU;
    const double uv = edgeU * edgeV;
    const double vv = edgeV * edgeV;
    const double lu = local * edgeU;
    const double lv = local * edgeV;
    const double determinant = uu * vv - uv * uv;
    if (std::abs(determinant) <= tolerance) {
      return false;
    }

    const double u = (vv * lu - uv * lv) / determinant;
    const double v = (uu * lv - uv * lu) / determinant;
    return u >= -0.5 - tolerance && u <= 0.5 + tolerance && v >= -0.5 - tolerance &&
           v <= 0.5 + tolerance;
  }

  inline bool rectangularLightContainsPoint(const GpuTracingLightRecord& light,
                                            const Vector3d& point, double tolerance) {
    return rectangularContainsPoint(Vector3d(light.positionOrDirection), Vector3d(light.u),
                                    Vector3d(light.v), point, tolerance);
  }
}
