#include "render/denoise/BilateralDenoiser.h"

#include "core/Buffer.h"

#include <algorithm>
#include <cmath>

namespace render {
  namespace {
    constexpr double kMinimumColorSigma = 1e-6;
    constexpr double kAlbedoSigma = 0.2;
    constexpr double kNormalSigma = 0.25;
    constexpr double kRelativeDepthSigma = 0.02;
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

  DenoiserFeatureRequest BilateralDenoiser::requestedFeatures() const {
    return DenoiserFeatureRequest{true, true, true};
  }

  void BilateralDenoiser::denoiseFrame(DenoiserFrame& frame) const {
    Buffer<Colord>& buffer = frame.beauty;
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
            const double weight = sampleWeight(frame, source, x, y, sx, sy, spatialSigma);
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

  double BilateralDenoiser::sampleWeight(const DenoiserFrame& frame, const Buffer<Colord>& source,
                                         int centerX, int centerY, int sampleX, int sampleY,
                                         double spatialSigma) const {
    const double dx = static_cast<double>(sampleX - centerX);
    const double dy = static_cast<double>(sampleY - centerY);
    const double spatialWeight = gaussian(dx * dx + dy * dy, spatialSigma);
    const double colorWeight = gaussian(
      colorDistanceSquared(source[sampleY][sampleX], source[centerY][centerX]), m_colorSigma);
    return spatialWeight * colorWeight * albedoWeight(frame, centerX, centerY, sampleX, sampleY) *
           normalWeight(frame, centerX, centerY, sampleX, sampleY) *
           depthWeight(frame, centerX, centerY, sampleX, sampleY);
  }

  double BilateralDenoiser::albedoWeight(const DenoiserFrame& frame, int centerX, int centerY,
                                         int sampleX, int sampleY) const {
    const auto* albedo = frame.features.albedo;
    if (!hasFeatureDimensions(albedo, frame)) {
      return 1.0;
    }

    return gaussian(colorDistanceSquared((*albedo)[sampleY][sampleX], (*albedo)[centerY][centerX]),
                    kAlbedoSigma);
  }

  double BilateralDenoiser::normalWeight(const DenoiserFrame& frame, int centerX, int centerY,
                                         int sampleX, int sampleY) const {
    const auto* normal = frame.features.normal;
    if (!hasFeatureDimensions(normal, frame)) {
      return 1.0;
    }

    return gaussian((*normal)[sampleY][sampleX].squaredDistanceTo((*normal)[centerY][centerX]),
                    kNormalSigma);
  }

  double BilateralDenoiser::depthWeight(const DenoiserFrame& frame, int centerX, int centerY,
                                        int sampleX, int sampleY) const {
    const auto* depth = frame.features.depth;
    if (!hasFeatureDimensions(depth, frame)) {
      return 1.0;
    }

    const double centerDepth = (*depth)[centerY][centerX];
    const double sampleDepth = (*depth)[sampleY][sampleX];
    const double scale = std::max({std::abs(centerDepth), std::abs(sampleDepth), 1.0});
    const double relativeDelta = (sampleDepth - centerDepth) / scale;
    return gaussian(relativeDelta * relativeDelta, kRelativeDepthSigma);
  }

  double BilateralDenoiser::colorDistanceSquared(const Colord& a, const Colord& b) const {
    double sum = 0.0;
    for (int component = 0; component != 3; ++component) {
      const double delta = a[component] - b[component];
      sum += delta * delta;
    }
    return sum;
  }

  double BilateralDenoiser::gaussian(double distanceSquared, double sigma) const {
    return std::exp(-distanceSquared / (2.0 * sigma * sigma));
  }

  bool BilateralDenoiser::hasFeatureDimensions(const Buffer<Colord>* buffer,
                                               const DenoiserFrame& frame) const {
    return buffer && buffer->width() == frame.beauty.width() &&
           buffer->height() == frame.beauty.height();
  }

  bool BilateralDenoiser::hasFeatureDimensions(const Buffer<Vector3d>* buffer,
                                               const DenoiserFrame& frame) const {
    return buffer && buffer->width() == frame.beauty.width() &&
           buffer->height() == frame.beauty.height();
  }

  bool BilateralDenoiser::hasFeatureDimensions(const Buffer<double>* buffer,
                                               const DenoiserFrame& frame) const {
    return buffer && buffer->width() == frame.beauty.width() &&
           buffer->height() == frame.beauty.height();
  }
}
