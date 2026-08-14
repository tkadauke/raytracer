#include "render/denoise/BoxDenoiser.h"

#include "core/Buffer.h"
#include "core/util/BufferUtils.h"

#include <algorithm>

namespace render {
  BoxDenoiser::BoxDenoiser(int radius)
      : m_radius(1) {
    setRadius(radius);
  }

  std::unique_ptr<Denoiser> BoxDenoiser::clone() const {
    return std::make_unique<BoxDenoiser>(m_radius);
  }

  const char* BoxDenoiser::diagnosticName() const {
    return "box";
  }

  DenoiserDiagnostics BoxDenoiser::diagnostics() const {
    return DenoiserDiagnostics{
      diagnosticName(),
      {DenoiserDiagnostics::NumericParameter{"radius", static_cast<double>(m_radius)}}};
  }

  void BoxDenoiser::denoiseFrame(DenoiserFrame& frame) const {
    Buffer<Colord>& buffer = frame.beauty;
    if (shouldSkipDenoise(m_radius, buffer)) {
      return;
    }

    Buffer<Colord> source(buffer.width(), buffer.height());
    core::util::copyBuffer(source, buffer);

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        Colord sum = Colord::black();
        int count = 0;
        int y0, y1, x0, x1;
        clampedRange(y, m_radius, buffer.height(), y0, y1);
        clampedRange(x, m_radius, buffer.width(), x0, x1);
        for (int sy = y0; sy <= y1; ++sy) {
          for (int sx = x0; sx <= x1; ++sx) {
            sum += source[sy][sx];
            ++count;
          }
        }
        buffer[y][x] = sum * (1.0 / static_cast<double>(count));
      }
    }
  }

  void BoxDenoiser::setRadius(int radius) {
    m_radius = std::max(0, radius);
  }

  int BoxDenoiser::radius() const {
    return m_radius;
  }
}
