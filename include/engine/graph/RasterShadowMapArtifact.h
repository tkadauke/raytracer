#pragma once

#include "core/Buffer.h"
#include "engine/graph/RenderGraphArtifactCache.h"

#include <memory>
#include <string>

namespace engine::raster {
  class Rasterizer;

  namespace detail {
    class ShadowMaps;
  }
}

namespace engine::graph {

  /**
    * Immutable graph artifact containing raster directional shadow maps.
    *
    * The artifact owns the full shadow-map collection consumed by raster beauty
    * passes and also exposes a depth preview for graph trace inspection.
    */
  class RasterShadowMapArtifact : public RenderGraphCachedArtifact {
  public:
    RasterShadowMapArtifact(RenderGraphCacheKey key,
                            std::shared_ptr<const engine::raster::detail::ShadowMaps> shadowMaps,
                            int previewWidth, int previewHeight, std::string description = {});

    std::shared_ptr<const engine::raster::detail::ShadowMaps> shadowMaps() const;

    const Buffer<double>& depthPreview() const;
    bool copyDepthTo(Buffer<double>& destination) const override;
    bool copyRasterShadowMapPreviewTo(Buffer<double>& destination) const override;
    bool applyRasterShadowMapsTo(engine::raster::Rasterizer& rasterizer) const override;

  private:
    std::shared_ptr<const engine::raster::detail::ShadowMaps> m_shadowMaps;
    Buffer<double> m_depthPreview;
  };

}
