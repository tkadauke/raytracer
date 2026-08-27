#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace engine::graph {

  inline std::runtime_error passError(const RenderPassNode& pass, const std::string& message) {
    return std::runtime_error("pass '" + pass.id + "': " + message);
  }

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

  /// Scans every finite value of a scalar buffer and reports the min/max
  /// range. Returns false (leaving `*minimum`/`*maximum` at +-infinity) if
  /// the buffer has no finite values.
  inline bool finiteRange(const Buffer<double>& buffer, double* minimum, double* maximum) {
    *minimum = std::numeric_limits<double>::infinity();
    *maximum = -std::numeric_limits<double>::infinity();
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        const double value = buffer[y][x];
        if (!std::isfinite(value)) {
          continue;
        }
        *minimum = std::min(*minimum, value);
        *maximum = std::max(*maximum, value);
      }
    }
    return std::isfinite(*minimum) && std::isfinite(*maximum);
  }

  /// Component-wise variant of finiteRange() for color buffers: scans every
  /// finite r/g/b component independently and reports the per-component
  /// min/max range.
  inline bool finiteRange(const Buffer<Colord>& buffer, Colord* minimum, Colord* maximum) {
    double minValues[3] = {std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity()};
    double maxValues[3] = {-std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()};

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        for (int component = 0; component != 3; ++component) {
          const double value = buffer[y][x][component];
          if (!std::isfinite(value)) {
            continue;
          }
          minValues[component] = std::min(minValues[component], value);
          maxValues[component] = std::max(maxValues[component], value);
        }
      }
    }

    *minimum = Colord(minValues);
    *maximum = Colord(maxValues);
    return std::isfinite(minValues[0]) && std::isfinite(minValues[1]) && std::isfinite(minValues[2]) &&
           std::isfinite(maxValues[0]) && std::isfinite(maxValues[1]) && std::isfinite(maxValues[2]);
  }

  /// Maps a depth value to a [0, 1] grayscale brightness within [minDepth,
  /// maxDepth], with nearer depths (closer to minDepth) rendered brighter.
  /// Shared by the depth-visualization render pass and the render-graph
  /// trace preview widget.
  inline double depthGrayscale(double value, double minDepth, double maxDepth) {
    const double range = std::max(maxDepth - minDepth, 1e-9);
    return 1.0 - std::clamp((value - minDepth) / range, 0.0, 1.0);
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
