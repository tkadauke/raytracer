#include "engine/raster/detail/OpenGLShadowTextureData.h"

#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/RasterShadowMaps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>

namespace engine::raster::detail {

  OpenGLShadowTextureData::OpenGLShadowTextureData()
      : OpenGLShadowTextureData(0, 0, 1.0) {
  }

  OpenGLShadowTextureData OpenGLShadowTextureData::from(const OpenGLShadowSamplingPlan& plan) {
    if (!plan.enabled() || !plan.cascade() || !plan.cascade()->depthBuffer) {
      return OpenGLShadowTextureData();
    }

    const Buffer<double>& depth = *plan.cascade()->depthBuffer;
    OpenGLShadowTextureData data(depth.width(), depth.height(), 1.0);
    data.m_bias = plan.shadowMap()->bias();
    data.m_filterRadius = plan.shadowMap()->filterRadius();
    data.m_origin = plan.cascade()->camera->origin();
    data.m_right = plan.cascade()->camera->right();
    data.m_up = plan.cascade()->camera->up();
    data.m_forward = plan.cascade()->camera->forward();
    data.m_halfExtent = plan.cascade()->camera->halfExtent();
    for (int y = 0; y != depth.height(); ++y) {
      for (int x = 0; x != depth.width(); ++x) {
        const double value = depth[y][x];
        if (std::isfinite(value)) {
          data.m_depthScale = std::max(data.m_depthScale, std::max(0.0, value) + 1.0);
        }
      }
    }

    data.m_rgbaPixels.reserve(static_cast<std::size_t>(depth.width() * depth.height() * 4));
    for (int y = 0; y != depth.height(); ++y) {
      for (int x = 0; x != depth.width(); ++x) {
        data.appendTexel(depth[y][x]);
      }
    }
    return data;
  }

  bool OpenGLShadowTextureData::enabled() const {
    return m_width > 0 && m_height > 0 && !m_rgbaPixels.empty();
  }

  int OpenGLShadowTextureData::width() const {
    return m_width;
  }

  int OpenGLShadowTextureData::height() const {
    return m_height;
  }

  double OpenGLShadowTextureData::depthScale() const {
    return m_depthScale;
  }

  double OpenGLShadowTextureData::bias() const {
    return m_bias;
  }

  int OpenGLShadowTextureData::filterRadius() const {
    return m_filterRadius;
  }

  const Vector3d& OpenGLShadowTextureData::origin() const {
    return m_origin;
  }

  const Vector3d& OpenGLShadowTextureData::right() const {
    return m_right;
  }

  const Vector3d& OpenGLShadowTextureData::up() const {
    return m_up;
  }

  const Vector3d& OpenGLShadowTextureData::forward() const {
    return m_forward;
  }

  double OpenGLShadowTextureData::halfExtent() const {
    return m_halfExtent;
  }

  std::size_t OpenGLShadowTextureData::uploadByteSize() const {
    return m_rgbaPixels.size() * sizeof(float);
  }

  std::string OpenGLShadowTextureData::traceMessage() const {
    if (!enabled()) {
      return {};
    }

    std::ostringstream message;
    message << "OpenGL raster shadow texture prepared " << m_width << "x" << m_height
            << " normalized depth texels (" << uploadByteSize()
            << " upload bytes) for shader-side binding";
    return message.str();
  }

  const std::vector<float>& OpenGLShadowTextureData::rgbaPixels() const {
    return m_rgbaPixels;
  }

  OpenGLShadowTextureData::OpenGLShadowTextureData(int width, int height, double depthScale)
      : m_width(width),
        m_height(height),
        m_depthScale(std::max(1.0, depthScale)) {
  }

  float OpenGLShadowTextureData::normalizedDepth(double depth) const {
    if (!std::isfinite(depth)) {
      return kNoOccluderDepth;
    }
    const double normalized = std::max(0.0, depth) / m_depthScale;
    return static_cast<float>(
      std::clamp(normalized, 0.0, static_cast<double>(kMaximumOccluderDepth)));
  }

  void OpenGLShadowTextureData::appendTexel(double depth) {
    const float value = normalizedDepth(depth);
    m_rgbaPixels.push_back(value);
    m_rgbaPixels.push_back(value);
    m_rgbaPixels.push_back(value);
    m_rgbaPixels.push_back(1.0f);
  }

}
