#include "engine/wavefront/detail/WavefrontMetricsRecorder.h"

#include "engine/wavefront/detail/WavefrontTileTypes.h"
#include "render/Integrator.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/denoise/Denoiser.h"

#include <algorithm>

namespace engine::wavefront::detail {
  void WavefrontMetricsRecorder::reset(const render::Camera& camera, int width, int height,
                                       const render::TilePlan& tilePlan, int configuredQueueSize,
                                       const render::Integrator& integrator,
                                       const render::Denoiser* denoiser, bool convergenceEnabled,
                                       double activeSampleFractionThreshold,
                                       double radianceDeltaRmsThreshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics = WavefrontRenderMetrics();
    m_metrics.input.width = width;
    m_metrics.input.height = height;
    m_metrics.input.samplesPerPixel = camera.samplesPerPixel();
    m_metrics.tiling.tileCount = tilePlan.size();
    m_metrics.scheduling.configuredQueueSize =
      static_cast<std::uint64_t>(std::max(0, configuredQueueSize));
    m_metrics.scheduling.resolvedQueueSize = tilePlan.size();
    m_metrics.scheduling.decision = tilePlan.isSingleTile() ? "single_tile" : "tiled";
    m_metrics.batching.integrator = integrator.diagnosticName();
    m_metrics.batching.executionMode = integrator.batchExecutionMode();
    if (denoiser) {
      const render::DenoiserDiagnostics diagnostics = denoiser->diagnostics();
      m_metrics.denoise.enabled = true;
      m_metrics.denoise.denoiser = diagnostics.name;
      m_metrics.denoise.featureTileCount = tilePlan.size();
      for (const auto& parameter : diagnostics.numericParameters) {
        m_metrics.denoise.numericParameters.push_back(
          WavefrontRenderMetrics::DenoiseSummary::NumericParameter{parameter.name,
                                                                   parameter.value});
      }
    }
    m_metrics.convergence.enabled = convergenceEnabled;
    m_metrics.convergence.activeSampleFractionThreshold = activeSampleFractionThreshold;
    m_metrics.convergence.radianceDeltaRmsThreshold = radianceDeltaRmsThreshold;
    m_metrics.convergence.decision = convergenceEnabled ? "configured" : "disabled";
  }

  void WavefrontMetricsRecorder::recordTile(const WavefrontTileTraceResult& result) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.input.renderedPixels += result.pixels.size();
    m_metrics.input.primarySamples += result.sampleCount;
    if (result.sampleCount == 0) {
      return;
    }

    ++m_metrics.tiling.nonEmptyTileCount;
    ++m_metrics.batching.batches;
    m_metrics.batching.samplesSubmitted += result.sampleCount;
    m_metrics.batching.compatibilityShadeSamples += result.batchMetrics.compatibilityShadeSamples;
    m_metrics.batching.maxBatchSize =
      std::max(m_metrics.batching.maxBatchSize, static_cast<std::uint64_t>(result.sampleCount));
    if (m_metrics.batching.activeSamplesPerDepth.size() <
        result.batchMetrics.activeSamplesPerDepth.size()) {
      m_metrics.batching.activeSamplesPerDepth.resize(
        result.batchMetrics.activeSamplesPerDepth.size());
    }
    for (std::size_t depth = 0; depth != result.batchMetrics.activeSamplesPerDepth.size();
         ++depth) {
      m_metrics.batching.activeSamplesPerDepth[depth] +=
        result.batchMetrics.activeSamplesPerDepth[depth];
    }
    if (m_metrics.batching.radianceDeltaSquaredSumPerDepth.size() <
        result.batchMetrics.radianceDeltaSquaredSumPerDepth.size()) {
      m_metrics.batching.radianceDeltaSquaredSumPerDepth.resize(
        result.batchMetrics.radianceDeltaSquaredSumPerDepth.size());
    }
    for (std::size_t depth = 0; depth != result.batchMetrics.radianceDeltaSquaredSumPerDepth.size();
         ++depth) {
      m_metrics.batching.radianceDeltaSquaredSumPerDepth[depth] +=
        result.batchMetrics.radianceDeltaSquaredSumPerDepth[depth];
    }
    if (m_metrics.batching.maxRadianceDeltaPerDepth.size() <
        result.batchMetrics.maxRadianceDeltaPerDepth.size()) {
      m_metrics.batching.maxRadianceDeltaPerDepth.resize(
        result.batchMetrics.maxRadianceDeltaPerDepth.size());
    }
    for (std::size_t depth = 0; depth != result.batchMetrics.maxRadianceDeltaPerDepth.size();
         ++depth) {
      m_metrics.batching.maxRadianceDeltaPerDepth[depth] =
        std::max(m_metrics.batching.maxRadianceDeltaPerDepth[depth],
                 result.batchMetrics.maxRadianceDeltaPerDepth[depth]);
    }
    if (result.batchMetrics.stoppedByConvergence) {
      ++m_metrics.convergence.stoppedTileCount;
      const std::uint64_t depth = result.batchMetrics.stoppedAfterDepth;
      if (m_metrics.convergence.earliestStoppedAfterDepth == 0 ||
          depth < m_metrics.convergence.earliestStoppedAfterDepth) {
        m_metrics.convergence.earliestStoppedAfterDepth = depth;
      }
      m_metrics.convergence.latestStoppedAfterDepth =
        std::max(m_metrics.convergence.latestStoppedAfterDepth, depth);
    }
  }

  void WavefrontMetricsRecorder::recordDenoiserFeatureTile(const Recti& rect) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_metrics.denoise.completedFeatureTileCount;
    m_metrics.denoise.featurePixels +=
      static_cast<std::uint64_t>(std::max(0, rect.width())) *
      static_cast<std::uint64_t>(std::max(0, rect.height()));
  }

  void WavefrontMetricsRecorder::recordDenoise(bool albedoFeature, bool normalFeature,
                                               bool depthFeature, double seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.denoise.albedoFeature = albedoFeature;
    m_metrics.denoise.normalFeature = normalFeature;
    m_metrics.denoise.depthFeature = depthFeature;
    m_metrics.denoise.seconds += seconds;
  }

  void WavefrontMetricsRecorder::recordDenoiserFeatureSeconds(Clock::time_point start) {
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.denoise.featureSeconds += seconds;
  }

  void WavefrontMetricsRecorder::finish(Clock::time_point start) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.timings.totalRenderSeconds =
      std::chrono::duration<double>(Clock::now() - start).count();
    m_metrics.batching.averageBatchSize =
      m_metrics.batching.batches == 0 ? 0.0
                                      : static_cast<double>(m_metrics.batching.samplesSubmitted) /
                                          static_cast<double>(m_metrics.batching.batches);
    if (!m_metrics.convergence.enabled) {
      m_metrics.convergence.decision = "disabled";
    } else if (m_metrics.convergence.stoppedTileCount > 0) {
      m_metrics.convergence.decision = "stopped_some_tiles";
    } else {
      m_metrics.convergence.decision = "not_reached";
    }
  }

  WavefrontRenderMetrics WavefrontMetricsRecorder::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
  }
}
