#include "render/denoise/Denoiser.h"

#include "core/Buffer.h"

#include <algorithm>

namespace render {
  bool Denoiser::shouldSkipDenoise(int radius, const Buffer<Colord>& buffer) {
    return radius <= 0 || buffer.width() <= 0 || buffer.height() <= 0;
  }

  void Denoiser::clampedRange(int center, int radius, int extent, int& lo, int& hi) {
    lo = std::max(0, center - radius);
    hi = std::min(extent - 1, center + radius);
  }
}
