#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace render {
  using GpuFloat4 = std::array<float, 4>;

  [[nodiscard]] inline double gpuFloat4MaxColor(const GpuFloat4& value) {
    return std::max({0.0f, value[0], value[1], value[2]});
  }

  [[nodiscard]] inline bool gpuFloat4HasValue(const GpuFloat4& value) {
    return std::any_of(value.begin(), value.end(),
                       [](float component) { return std::fabs(component) > 1.0e-8f; });
  }
}
