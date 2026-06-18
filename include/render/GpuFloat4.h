#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <algorithm>
#include <array>

namespace render {
  using GpuFloat4 = std::array<float, 4>;

  [[nodiscard]] inline GpuFloat4 gpuFloat4(const Vector3d& value, float w = 0.0f) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
            static_cast<float>(value.z()), w};
  }

  [[nodiscard]] inline GpuFloat4 gpuColor4(const Colord& value, float alpha = 1.0f) {
    return {static_cast<float>(value.r()), static_cast<float>(value.g()),
            static_cast<float>(value.b()), alpha};
  }

  [[nodiscard]] inline GpuFloat4 gpuFloat4Zero() {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }

  [[nodiscard]] inline Vector3d gpuFloat4ToVector3(const GpuFloat4& value) {
    return Vector3d(value);
  }

  [[nodiscard]] inline Vector4d gpuFloat4ToPoint4(const GpuFloat4& value) {
    return Vector4d(value[0], value[1], value[2], value[3]);
  }

  [[nodiscard]] inline Colord gpuFloat4ToColor(const GpuFloat4& value) {
    return Colord(value[0], value[1], value[2]);
  }

  [[nodiscard]] inline double gpuFloat4MaxColor(const GpuFloat4& value) {
    return std::max({0.0f, value[0], value[1], value[2]});
  }
}
