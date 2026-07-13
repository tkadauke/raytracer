#pragma once

#include "core/Color.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace engine::graph {

  inline double normalizedComponent(double value, double minimum, double maximum) {
    if (!std::isfinite(value)) {
      return 0.0;
    }
    const double range = maximum - minimum;
    if (range <= 1e-9) {
      return 0.5;
    }
    return std::clamp((value - minimum) / range, 0.0, 1.0);
  }

  inline Colord colorForObjectId(std::uint32_t id) {
    if (id == 0) {
      return Colord::black();
    }

    std::uint32_t hash = id * 2654435761u;
    hash ^= hash >> 16;
    return Colord(static_cast<double>((hash >> 16) & 0xffu) / 255.0,
                  static_cast<double>((hash >> 8) & 0xffu) / 255.0,
                  static_cast<double>(hash & 0xffu) / 255.0);
  }

}
