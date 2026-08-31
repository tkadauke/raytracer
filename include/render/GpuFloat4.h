#pragma once

#include "core/math/Vector.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace render {
  using GpuFloat4 = std::array<float, 4>;

  [[nodiscard]] inline GpuFloat4 gpuFloat4(double x, double y = 0.0, double z = 0.0,
                                           double w = 0.0) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
            static_cast<float>(w)};
  }

  template<int Dimensions, class T, class StorageCellType, class Derived>
  [[nodiscard]] inline GpuFloat4
  gpuFloat4(const Vector<Dimensions, T, StorageCellType, Derived>& value, float fill = 0.0f) {
    return value.toFloat4(fill);
  }

  [[nodiscard]] inline GpuFloat4 gpuTransformPoint(const GpuFloat4& row0, const GpuFloat4& row1,
                                                   const GpuFloat4& row2, const GpuFloat4& row3,
                                                   const GpuFloat4& point) {
    return {row0[0] * point[0] + row0[1] * point[1] + row0[2] * point[2] + row0[3] * point[3],
            row1[0] * point[0] + row1[1] * point[1] + row1[2] * point[2] + row1[3] * point[3],
            row2[0] * point[0] + row2[1] * point[1] + row2[2] * point[2] + row2[3] * point[3],
            row3[0] * point[0] + row3[1] * point[1] + row3[2] * point[2] + row3[3] * point[3]};
  }

  [[nodiscard]] inline GpuFloat4 gpuTransformPoint(const std::array<float, 16>& matrix,
                                                   const GpuFloat4& point) {
    return gpuTransformPoint(GpuFloat4{matrix[0], matrix[1], matrix[2], matrix[3]},
                             GpuFloat4{matrix[4], matrix[5], matrix[6], matrix[7]},
                             GpuFloat4{matrix[8], matrix[9], matrix[10], matrix[11]},
                             GpuFloat4{matrix[12], matrix[13], matrix[14], matrix[15]}, point);
  }

  [[nodiscard]] inline GpuFloat4 gpuTransformDirection(const GpuFloat4& row0, const GpuFloat4& row1,
                                                       const GpuFloat4& row2,
                                                       const GpuFloat4& direction) {
    return {row0[0] * direction[0] + row0[1] * direction[1] + row0[2] * direction[2],
            row1[0] * direction[0] + row1[1] * direction[1] + row1[2] * direction[2],
            row2[0] * direction[0] + row2[1] * direction[1] + row2[2] * direction[2], 0.0f};
  }

  [[nodiscard]] inline GpuFloat4 gpuTransformDirection(const std::array<float, 16>& matrix,
                                                       const GpuFloat4& direction) {
    return gpuTransformDirection(GpuFloat4{matrix[0], matrix[1], matrix[2], matrix[3]},
                                 GpuFloat4{matrix[4], matrix[5], matrix[6], matrix[7]},
                                 GpuFloat4{matrix[8], matrix[9], matrix[10], matrix[11]},
                                 direction);
  }

  [[nodiscard]] inline double gpuFloat4MaxColor(const GpuFloat4& value) {
    return std::max({0.0f, value[0], value[1], value[2]});
  }

  [[nodiscard]] inline bool gpuFloat4HasValue(const GpuFloat4& value) {
    return std::any_of(value.begin(), value.end(),
                       [](float component) { return std::fabs(component) > 1.0e-8f; });
  }
}
