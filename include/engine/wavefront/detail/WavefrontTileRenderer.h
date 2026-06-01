#pragma once

#include "engine/wavefront/detail/WavefrontMetricsRecorder.h"
#include "engine/wavefront/detail/WavefrontTileTypes.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>

class QThreadPool;

namespace engine {
  class TileRenderTask;
}

namespace render {
  class Camera;
  class Denoiser;
  class Integrator;
  class RayCaster;
  class Scene;
  class TilePlan;
  class Tonemap;
}

namespace engine::wavefront::detail {
  struct WavefrontTileRenderConfig {
    render::Integrator& integrator;
    render::Denoiser* denoiser{nullptr};
    bool showProgressIndicators{false};
    bool convergenceEnabled{false};
    double convergenceActiveSampleFractionThreshold{0.0};
    double convergenceRadianceDeltaRmsThreshold{0.0};
    std::optional<std::uint64_t> samplingSeed;
  };

  class WavefrontTileRenderer {
  public:
    WavefrontTileRenderer(WavefrontTileRenderConfig config, WavefrontMetricsRecorder& metrics);

    void renderHdrTile(render::Camera& camera, const render::RayCaster& rayCaster,
                       const render::Scene& scene, Buffer<Colord>& buffer, const Recti& rect,
                       std::optional<std::uint64_t> tileSeed, bool publishProgressSnapshots,
                       const WavefrontDenoiserFeatureSet* denoiserFeatures = nullptr) const;
    void renderDisplayTile(render::Camera& camera, const render::RayCaster& rayCaster,
                           const render::Scene& scene, Buffer<unsigned int>& buffer,
                           std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                           std::optional<std::uint64_t> tileSeed,
                           bool publishProgressSnapshots) const;
    void renderDualOutputTile(render::Camera& camera, const render::RayCaster& rayCaster,
                              const render::Scene& scene, Buffer<Colord>& hdrBuffer,
                              Buffer<unsigned int>& displayBuffer,
                              std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                              std::optional<std::uint64_t> tileSeed,
                              bool publishProgressSnapshots,
                              const WavefrontDenoiserFeatureSet* denoiserFeatures = nullptr) const;

    std::unique_ptr<WavefrontDenoiserFeatureSet>
    buildDenoiserFeatures(render::Camera& camera, const render::Scene& scene, const Recti& rect,
                          const render::TilePlan& tilePlan, QThreadPool& threadPool,
                          std::list<std::shared_ptr<engine::TileRenderTask>>& tasks) const;
    void denoise(Buffer<Colord>& buffer,
                 const WavefrontDenoiserFeatureSet* features = nullptr) const;
    void writeDisplayBuffer(Buffer<unsigned int>& displayBuffer, const Buffer<Colord>& hdrBuffer,
                            std::shared_ptr<render::Tonemap> tonemap) const;

  private:
    WavefrontTileRenderConfig m_config;
    WavefrontMetricsRecorder& m_metrics;
  };
}
