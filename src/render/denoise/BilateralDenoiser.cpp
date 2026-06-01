#include "render/denoise/BilateralDenoiser.h"

#include "core/Buffer.h"

#include <algorithm>
#include <cmath>

namespace render {
  namespace {
    constexpr double kMinimumColorSigma = 1e-6;
  }

  BilateralDenoiser::BilateralDenoiser(int radius, double colorSigma)
      : m_radius(2),
        m_colorSigma(0.1) {
    setRadius(radius);
    setColorSigma(colorSigma);
  }

  std::unique_ptr<Denoiser> BilateralDenoiser::clone() const {
    return std::make_unique<BilateralDenoiser>(m_radius, m_colorSigma);
  }

  const char* BilateralDenoiser::diagnosticName() const {
    return "bilateral";
  }

  DenoiserDiagnostics BilateralDenoiser::diagnostics() const {
    return DenoiserDiagnostics{
      diagnosticName(),
      {DenoiserDiagnostics::NumericParameter{"radius", static_cast<double>(m_radius)},
       DenoiserDiagnostics::NumericParameter{"color_sigma", m_colorSigma}}};
  }

  void BilateralDenoiser::denoise(Buffer<Colord>& buffer) const {
    if (m_radius <= 0 || buffer.width() <= 0 || buffer.height() <= 0) {
      return;
    }

    Buffer<Colord> source(buffer.width(), buffer.height());
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        source[y][x] = buffer[y][x];
      }
    }

    const double spatialSigma = std::max(0.5, static_cast<double>(m_radius) * 0.5);
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        const Colord center = source[y][x];
        Colord sum = Colord::black();
        double weightSum = 0.0;
        const int y0 = std::max(0, y - m_radius);
        const int y1 = std::min(buffer.height() - 1, y + m_radius);
        const int x0 = std::max(0, x - m_radius);
        const int x1 = std::min(buffer.width() - 1, x + m_radius);
        for (int sy = y0; sy <= y1; ++sy) {
          for (int sx = x0; sx <= x1; ++sx) {
            const double dx = static_cast<double>(sx - x);
            const double dy = static_cast<double>(sy - y);
            const double spatialWeight = gaussian(dx * dx + dy * dy, spatialSigma);
            const double colorWeight =
              gaussian(colorDistanceSquared(source[sy][sx], center), m_colorSigma);
            const double weight = spatialWeight * colorWeight;
            sum += source[sy][sx] * weight;
            weightSum += weight;
          }
        }
        buffer[y][x] = weightSum > 0.0 ? sum * (1.0 / weightSum) : center;
      }
    }
  }

  void BilateralDenoiser::setRadius(int radius) {
    m_radius = std::max(0, radius);
  }

  int BilateralDenoiser::radius() const {
    return m_radius;
  }

  void BilateralDenoiser::setColorSigma(double sigma) {
    m_colorSigma = std::max(kMinimumColorSigma, sigma);
  }

  double BilateralDenoiser::colorSigma() const {
    return m_colorSigma;
  }

  double BilateralDenoiser::colorDistanceSquared(const Colord& a, const Colord& b) {
    double sum = 0.0;
    for (int component = 0; component != 3; ++component) {
      const double delta = a[component] - b[component];
      sum += delta * delta;
    }
    return sum;
  }

  double BilateralDenoiser::gaussian(double distanceSquared, double sigma) {
    return std::exp(-distanceSquared / (2.0 * sigma * sigma));
  }
}
