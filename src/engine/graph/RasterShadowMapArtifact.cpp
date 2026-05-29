#include "engine/graph/RasterShadowMapArtifact.h"

#include "core/util/BufferUtils.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/RasterShadowMaps.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace engine::graph {

  RasterShadowMapArtifact::RasterShadowMapArtifact(
    RenderGraphCacheKey key, std::shared_ptr<const engine::raster::detail::ShadowMaps> shadowMaps,
    int previewWidth, int previewHeight, std::string description)
      : RenderGraphCachedArtifact(std::move(key), std::move(description)),
        m_shadowMaps(std::move(shadowMaps)),
        m_depthPreview(previewWidth, previewHeight) {
    m_depthPreview.clear(std::numeric_limits<double>::infinity());
    if (!m_shadowMaps) {
      m_shadowMaps = std::make_shared<engine::raster::detail::ShadowMaps>();
    }
    m_shadowMaps->copyFirstDirectionalDepthTo(m_depthPreview);
  }

  std::shared_ptr<const engine::raster::detail::ShadowMaps>
  RasterShadowMapArtifact::shadowMaps() const {
    return m_shadowMaps;
  }

  const Buffer<double>& RasterShadowMapArtifact::depthPreview() const {
    return m_depthPreview;
  }

  bool RasterShadowMapArtifact::copyDepthTo(Buffer<double>& destination) const {
    if (!core::util::bufferDimensionsEqual(destination, m_depthPreview)) {
      throw std::runtime_error(
        "cached raster shadow-map artifact copy requires matching buffer dimensions");
    }

    core::util::copyBuffer(destination, m_depthPreview);
    return true;
  }

  bool RasterShadowMapArtifact::copyRasterShadowMapPreviewTo(Buffer<double>& destination) const {
    return copyDepthTo(destination);
  }

  bool
  RasterShadowMapArtifact::applyRasterShadowMapsTo(engine::raster::Rasterizer& rasterizer) const {
    rasterizer.setExternalShadowMaps(m_shadowMaps);
    return static_cast<bool>(m_shadowMaps);
  }

  bool RasterShadowMapArtifact::applyRasterShadowMapsTo(
    engine::raster::OpenGLRasterizer& rasterizer) const {
    rasterizer.setExternalShadowMaps(m_shadowMaps);
    return static_cast<bool>(m_shadowMaps);
  }

}
