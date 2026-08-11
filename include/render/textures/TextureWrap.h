#pragma once

#include <algorithm>
#include <cmath>

namespace render {

  /// Wrap an integer texel coordinate into `[0, size)`, either clamping to
  /// the edge texel or repeating (with correct handling of negative input).
  inline int wrapTexelCoordinate(int coordinate, int size, bool clampToEdge) {
    if (clampToEdge)
      return std::clamp(coordinate, 0, size - 1);

    int wrapped = coordinate % size;
    if (wrapped < 0)
      wrapped += size;
    return wrapped;
  }

  /// Normalize a texture coordinate, either clamping to `[0, 1]` or wrapping
  /// via its fractional part.
  inline double wrapUnitCoordinate(double coordinate, bool clampToEdge) {
    if (clampToEdge)
      return std::clamp(coordinate, 0.0, 1.0);
    return coordinate - std::floor(coordinate);
  }

}
