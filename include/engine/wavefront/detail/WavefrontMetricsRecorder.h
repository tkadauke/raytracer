#pragma once

#include "engine/wavefront/WavefrontRaytracer.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace render {
  class Camera;
  class Denoiser;
  class Integrator;
  class TilePlan;
}

namespace engine::wavefront::detail {
  struct WavefrontTileTraceResult;

  class WavefrontMetricsRecorder {
  public:
    using Clock = std::chrono::steady_clock;

    void reset(const render::Camera& camera, int width, int height,
               const render::TilePlan& tilePlan, int configuredQueueSize,
               const render::Integrator& integrator, const render::Denoiser* denoiser,
               std::uint64_t expectedIntersectionRays,
               std::uint64_t expectedClosestHitIntersectionRays,
               std::uint64_t expectedAnyHitIntersectionRays,
               std::uint64_t autoMinimumGpuIntersectionRays,
               std::uint64_t autoEstimatedQueryTransferBytes,
               std::optional<std::uint64_t> samplingSeed, const std::string& sampleStreamMode,
               bool convergenceEnabled, double activeSampleFractionThreshold,
               double radianceDeltaRmsThreshold, bool adaptiveSamplingEnabled,
               int adaptiveMinimumSamples, double adaptiveStddevThreshold);
    void clear();
    void recordTile(const WavefrontTileTraceResult& result);
    void recordDenoiserFeatureTile(const Recti& rect);
    void recordDenoise(bool albedoFeature, bool normalFeature, bool depthFeature, double seconds);
    void recordDenoiserFeatureSeconds(Clock::time_point start);
    void finish(Clock::time_point start);

    WavefrontRenderMetrics snapshot() const;

  private:
    mutable std::mutex m_mutex;
    WavefrontRenderMetrics m_metrics;
  };
}
