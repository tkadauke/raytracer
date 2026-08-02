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

  [[nodiscard]] inline double gpuFloat4MaxColor(const GpuFloat4& value) {
    return std::max({0.0f, value[0], value[1], value[2]});
  }

  [[nodiscard]] inline bool gpuFloat4HasValue(const GpuFloat4& value) {
    return std::any_of(value.begin(), value.end(),
                       [](float component) { return std::fabs(component) > 1.0e-8f; });
  }
}
