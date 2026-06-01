#include "engine/wavefront/detail/WavefrontTileTypes.h"

namespace engine::wavefront::detail {
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
