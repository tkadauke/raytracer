#pragma once

#include <algorithm>
#include <array>

namespace render {
  using GpuFloat4 = std::array<float, 4>;

  [[nodiscard]] inline double gpuFloat4MaxColor(const GpuFloat4& value) {
    return std::max({0.0f, value[0], value[1], value[2]});
  }
}
