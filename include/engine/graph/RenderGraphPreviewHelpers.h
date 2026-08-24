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

  /// Scans @p source for the per-channel finite min/max, ignoring
  /// non-finite (NaN/Inf) samples. Returns `false` (with @p minimum /
  /// @p maximum left partially finite) if any channel never saw a finite
  /// sample. Shared by the execution-trace and pass-payload preview
  /// builders that fall back to a flat color when a buffer is entirely
  /// non-finite.
  inline bool finiteColorRange(const Buffer<Colord>& source, Colord* minimum, Colord* maximum) {
    double minValues[3] = {std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity()};
    double maxValues[3] = {-std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()};

    for (int y = 0; y != source.height(); ++y) {
      for (int x = 0; x != source.width(); ++x) {
        for (int component = 0; component != 3; ++component) {
          const double value = source[y][x][component];
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
    return std::isfinite(minValues[0]) && std::isfinite(minValues[1]) &&
           std::isfinite(minValues[2]) && std::isfinite(maxValues[0]) &&
           std::isfinite(maxValues[1]) && std::isfinite(maxValues[2]);
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
