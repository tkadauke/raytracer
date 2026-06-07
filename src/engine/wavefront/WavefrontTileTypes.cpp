#include "engine/wavefront/detail/WavefrontTileTypes.h"

#include <cmath>

namespace engine::wavefront::detail {
  void WavefrontTileTraceResult::recordSampleVariance(
    const std::vector<Colord>& sampleColors, const std::vector<std::size_t>& samplePixelIndices) {
    if (pixels.empty() || sampleColors.empty()) {
      return;
    }

    std::vector<Colord> sums(pixels.size(), Colord::black());
    std::vector<std::uint64_t> counts(pixels.size(), 0);
    const std::size_t count = std::min(sampleColors.size(), samplePixelIndices.size());
    for (std::size_t index = 0; index != count; ++index) {
      const std::size_t pixelIndex = samplePixelIndices[index];
      if (pixelIndex >= pixels.size()) {
        continue;
      }
      sums[pixelIndex] += sampleColors[index];
      ++counts[pixelIndex];
    }

    std::vector<Colord> means(pixels.size(), Colord::black());
    for (std::size_t pixelIndex = 0; pixelIndex != pixels.size(); ++pixelIndex) {
      if (counts[pixelIndex] > 0) {
        means[pixelIndex] = sums[pixelIndex] * (1.0 / static_cast<double>(counts[pixelIndex]));
      }
    }

    std::vector<double> varianceSums(pixels.size(), 0.0);
    for (std::size_t index = 0; index != count; ++index) {
      const std::size_t pixelIndex = samplePixelIndices[index];
      if (pixelIndex >= pixels.size() || counts[pixelIndex] <= 1) {
        continue;
      }
      const Colord delta = sampleColors[index] - means[pixelIndex];
      varianceSums[pixelIndex] +=
        delta.r() * delta.r() + delta.g() * delta.g() + delta.b() * delta.b();
    }

    for (std::size_t pixelIndex = 0; pixelIndex != pixels.size(); ++pixelIndex) {
      if (counts[pixelIndex] <= 1) {
        continue;
      }
      const int area = std::max(1, pixels[pixelIndex].area());
      const double variance =
        varianceSums[pixelIndex] * (1.0 / static_cast<double>(counts[pixelIndex]));
      sampleVariancePixelArea += static_cast<std::uint64_t>(area);
      sampleRadianceVarianceSum += variance * static_cast<double>(area);
      maxSampleRadianceStddev = std::max(maxSampleRadianceStddev, std::sqrt(variance));
    }
  }

  WavefrontDenoiserFeatureSet::WavefrontDenoiserFeatureSet(
    int width, int height, render::DenoiserFeatureRequest requestedFeatures)
      : m_requestedFeatures(requestedFeatures),
        m_width(width),
        m_height(height) {
    if (m_requestedFeatures.albedo) {
      m_albedo = std::make_unique<Buffer<Colord>>(width, height);
      m_albedo->clear(Colord::black());
    }
    if (m_requestedFeatures.normal) {
      m_normal = std::make_unique<Buffer<Vector3d>>(width, height);
      m_normal->clear(Vector3d::null);
    }
    if (m_requestedFeatures.depth) {
      m_depth = std::make_unique<Buffer<double>>(width, height);
      m_depth->clear(0.0);
    }
  }

  const render::DenoiserFeatureRequest& WavefrontDenoiserFeatureSet::requestedFeatures() const {
    return m_requestedFeatures;
  }

  render::DenoiserFeatureBuffers WavefrontDenoiserFeatureSet::buffers() const {
    return render::DenoiserFeatureBuffers{m_albedo.get(), m_normal.get(), m_depth.get()};
  }

  bool WavefrontDenoiserFeatureSet::hasSourcePixel(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
  }

  void WavefrontDenoiserFeatureSet::write(const Recti& footprint, const Colord& albedo,
                                          const Vector3d& normal, double depth) {
    for (int y = footprint.top(); y != footprint.bottom(); ++y) {
      for (int x = footprint.left(); x != footprint.right(); ++x) {
        if (m_albedo) {
          (*m_albedo)[y][x] = albedo;
        }
        if (m_normal) {
          (*m_normal)[y][x] = normal;
        }
        if (m_depth) {
          (*m_depth)[y][x] = depth;
        }
      }
    }
  }

  void WavefrontDenoiserFeatureSet::copyPixelFrom(const WavefrontDenoiserFeatureSet& source,
                                                  int targetX, int targetY, int sourceX,
                                                  int sourceY) {
    if (m_albedo) {
      const auto sourceBuffers = source.buffers();
      if (sourceBuffers.albedo) {
        (*m_albedo)[targetY][targetX] = (*sourceBuffers.albedo)[sourceY][sourceX];
      }
    }
    if (m_normal) {
      const auto sourceBuffers = source.buffers();
      if (sourceBuffers.normal) {
        (*m_normal)[targetY][targetX] = (*sourceBuffers.normal)[sourceY][sourceX];
      }
    }
    if (m_depth) {
      const auto sourceBuffers = source.buffers();
      if (sourceBuffers.depth) {
        (*m_depth)[targetY][targetX] = (*sourceBuffers.depth)[sourceY][sourceX];
      }
    }
  }
}
