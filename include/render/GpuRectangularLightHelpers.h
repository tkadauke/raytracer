#pragma once

#include "core/math/Vector.h"
#include "render/GpuTracingScene.h"

#include <algorithm>

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
}
