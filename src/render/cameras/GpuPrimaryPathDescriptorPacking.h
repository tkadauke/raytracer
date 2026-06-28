#pragma once

#include "core/math/Vector.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace render::detail {
  inline std::uint32_t checkedU32(std::uint64_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
    }
    return static_cast<std::uint32_t>(value);
  }

  inline std::array<float, 4> gpuFloat4(double x, double y = 0.0, double z = 0.0,
                                        double w = 0.0) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
            static_cast<float>(w)};
  }

  inline std::array<float, 4> vector4(const Vector3d& value, float w) {
    return value.toFloat4(w);
  }

  inline std::array<float, 4> parameters4(double x, double y, double z = 0.0, double w = 0.0) {
    return gpuFloat4(x, y, z, w);
  }
}
