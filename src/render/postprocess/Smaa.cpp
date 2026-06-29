#include "render/postprocess/Smaa.h"

#include "core/Buffer.h"

#include <algorithm>
#include <cmath>

namespace {
  struct EdgeAxis {
    int negativeDx;
    int negativeDy;
    int positiveDx;
    int positiveDy;
    int spanNegativeDx;
    int spanNegativeDy;
    int spanPositiveDx;
    int spanPositiveDy;
  };

  Colord sampleClamped(const Buffer<Colord>& buffer, int x, int y) {
    x = std::clamp(x, 0, buffer.width() - 1);
    y = std::clamp(y, 0, buffer.height() - 1);
    return buffer[y][x];
  }

  double edgeSpan(const Buffer<Colord>& source, int x, int y, const EdgeAxis& axis,
                  double centerLuma) {
    constexpr int maxSearchSteps = 8;
    constexpr double searchThreshold = 1.0 / 16.0;

    int negativeSteps = 0;
    int positiveSteps = 0;
    for (int step = 1; step <= maxSearchSteps; ++step) {
      const int sx = x + axis.spanNegativeDx * step;
      const int sy = y + axis.spanNegativeDy * step;
      if (std::abs(sampleClamped(source, sx, sy).luminance() - centerLuma) < searchThreshold)
        break;
      negativeSteps = step;
    }
    for (int step = 1; step <= maxSearchSteps; ++step) {
      const int sx = x + axis.spanPositiveDx * step;
      const int sy = y + axis.spanPositiveDy * step;
      if (std::abs(sampleClamped(source, sx, sy).luminance() - centerLuma) < searchThreshold)
        break;
      positiveSteps = step;
    }
    return static_cast<double>(negativeSteps + positiveSteps + 1);
  }

  const EdgeAxis& strongestEdgeAxis(const Buffer<Colord>& source, int x, int y) {
    static const EdgeAxis axes[] = {
      {-1, 0, 1, 0, 0, -1, 0, 1},
      {0, -1, 0, 1, -1, 0, 1, 0},
      {-1, -1, 1, 1, 1, -1, -1, 1},
      {1, -1, -1, 1, -1, -1, 1, 1},
    };

    const EdgeAxis* best = &axes[0];
    double bestContrast = -1.0;
    for (const auto& axis : axes) {
      const double negative =
        sampleClamped(source, x + axis.negativeDx, y + axis.negativeDy).luminance();
      const double positive =
        sampleClamped(source, x + axis.positiveDx, y + axis.positiveDy).luminance();
      const double contrast = std::abs(negative - positive);
      if (contrast > bestContrast) {
        best = &axis;
        bestContrast = contrast;
      }
    }
    return *best;
  }
}

void render::postprocess::applySmaa(Buffer<Colord>& buffer) {
  const int width = buffer.width();
  const int height = buffer.height();
  if (width < 3 || height < 3)
    return;

  Buffer<Colord> source(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      source[y][x] = buffer[y][x];
    }
  }

  constexpr double edgeThreshold = 0.08;
  constexpr double maxBlend = 0.65;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const double center = source[y][x].luminance();

      const EdgeAxis& axis = strongestEdgeAxis(source, x, y);
      const Colord negativeSide = sampleClamped(source, x + axis.negativeDx, y + axis.negativeDy);
      const Colord positiveSide = sampleClamped(source, x + axis.positiveDx, y + axis.positiveDy);
      const double negative = negativeSide.luminance();
      const double positive = positiveSide.luminance();
      const double edgeDelta = std::max(std::abs(center - negative), std::abs(center - positive));
      if (edgeDelta < edgeThreshold)
        continue;

      const Colord crossEdgeAverage = (negativeSide + positiveSide) * 0.5;

      const double span = edgeSpan(source, x, y, axis, center);
      const double blend = std::clamp(edgeDelta * (0.45 + span / 24.0), 0.0, maxBlend);
      buffer[y][x] = source[y][x] * (1.0 - blend) + crossEdgeAverage * blend;
    }
  }
}
