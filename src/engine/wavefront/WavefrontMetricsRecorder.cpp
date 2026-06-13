#include "engine/wavefront/detail/WavefrontMetricsRecorder.h"

#include "engine/wavefront/detail/WavefrontTileTypes.h"
#include "render/Integrator.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/denoise/Denoiser.h"

#include <algorithm>

namespace engine::wavefront::detail {
  void WavefrontMetricsRecorder::reset(
    const render::Camera& camera, int width, int height, const render::TilePlan& tilePlan,
    int configuredQueueSize, const render::Integrator& integrator, const render::Denoiser* denoiser,
    std::uint64_t expectedIntersectionRays, std::uint64_t expectedClosestHitIntersectionRays,
    std::uint64_t expectedAnyHitIntersectionRays, std::uint64_t autoMinimumGpuIntersectionRays,
    std::uint64_t autoEstimatedQueryTransferBytes, std::optional<std::uint64_t> samplingSeed,
    const std::string& sampleStreamMode, bool convergenceEnabled,
    double activeSampleFractionThreshold, double radianceDeltaRmsThreshold,
    bool adaptiveSamplingEnabled, int adaptiveMinimumSamples, double adaptiveStddevThreshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics = WavefrontRenderMetrics();
    m_metrics.input.width = width;
    m_metrics.input.height = height;
    m_metrics.input.samplesPerPixel = camera.samplesPerPixel();
    m_metrics.input.samplingSeed = samplingSeed;
    m_metrics.input.sampleStreamMode = sampleStreamMode;
    m_metrics.accumulation.diagnostics = render::TracingAccumulationDiagnostics::forLayout(
      render::TracingAccumulationLayout::image(width, height), "cpu_wavefront_tile",
      "cpu_tile_local");
    m_metrics.tiling.resetFromTilePlan(tilePlan);
    m_metrics.scheduling.configuredQueueSize =
      static_cast<std::uint64_t>(std::max(0, configuredQueueSize));
    m_metrics.scheduling.resolvedQueueSize = tilePlan.size();
    m_metrics.scheduling.decision = tilePlan.isSingleTile() ? "single_tile" : "tiled";
    m_metrics.batching.integrator = integrator.diagnosticName();
    m_metrics.batching.executionMode = integrator.batchExecutionMode();
    m_metrics.batching.intersectionBackendExpectedRays = expectedIntersectionRays;
    m_metrics.batching.intersectionBackendExpectedClosestHitRays =
      expectedClosestHitIntersectionRays;
    m_metrics.batching.intersectionBackendExpectedAnyHitRays = expectedAnyHitIntersectionRays;
    m_metrics.batching.intersectionBackendAutoMinimumGpuRays = autoMinimumGpuIntersectionRays;
    m_metrics.batching.intersectionBackendAutoEstimatedQueryTransferBytes =
      autoEstimatedQueryTransferBytes;
    if (denoiser) {
      const render::DenoiserDiagnostics diagnostics = denoiser->diagnostics();
      m_metrics.denoise.enabled = true;
      m_metrics.denoise.denoiser = diagnostics.name;
      if (denoiser->requestedFeatures().any()) {
        m_metrics.denoise.featureTileCount = tilePlan.size();
      }
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
    m_metrics.adaptiveSampling.enabled = adaptiveSamplingEnabled;
    m_metrics.adaptiveSampling.minimumSamples = std::max(1, adaptiveMinimumSamples);
    m_metrics.adaptiveSampling.stddevThreshold = std::max(0.0, adaptiveStddevThreshold);
  }

  void WavefrontMetricsRecorder::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics = WavefrontRenderMetrics();
  }

  void WavefrontMetricsRecorder::recordTile(const WavefrontTileTraceResult& result) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.input.renderedPixels += result.pixels.size();
    m_metrics.input.primarySamples += result.sampleCount;
    if (result.sampleCount == 0) {
      return;
    }

    ++m_metrics.tiling.nonEmptyTileCount;
    const std::uint64_t tileSamples = static_cast<std::uint64_t>(result.sampleCount);
    if (m_metrics.tiling.minNonEmptyTileSamples == 0 ||
        tileSamples < m_metrics.tiling.minNonEmptyTileSamples) {
      m_metrics.tiling.minNonEmptyTileSamples = tileSamples;
    }
    m_metrics.tiling.maxTileSamples = std::max(m_metrics.tiling.maxTileSamples, tileSamples);
    ++m_metrics.batching.batches;
    m_metrics.timings.sampleGenerationWorkerSeconds += result.sampleGenerationWorkerSeconds;
    m_metrics.timings.sampleStreamWorkerSeconds += result.sampleStreamWorkerSeconds;
    m_metrics.timings.primaryRayWorkerSeconds += result.primaryRayWorkerSeconds;
    m_metrics.timings.sampleEnqueueWorkerSeconds += result.sampleEnqueueWorkerSeconds;
    m_metrics.timings.recordIntegratorBatch(result.integratorBatchWorkerSeconds,
                                            result.batchMetrics);
    m_metrics.batching.samplesSubmitted += result.sampleCount;
    m_metrics.batching.addIntegratorMetrics(result.batchMetrics);
    m_metrics.batching.sampleVariancePixelArea += result.sampleVariancePixelArea;
    m_metrics.batching.sampleRadianceVarianceSum += result.sampleRadianceVarianceSum;
    m_metrics.batching.maxSampleRadianceStddev =
      std::max(m_metrics.batching.maxSampleRadianceStddev, result.maxSampleRadianceStddev);
    m_metrics.batching.maxBatchSize =
      std::max(m_metrics.batching.maxBatchSize, static_cast<std::uint64_t>(result.sampleCount));
    m_metrics.convergence.feedbackDepthCount +=
      result.batchMetrics.observerConvergenceFeedbackDepths;
    if (result.batchMetrics.stoppedByConvergence) {
      m_metrics.convergence.recordStoppedTileAfterDepth(result.batchMetrics.stoppedAfterDepth);
    }
  }

  void WavefrontMetricsRecorder::recordDenoiserFeatureTile(const Recti& rect) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_metrics.denoise.completedFeatureTileCount;
    m_metrics.denoise.featurePixels += static_cast<std::uint64_t>(std::max(0, rect.width())) *
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
    m_metrics.tiling.averageNonEmptyTileSamples =
      m_metrics.tiling.nonEmptyTileCount == 0
        ? 0.0
        : static_cast<double>(m_metrics.input.primarySamples) /
            static_cast<double>(m_metrics.tiling.nonEmptyTileCount);
    if (!m_metrics.convergence.enabled) {
      m_metrics.convergence.decision = "disabled";
    } else if (m_metrics.convergence.stoppedTileCount > 0) {
      m_metrics.convergence.decision = "stopped_some_tiles";
    } else {
      m_metrics.convergence.decision = "not_reached";
    }
    m_metrics.adaptiveSampling.maximumPrimarySamples =
      m_metrics.input.renderedPixels *
      static_cast<std::uint64_t>(std::max(0, m_metrics.input.samplesPerPixel));
    if (m_metrics.adaptiveSampling.enabled &&
        m_metrics.adaptiveSampling.maximumPrimarySamples > m_metrics.input.primarySamples) {
      m_metrics.adaptiveSampling.skippedPrimarySamples =
        m_metrics.adaptiveSampling.maximumPrimarySamples - m_metrics.input.primarySamples;
    } else {
      m_metrics.adaptiveSampling.skippedPrimarySamples = 0;
    }
    m_metrics.adaptiveSampling.skippedPrimarySampleFraction =
      m_metrics.adaptiveSampling.maximumPrimarySamples == 0
        ? 0.0
        : static_cast<double>(m_metrics.adaptiveSampling.skippedPrimarySamples) /
            static_cast<double>(m_metrics.adaptiveSampling.maximumPrimarySamples);
    m_metrics.accumulation.diagnostics.clearOperations = m_metrics.tiling.nonEmptyTileCount;
    m_metrics.accumulation.diagnostics.addOperations = m_metrics.input.primarySamples;
    m_metrics.accumulation.diagnostics.addedSamples = m_metrics.input.primarySamples;
    m_metrics.accumulation.diagnostics.resolveOperations = m_metrics.input.renderedPixels;
    m_metrics.accumulation.diagnostics.readbackOperations = 0;
    m_metrics.accumulation.diagnostics.readbackBytes = 0;
  }

  WavefrontRenderMetrics WavefrontMetricsRecorder::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
  }
}
