#include "render/denoise/BoxDenoiser.h"

#include "core/Buffer.h"

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

  void BoxDenoiser::denoise(Buffer<Colord>& buffer) const {
    if (m_radius <= 0 || buffer.width() <= 0 || buffer.height() <= 0) {
      return;
    }

    Buffer<Colord> source(buffer.width(), buffer.height());
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        source[y][x] = buffer[y][x];
      }
    }

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        Colord sum = Colord::black();
        int count = 0;
        const int y0 = std::max(0, y - m_radius);
        const int y1 = std::min(buffer.height() - 1, y + m_radius);
        const int x0 = std::max(0, x - m_radius);
        const int x1 = std::min(buffer.width() - 1, x + m_radius);
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
