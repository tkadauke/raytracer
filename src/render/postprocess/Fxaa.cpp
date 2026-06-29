#include "render/postprocess/Fxaa.h"

#include "core/Buffer.h"

#include <algorithm>
#include <cmath>

namespace {
  Colord sampleBilinear(const Buffer<Colord>& buffer, double x, double y) {
    x = std::clamp(x, 0.0, static_cast<double>(buffer.width() - 1));
    y = std::clamp(y, 0.0, static_cast<double>(buffer.height() - 1));

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, buffer.width() - 1);
    const int y1 = std::min(y0 + 1, buffer.height() - 1);
    const double tx = x - x0;
    const double ty = y - y0;

    const Colord top = buffer[y0][x0] * (1.0 - tx) + buffer[y0][x1] * tx;
    const Colord bottom = buffer[y1][x0] * (1.0 - tx) + buffer[y1][x1] * tx;
    return top * (1.0 - ty) + bottom * ty;
  }
}

void render::postprocess::applyFxaa(Buffer<Colord>& buffer) {
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

  constexpr double edgeThresholdMin = 1.0 / 32.0;
  constexpr double edgeThreshold = 1.0 / 8.0;
  constexpr double reduceMin = 1.0 / 128.0;
  constexpr double reduceMul = 1.0 / 8.0;
  constexpr double spanMax = 8.0;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const double lumaM = source[y][x].luminance();
      const double lumaNW = source[y - 1][x - 1].luminance();
      const double lumaNE = source[y - 1][x + 1].luminance();
      const double lumaSW = source[y + 1][x - 1].luminance();
      const double lumaSE = source[y + 1][x + 1].luminance();

      const double minLuma = std::min({lumaM, lumaNW, lumaNE, lumaSW, lumaSE});
      const double maxLuma = std::max({lumaM, lumaNW, lumaNE, lumaSW, lumaSE});
      if (maxLuma - minLuma < std::max(edgeThresholdMin, maxLuma * edgeThreshold))
        continue;

      double dirX = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
      double dirY = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
      const double dirReduce =
        std::max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * reduceMul, reduceMin);
      const double rcpDirMin = 1.0 / (std::min(std::abs(dirX), std::abs(dirY)) + dirReduce);
      dirX = std::clamp(dirX * rcpDirMin, -spanMax, spanMax);
      dirY = std::clamp(dirY * rcpDirMin, -spanMax, spanMax);

      const Colord rgbA =
        (sampleBilinear(source, x + dirX * (1.0 / 3.0 - 0.5), y + dirY * (1.0 / 3.0 - 0.5)) +
         sampleBilinear(source, x + dirX * (2.0 / 3.0 - 0.5), y + dirY * (2.0 / 3.0 - 0.5))) *
        0.5;
      const Colord rgbB = rgbA * 0.5 + (sampleBilinear(source, x + dirX * -0.5, y + dirY * -0.5) +
                                        sampleBilinear(source, x + dirX * 0.5, y + dirY * 0.5)) *
                                         0.25;
      const double lumaB = rgbB.luminance();
      buffer[y][x] = (lumaB < minLuma || lumaB > maxLuma) ? rgbA : rgbB;
    }
  }
}
