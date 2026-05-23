#include "render/postprocess/Smaa.h"

#include "core/Buffer.h"

#include <algorithm>
#include <cmath>

namespace {
  double luma(const Colord& color) {
    return color.r() * 0.299 + color.g() * 0.587 + color.b() * 0.114;
  }

  Colord sampleClamped(const Buffer<Colord>& buffer, int x, int y) {
    x = std::clamp(x, 0, buffer.width() - 1);
    y = std::clamp(y, 0, buffer.height() - 1);
    return buffer[y][x];
  }

  double edgeSpan(const Buffer<Colord>& source, int x, int y, bool verticalEdge,
                  double centerLuma) {
    constexpr int maxSearchSteps = 8;
    constexpr double searchThreshold = 1.0 / 16.0;

    int negativeSteps = 0;
    int positiveSteps = 0;
    for (int step = 1; step <= maxSearchSteps; ++step) {
      const int sx = verticalEdge ? x : x - step;
      const int sy = verticalEdge ? y - step : y;
      if (std::abs(luma(sampleClamped(source, sx, sy)) - centerLuma) < searchThreshold)
        break;
      negativeSteps = step;
    }
    for (int step = 1; step <= maxSearchSteps; ++step) {
      const int sx = verticalEdge ? x : x + step;
      const int sy = verticalEdge ? y + step : y;
      if (std::abs(luma(sampleClamped(source, sx, sy)) - centerLuma) < searchThreshold)
        break;
      positiveSteps = step;
    }
    return static_cast<double>(negativeSteps + positiveSteps + 1);
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
  constexpr double maxBlend = 0.55;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const double center = luma(source[y][x]);
      const double left = luma(source[y][x - 1]);
      const double right = luma(source[y][x + 1]);
      const double up = luma(source[y - 1][x]);
      const double down = luma(source[y + 1][x]);

      const double horizontalDelta = std::max(std::abs(center - left), std::abs(center - right));
      const double verticalDelta = std::max(std::abs(center - up), std::abs(center - down));
      const double edgeDelta = std::max(horizontalDelta, verticalDelta);
      if (edgeDelta < edgeThreshold)
        continue;

      const bool verticalEdge = horizontalDelta >= verticalDelta;
      const Colord negativeSide = verticalEdge ? source[y][x - 1] : source[y - 1][x];
      const Colord positiveSide = verticalEdge ? source[y][x + 1] : source[y + 1][x];
      const Colord crossEdgeAverage = (negativeSide + positiveSide) * 0.5;

      const double span = edgeSpan(source, x, y, verticalEdge, center);
      const double blend = std::clamp(edgeDelta * (0.35 + span / 32.0), 0.0, maxBlend);
      buffer[y][x] = source[y][x] * (1.0 - blend) + crossEdgeAverage * blend;
    }
  }
}
