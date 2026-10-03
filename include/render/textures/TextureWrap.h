#pragma once

#include "core/Color.h"

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

  /// The lower texel index and fractional offset for bilinear filtering of a
  /// pixel-space coordinate (already offset by -0.5 so integer coordinates
  /// land on texel centers). The caller is responsible for wrapping `lower`
  /// and `lower + 1` into `[0, size)`.
  struct BilinearTexelOffset {
    int lower;
    double fraction;
  };

  inline BilinearTexelOffset bilinearTexelOffset(double pixelSpaceCoordinate) {
    const int lower = static_cast<int>(std::floor(pixelSpaceCoordinate));
    return {lower, pixelSpaceCoordinate - lower};
  }

  /// Blend the four texel corners of a bilinear filter footprint, given the
  /// fractional offsets along each axis.
  inline Colord bilinearBlend(const Colord& c00, const Colord& c10, const Colord& c01,
                              const Colord& c11, double tx, double ty) {
    return c00.lerp(c10, tx).lerp(c01.lerp(c11, tx), ty);
  }

}
