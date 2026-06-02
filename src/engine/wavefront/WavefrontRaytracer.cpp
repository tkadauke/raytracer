#include "engine/wavefront/WavefrontRaytracer.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "core/util/BufferUtils.h"
#include "engine/TileRenderTask.h"
#include "engine/wavefront/detail/WavefrontMetricsRecorder.h"
#include "engine/wavefront/detail/WavefrontTileRenderer.h"
#include "render/Integrator.h"
#include "render/RayCaster.h"
#include "render/SamplingSeed.h"
#include "render/Stats.h"
#include "render/TilePlan.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/denoise/Denoiser.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace engine::wavefront {
  namespace {
    class RecursiveRayCasterAdapter : public render::RayCaster {
    public:
      RecursiveRayCasterAdapter(const render::Scene& scene, const render::Integrator& integrator)
          : m_scene(scene),
            m_integrator(integrator) {
      }

      Colord rayColor(const Rayd& ray, render::State& state) const override {
        return m_integrator.radiance(m_scene, ray, state, *this);
      }

    private:
      const render::Scene& m_scene;
      const render::Integrator& m_integrator;
    };
  }

  void WavefrontRenderMetrics::TimingSummary::recordIntegratorBatch(
    double batchSeconds, const render::IntegratorBatchMetrics& batchMetrics) {
    integratorBatchWorkerSeconds += batchSeconds;
    integratorIntersectionWorkerSeconds += batchMetrics.intersectionWorkerSeconds;
    integratorShadingWorkerSeconds += batchMetrics.shadingWorkerSeconds;
    const double overhead = std::max(0.0, batchSeconds - batchMetrics.intersectionWorkerSeconds -
                                            batchMetrics.shadingWorkerSeconds);
    integratorOverheadWorkerSeconds += overhead;
    integratorPathSetupWorkerSeconds += batchMetrics.pathSetupWorkerSeconds;
    integratorFrontierBookkeepingWorkerSeconds += batchMetrics.frontierBookkeepingWorkerSeconds;
    integratorProgressSnapshotWorkerSeconds += batchMetrics.progressSnapshotWorkerSeconds;
    integratorConvergenceTestWorkerSeconds += batchMetrics.convergenceTestWorkerSeconds;
    integratorResidualWorkerSeconds +=
      std::max(0.0, overhead - batchMetrics.pathSetupWorkerSeconds -
                      batchMetrics.frontierBookkeepingWorkerSeconds -
                      batchMetrics.progressSnapshotWorkerSeconds -
                      batchMetrics.convergenceTestWorkerSeconds);
  }

  void WavefrontRenderMetrics::BatchSummary::addIntegratorMetrics(
    const render::IntegratorBatchMetrics& metrics) {
    activeSampleDepthsProcessed += metrics.activeSampleDepthsProcessed;
    compatibilityShadeSamples += metrics.compatibilityShadeSamples;

    const auto addCounts = [](std::vector<std::uint64_t>& target,
                              const std::vector<std::uint64_t>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size());
      }
      for (std::size_t depth = 0; depth != source.size(); ++depth) {
        target[depth] += source[depth];
      }
    };
    addCounts(activeSamplesPerDepth, metrics.activeSamplesPerDepth);
    addCounts(frontierRayHitsPerDepth, metrics.frontierRayHitsPerDepth);
    addCounts(frontierRayMissesPerDepth, metrics.frontierRayMissesPerDepth);
    addCounts(frontierPacketChunksPerDepth, metrics.frontierPacketChunksPerDepth);
    addCounts(frontierPacketRaysPerDepth, metrics.frontierPacketRaysPerDepth);
    addCounts(frontierRay4PacketChunksPerDepth, metrics.frontierRay4PacketChunksPerDepth);
    addCounts(frontierRay8PacketChunksPerDepth, metrics.frontierRay8PacketChunksPerDepth);
    addCounts(frontierScalarRaysPerDepth, metrics.frontierScalarRaysPerDepth);
    addCounts(frontierPacketScalarFallbackRaysPerDepth,
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    for (const auto& [reason, count] : metrics.frontierPacketScalarFallbackRaysByReason) {
      frontierPacketScalarFallbackRaysByReason[reason] += count;
    }
    addCounts(frontierPacketRefinedRaysPerDepth, metrics.frontierPacketRefinedRaysPerDepth);
    for (const auto& [material, count] : metrics.frontierPacketRefinedRaysByMaterial) {
      frontierPacketRefinedRaysByMaterial[material] += count;
    }

    if (radianceDeltaSquaredSumPerDepth.size() < metrics.radianceDeltaSquaredSumPerDepth.size()) {
      radianceDeltaSquaredSumPerDepth.resize(metrics.radianceDeltaSquaredSumPerDepth.size());
    }
    for (std::size_t depth = 0; depth != metrics.radianceDeltaSquaredSumPerDepth.size(); ++depth) {
      radianceDeltaSquaredSumPerDepth[depth] += metrics.radianceDeltaSquaredSumPerDepth[depth];
    }

    if (maxRadianceDeltaPerDepth.size() < metrics.maxRadianceDeltaPerDepth.size()) {
      maxRadianceDeltaPerDepth.resize(metrics.maxRadianceDeltaPerDepth.size());
    }
    for (std::size_t depth = 0; depth != metrics.maxRadianceDeltaPerDepth.size(); ++depth) {
      maxRadianceDeltaPerDepth[depth] =
        std::max(maxRadianceDeltaPerDepth[depth], metrics.maxRadianceDeltaPerDepth[depth]);
    }
  }

  void WavefrontRenderMetrics::TilingSummary::resetFromTilePlan(const render::TilePlan& tilePlan) {
    *this = TilingSummary();
    tileCount = tilePlan.size();
    tileRows = static_cast<std::uint64_t>(std::max(0, tilePlan.rows()));
    tileColumns = static_cast<std::uint64_t>(std::max(0, tilePlan.cols()));
    maxTileWidth = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTileWidth()));
    maxTileHeight = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTileHeight()));
    maxTilePixels = static_cast<std::uint64_t>(std::max(0, tilePlan.maxTilePixels()));
    averageTilePixels = tilePlan.averageTilePixels();
  }

  QJsonObject WavefrontRenderMetrics::toJson() const {
    QJsonObject inputJson;
    inputJson["width"] = input.width;
    inputJson["height"] = input.height;
    inputJson["samplesPerPixel"] = input.samplesPerPixel;
    inputJson["renderedPixels"] = static_cast<double>(input.renderedPixels);
    inputJson["primarySamples"] = static_cast<double>(input.primarySamples);

    QJsonObject tilingJson;
    tilingJson["tileCount"] = static_cast<double>(tiling.tileCount);
    tilingJson["tileRows"] = static_cast<double>(tiling.tileRows);
    tilingJson["tileColumns"] = static_cast<double>(tiling.tileColumns);
    tilingJson["maxTileWidth"] = static_cast<double>(tiling.maxTileWidth);
    tilingJson["maxTileHeight"] = static_cast<double>(tiling.maxTileHeight);
    tilingJson["maxTilePixels"] = static_cast<double>(tiling.maxTilePixels);
    tilingJson["averageTilePixels"] = tiling.averageTilePixels;
    tilingJson["nonEmptyTileCount"] = static_cast<double>(tiling.nonEmptyTileCount);
    tilingJson["minNonEmptyTileSamples"] = static_cast<double>(tiling.minNonEmptyTileSamples);
    tilingJson["maxTileSamples"] = static_cast<double>(tiling.maxTileSamples);
    tilingJson["averageNonEmptyTileSamples"] = tiling.averageNonEmptyTileSamples;

    QJsonObject schedulingJson;
    schedulingJson["configuredQueueSize"] = static_cast<double>(scheduling.configuredQueueSize);
    schedulingJson["resolvedQueueSize"] = static_cast<double>(scheduling.resolvedQueueSize);
    schedulingJson["decision"] = QString::fromStdString(scheduling.decision);

    QJsonObject batchingJson;
    const auto integerArray = [](const std::vector<std::uint64_t>& values) {
      QJsonArray result;
      for (const std::uint64_t value : values) {
        result.push_back(static_cast<double>(value));
      }
      return result;
    };
    const QJsonArray activeSamplesPerDepth = integerArray(batching.activeSamplesPerDepth);
    const QJsonArray frontierRayHitsPerDepth = integerArray(batching.frontierRayHitsPerDepth);
    const QJsonArray frontierRayMissesPerDepth = integerArray(batching.frontierRayMissesPerDepth);
    const QJsonArray frontierPacketChunksPerDepth =
      integerArray(batching.frontierPacketChunksPerDepth);
    const QJsonArray frontierPacketRaysPerDepth = integerArray(batching.frontierPacketRaysPerDepth);
    const QJsonArray frontierRay4PacketChunksPerDepth =
      integerArray(batching.frontierRay4PacketChunksPerDepth);
    const QJsonArray frontierRay8PacketChunksPerDepth =
      integerArray(batching.frontierRay8PacketChunksPerDepth);
    const QJsonArray frontierScalarRaysPerDepth = integerArray(batching.frontierScalarRaysPerDepth);
    const QJsonArray frontierPacketScalarFallbackRaysPerDepth =
      integerArray(batching.frontierPacketScalarFallbackRaysPerDepth);
    const QJsonArray frontierPacketRefinedRaysPerDepth =
      integerArray(batching.frontierPacketRefinedRaysPerDepth);
    const auto integerObject = [](const std::map<std::string, std::uint64_t>& values) {
      QJsonObject result;
      for (const auto& [key, count] : values) {
        result[QString::fromStdString(key)] = static_cast<double>(count);
      }
      return result;
    };
    const QJsonObject frontierPacketScalarFallbackRaysByReason =
      integerObject(batching.frontierPacketScalarFallbackRaysByReason);
    const QJsonObject frontierPacketRefinedRaysByMaterial =
      integerObject(batching.frontierPacketRefinedRaysByMaterial);
    QJsonArray radianceDeltaL2PerDepth;
    QJsonArray radianceDeltaRmsPerDepth;
    for (std::size_t depth = 0; depth != batching.radianceDeltaSquaredSumPerDepth.size(); ++depth) {
      const double squaredSum = batching.radianceDeltaSquaredSumPerDepth[depth];
      radianceDeltaL2PerDepth.push_back(std::sqrt(squaredSum));
      const std::uint64_t activeSamples =
        depth < batching.activeSamplesPerDepth.size() ? batching.activeSamplesPerDepth[depth] : 0;
      radianceDeltaRmsPerDepth.push_back(
        activeSamples == 0 ? 0.0 : std::sqrt(squaredSum / static_cast<double>(activeSamples)));
    }
    QJsonArray maxRadianceDeltaPerDepth;
    for (const double delta : batching.maxRadianceDeltaPerDepth) {
      maxRadianceDeltaPerDepth.push_back(delta);
    }
    batchingJson["integrator"] = QString::fromStdString(batching.integrator);
    batchingJson["executionMode"] = QString::fromStdString(batching.executionMode);
    batchingJson["batches"] = static_cast<double>(batching.batches);
    batchingJson["samplesSubmitted"] = static_cast<double>(batching.samplesSubmitted);
    batchingJson["maxBatchSize"] = static_cast<double>(batching.maxBatchSize);
    batchingJson["averageBatchSize"] = batching.averageBatchSize;
    batchingJson["activeSampleDepthsProcessed"] =
      static_cast<double>(batching.activeSampleDepthsProcessed);
    batchingJson["compatibilityShadeSamples"] =
      static_cast<double>(batching.compatibilityShadeSamples);
    batchingJson["activeSamplesPerDepth"] = activeSamplesPerDepth;
    batchingJson["frontierRayHitsPerDepth"] = frontierRayHitsPerDepth;
    batchingJson["frontierRayMissesPerDepth"] = frontierRayMissesPerDepth;
    batchingJson["frontierPacketChunksPerDepth"] = frontierPacketChunksPerDepth;
    batchingJson["frontierPacketRaysPerDepth"] = frontierPacketRaysPerDepth;
    batchingJson["frontierRay4PacketChunksPerDepth"] = frontierRay4PacketChunksPerDepth;
    batchingJson["frontierRay8PacketChunksPerDepth"] = frontierRay8PacketChunksPerDepth;
    batchingJson["frontierScalarRaysPerDepth"] = frontierScalarRaysPerDepth;
    batchingJson["frontierPacketScalarFallbackRaysPerDepth"] =
      frontierPacketScalarFallbackRaysPerDepth;
    batchingJson["frontierPacketScalarFallbackRaysByReason"] =
      frontierPacketScalarFallbackRaysByReason;
    batchingJson["frontierPacketRefinedRaysPerDepth"] = frontierPacketRefinedRaysPerDepth;
    batchingJson["frontierPacketRefinedRaysByMaterial"] = frontierPacketRefinedRaysByMaterial;
    batchingJson["radianceDeltaL2PerDepth"] = radianceDeltaL2PerDepth;
    batchingJson["radianceDeltaRmsPerDepth"] = radianceDeltaRmsPerDepth;
    batchingJson["maxRadianceDeltaPerDepth"] = maxRadianceDeltaPerDepth;

    QJsonObject timingsJson;
    timingsJson["sampleGenerationWorkerSeconds"] = timings.sampleGenerationWorkerSeconds;
    timingsJson["sampleStreamWorkerSeconds"] = timings.sampleStreamWorkerSeconds;
    timingsJson["primaryRayWorkerSeconds"] = timings.primaryRayWorkerSeconds;
    timingsJson["sampleEnqueueWorkerSeconds"] = timings.sampleEnqueueWorkerSeconds;
    timingsJson["sampleGenerationOverheadWorkerSeconds"] =
      std::max(0.0, timings.sampleGenerationWorkerSeconds - timings.sampleStreamWorkerSeconds -
                      timings.primaryRayWorkerSeconds - timings.sampleEnqueueWorkerSeconds);
    timingsJson["integratorBatchWorkerSeconds"] = timings.integratorBatchWorkerSeconds;
    timingsJson["integratorIntersectionWorkerSeconds"] =
      timings.integratorIntersectionWorkerSeconds;
    timingsJson["integratorShadingWorkerSeconds"] = timings.integratorShadingWorkerSeconds;
    timingsJson["integratorOverheadWorkerSeconds"] = timings.integratorOverheadWorkerSeconds;
    timingsJson["integratorPathSetupWorkerSeconds"] = timings.integratorPathSetupWorkerSeconds;
    timingsJson["integratorFrontierBookkeepingWorkerSeconds"] =
      timings.integratorFrontierBookkeepingWorkerSeconds;
    timingsJson["integratorProgressSnapshotWorkerSeconds"] =
      timings.integratorProgressSnapshotWorkerSeconds;
    timingsJson["integratorConvergenceTestWorkerSeconds"] =
      timings.integratorConvergenceTestWorkerSeconds;
    timingsJson["integratorResidualWorkerSeconds"] = timings.integratorResidualWorkerSeconds;
    timingsJson["totalRenderSeconds"] = timings.totalRenderSeconds;

    QJsonObject denoiseJson;
    denoiseJson["enabled"] = denoise.enabled;
    denoiseJson["seconds"] = denoise.seconds;
    denoiseJson["featureSeconds"] = denoise.featureSeconds;
    if (denoise.enabled) {
      denoiseJson["denoiser"] = QString::fromStdString(denoise.denoiser);
    }
    QJsonObject denoiseParametersJson;
    for (const auto& parameter : denoise.numericParameters) {
      denoiseParametersJson[QString::fromStdString(parameter.name)] = parameter.value;
    }
    if (!denoiseParametersJson.isEmpty()) {
      denoiseJson["parameters"] = denoiseParametersJson;
    }
    if (denoise.enabled) {
      QJsonObject featureJson;
      featureJson["albedo"] = denoise.albedoFeature;
      featureJson["normal"] = denoise.normalFeature;
      featureJson["depth"] = denoise.depthFeature;
      denoiseJson["features"] = featureJson;

      QJsonObject featurePrepassJson;
      featurePrepassJson["tileCount"] = static_cast<double>(denoise.featureTileCount);
      featurePrepassJson["completedTileCount"] =
        static_cast<double>(denoise.completedFeatureTileCount);
      featurePrepassJson["pixels"] = static_cast<double>(denoise.featurePixels);
      featurePrepassJson["seconds"] = denoise.featureSeconds;
      denoiseJson["featurePrepass"] = featurePrepassJson;
    }

    QJsonObject convergenceJson;
    QJsonArray stoppedTileDepthHistogram;
    for (const std::uint64_t count : convergence.stoppedTileDepthHistogram) {
      stoppedTileDepthHistogram.push_back(static_cast<double>(count));
    }
    convergenceJson["enabled"] = convergence.enabled;
    convergenceJson["activeSampleFractionThreshold"] = convergence.activeSampleFractionThreshold;
    convergenceJson["radianceDeltaRmsThreshold"] = convergence.radianceDeltaRmsThreshold;
    convergenceJson["stoppedTileCount"] = static_cast<double>(convergence.stoppedTileCount);
    convergenceJson["earliestStoppedAfterDepth"] =
      static_cast<double>(convergence.earliestStoppedAfterDepth);
    convergenceJson["latestStoppedAfterDepth"] =
      static_cast<double>(convergence.latestStoppedAfterDepth);
    convergenceJson["feedbackDepthCount"] = static_cast<double>(convergence.feedbackDepthCount);
    convergenceJson["stoppedTileDepthHistogram"] = stoppedTileDepthHistogram;
    convergenceJson["decision"] = QString::fromStdString(convergence.decision);

    QJsonObject object;
    object["input"] = inputJson;
    object["tiling"] = tilingJson;
    object["scheduling"] = schedulingJson;
    object["batching"] = batchingJson;
    object["convergence"] = convergenceJson;
    object["denoise"] = denoiseJson;
    object["timings"] = timingsJson;
    return object;
  }

  void
  WavefrontRenderMetrics::ConvergenceSummary::recordStoppedTileAfterDepth(std::uint64_t depth) {
    ++stoppedTileCount;
    if (earliestStoppedAfterDepth == 0 || depth < earliestStoppedAfterDepth) {
      earliestStoppedAfterDepth = depth;
    }
    latestStoppedAfterDepth = std::max(latestStoppedAfterDepth, depth);
    if (depth > 0) {
      if (stoppedTileDepthHistogram.size() < depth) {
        stoppedTileDepthHistogram.resize(depth);
      }
      ++stoppedTileDepthHistogram[depth - 1];
    }
  }

  struct WavefrontRaytracer::Private {
    Private()
        : threadPool(std::make_unique<QThreadPool>()),
          queueSize(QThread::idealThreadCount()),
          integrator(std::make_unique<render::WhittedIntegrator>()),
          showProgressIndicators(false),
          convergenceActiveSampleFractionThreshold(
            RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD),
          convergenceRadianceDeltaRmsThreshold(RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD) {
    }

    std::unique_ptr<QThreadPool> threadPool;
    std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
    std::list<std::shared_ptr<engine::TileRenderTask>> denoiserFeatureTasks;
    int queueSize;
    std::unique_ptr<render::Integrator> integrator;
    std::unique_ptr<render::Denoiser> denoiser;
    bool showProgressIndicators;
    bool metricsEnabled{false};
    bool convergenceEnabled{false};
    double convergenceActiveSampleFractionThreshold;
    double convergenceRadianceDeltaRmsThreshold;
    std::optional<int> maximumRecursionDepth;
    std::optional<std::uint64_t> samplingSeed;
    detail::WavefrontMetricsRecorder metrics;

    detail::WavefrontTileRenderConfig tileRenderConfig() const {
      return detail::WavefrontTileRenderConfig{*integrator,
                                               denoiser.get(),
                                               showProgressIndicators,
                                               convergenceEnabled,
                                               convergenceActiveSampleFractionThreshold,
                                               convergenceRadianceDeltaRmsThreshold,
                                               metricsEnabled,
                                               samplingSeed};
    }

    detail::WavefrontTileRenderer tileRenderer() {
      return detail::WavefrontTileRenderer(tileRenderConfig(), metrics);
    }

    void configureIntegratorCancellation(const WavefrontRaytracer& owner) {
      integrator->setCancellationCallback(
        [&owner] { return owner.camera() && owner.camera()->isCancelled(); });
    }
  };

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::WavefrontRaytracer(std::shared_ptr<render::Camera> camera,
                                         std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(camera), std::move(scene)),
        p(std::make_unique<Private>()) {
    p->configureIntegratorCancellation(*this);
  }

  WavefrontRaytracer::~WavefrontRaytracer() = default;

  std::shared_ptr<render::RenderEngine> WavefrontRaytracer::cloneForRender() const {
    auto result =
      std::make_shared<WavefrontRaytracer>(m_camera ? m_camera->clone() : nullptr, m_scene);
    copyRenderEngineStateTo(*result);
    result->setIntegrator(p->integrator->clone());
    if (p->maximumRecursionDepth) {
      result->setMaximumRecursionDepth(*p->maximumRecursionDepth);
    }
    if (p->denoiser) {
      result->setDenoiser(p->denoiser->clone());
    }
    result->setMaximumThreads(p->threadPool->maxThreadCount());
    result->setQueueSize(p->queueSize);
    result->setShowProgressIndicators(p->showProgressIndicators);
    result->setMetricsEnabled(p->metricsEnabled);
    result->setConvergenceEnabled(p->convergenceEnabled);
    result->setConvergenceActiveSampleFractionThreshold(
      p->convergenceActiveSampleFractionThreshold);
    result->setConvergenceRadianceDeltaRmsThreshold(p->convergenceRadianceDeltaRmsThreshold);
    if (p->samplingSeed) {
      result->setSamplingSeed(*p->samplingSeed);
    }
    return result;
  }

  void WavefrontRaytracer::render(Buffer<Colord>& buffer) {
    if (!m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    if (p->metricsEnabled) {
      p->metrics.reset(*m_camera, buffer.width(), buffer.height(), tilePlan, p->queueSize,
                       *p->integrator, p->denoiser.get(), p->convergenceEnabled,
                       p->convergenceActiveSampleFractionThreshold,
                       p->convergenceRadianceDeltaRmsThreshold);
    }
    auto tileRenderer = p->tileRenderer();
    const auto denoiserFeatures = tileRenderer.buildDenoiserFeatures(
      *m_camera, *m_scene, buffer.rect(), tilePlan, *p->threadPool, p->denoiserFeatureTasks);
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, samplingSeed, tileRenderer,
       denoiserFeaturePtr, publishProgressSnapshots](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderHdrTile(*camera, *rayCaster, *m_scene, *bufferPtr, rect, tileSeed,
                                   publishProgressSnapshots, denoiserFeaturePtr);
    });
    tileRenderer.denoise(buffer, denoiserFeatures.get());
    if (p->metricsEnabled) {
      p->metrics.finish(renderStart);
    }

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<unsigned int>& buffer) {
    if (!m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    if (p->denoiser) {
      Buffer<Colord> hdrBuffer(buffer.width(), buffer.height());
      render(hdrBuffer, buffer, tonemap());
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    auto tonemapOp = tonemap();
    Buffer<unsigned int>* bufferPtr = &buffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(buffer.width(), buffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    if (p->metricsEnabled) {
      p->metrics.reset(*m_camera, buffer.width(), buffer.height(), tilePlan, p->queueSize,
                       *p->integrator, p->denoiser.get(), p->convergenceEnabled,
                       p->convergenceActiveSampleFractionThreshold,
                       p->convergenceRadianceDeltaRmsThreshold);
    }
    auto tileRenderer = p->tileRenderer();
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, tonemapOp, samplingSeed,
       tileRenderer, publishProgressSnapshots](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderDisplayTile(*camera, *rayCaster, *m_scene, *bufferPtr, tonemapOp, rect,
                                       tileSeed, publishProgressSnapshots);
    });
    if (p->metricsEnabled) {
      p->metrics.finish(renderStart);
    }

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                                  std::shared_ptr<render::Tonemap> displayTonemap) {
    if (!core::util::bufferDimensionsEqual(hdrBuffer, displayBuffer)) {
      throw std::runtime_error("wavefront dual-output render requires matching buffer dimensions");
    }

    if (!m_scene || !m_camera) {
      hdrBuffer.clear();
      displayBuffer.clear();
      return;
    }

    p->tasks.clear();
    p->denoiserFeatureTasks.clear();

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().reset();
#endif

    m_camera->viewPlane()->setup(m_camera->matrix(), hdrBuffer.rect());
    m_camera->setShowProgressIndicators(p->showProgressIndicators);

    auto rayCaster = std::make_shared<RecursiveRayCasterAdapter>(*m_scene, *p->integrator);
    auto camera = m_camera;
    Buffer<Colord>* hdrBufferPtr = &hdrBuffer;
    Buffer<unsigned int>* displayBufferPtr = &displayBuffer;

    const render::TilePlan tilePlan =
      render::TilePlan::forBuffer(hdrBuffer.width(), hdrBuffer.height(), p->queueSize);
    const auto renderStart = detail::WavefrontMetricsRecorder::Clock::now();
    if (p->metricsEnabled) {
      p->metrics.reset(*m_camera, hdrBuffer.width(), hdrBuffer.height(), tilePlan, p->queueSize,
                       *p->integrator, p->denoiser.get(), p->convergenceEnabled,
                       p->convergenceActiveSampleFractionThreshold,
                       p->convergenceRadianceDeltaRmsThreshold);
    }
    auto tileRenderer = p->tileRenderer();
    const auto denoiserFeatures = tileRenderer.buildDenoiserFeatures(
      *m_camera, *m_scene, hdrBuffer.rect(), tilePlan, *p->threadPool,
      p->denoiserFeatureTasks);
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    const auto samplingSeed = p->samplingSeed;
    const bool publishProgressSnapshots = progressiveDisplayEnabled();
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, hdrBufferPtr, displayBufferPtr, displayTonemap, samplingSeed,
       tileRenderer, denoiserFeaturePtr, publishProgressSnapshots](const Recti& rect,
                                                                   std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        tileRenderer.renderDualOutputTile(*camera, *rayCaster, *m_scene, *hdrBufferPtr,
                                          *displayBufferPtr, displayTonemap, rect, tileSeed,
                                          publishProgressSnapshots, denoiserFeaturePtr);
    });
    tileRenderer.denoise(hdrBuffer, denoiserFeatures.get());
    tileRenderer.writeDisplayBuffer(displayBuffer, hdrBuffer, displayTonemap);
    if (p->metricsEnabled) {
      p->metrics.finish(renderStart);
    }

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::cancel() {
    if (m_camera) {
      m_camera->cancel();
    }
  }

  void WavefrontRaytracer::uncancel() {
    if (m_camera) {
      m_camera->uncancel();
    }
  }

  std::list<Recti> WavefrontRaytracer::activeTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->denoiserFeatureTasks) {
      if (task->active.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    for (const auto& task : p->tasks) {
      if (task->active.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  std::list<Recti> WavefrontRaytracer::completedTiles() const {
    std::list<Recti> result;
    for (const auto& task : p->tasks) {
      if (task->completed.load(std::memory_order_acquire)) {
        result.push_back(task->rect);
      }
    }
    return result;
  }

  void WavefrontRaytracer::setIntegrator(std::unique_ptr<render::Integrator> integrator) {
    if (!integrator) {
      throw std::invalid_argument("WavefrontRaytracer integrator cannot be null");
    }
    p->integrator = std::move(integrator);
    if (p->maximumRecursionDepth) {
      p->integrator->setMaximumRecursionDepth(*p->maximumRecursionDepth);
    }
    p->configureIntegratorCancellation(*this);
  }

  const render::Integrator& WavefrontRaytracer::integrator() const {
    return *p->integrator;
  }

  void WavefrontRaytracer::setDenoiser(std::unique_ptr<render::Denoiser> denoiser) {
    p->denoiser = std::move(denoiser);
  }

  void WavefrontRaytracer::clearDenoiser() {
    p->denoiser.reset();
  }

  const render::Denoiser* WavefrontRaytracer::denoiser() const {
    return p->denoiser.get();
  }

  void WavefrontRaytracer::setMetricsEnabled(bool enabled) {
    p->metricsEnabled = enabled;
  }

  bool WavefrontRaytracer::metricsEnabled() const {
    return p->metricsEnabled;
  }

  void WavefrontRaytracer::setMaximumRecursionDepth(int depth) {
    p->maximumRecursionDepth = depth;
    p->integrator->setMaximumRecursionDepth(depth);
  }

  void WavefrontRaytracer::setSamplingSeed(std::uint64_t seed) {
    p->samplingSeed = seed;
  }

  void WavefrontRaytracer::clearSamplingSeed() {
    p->samplingSeed.reset();
  }

  std::optional<std::uint64_t> WavefrontRaytracer::samplingSeed() const {
    return p->samplingSeed;
  }

  void WavefrontRaytracer::setMaximumThreads(int threads) {
    p->threadPool->setMaxThreadCount(threads);
  }

  void WavefrontRaytracer::setQueueSize(int queue) {
    p->queueSize = queue;
  }

  void WavefrontRaytracer::setShowProgressIndicators(bool show) {
    p->showProgressIndicators = show;
  }

  void WavefrontRaytracer::setConvergenceEnabled(bool enabled) {
    p->convergenceEnabled = enabled;
  }

  bool WavefrontRaytracer::convergenceEnabled() const {
    return p->convergenceEnabled;
  }

  void WavefrontRaytracer::setConvergenceActiveSampleFractionThreshold(double fraction) {
    p->convergenceActiveSampleFractionThreshold = std::clamp(fraction, 0.0, 1.0);
  }

  double WavefrontRaytracer::convergenceActiveSampleFractionThreshold() const {
    return p->convergenceActiveSampleFractionThreshold;
  }

  void WavefrontRaytracer::setConvergenceRadianceDeltaRmsThreshold(double threshold) {
    p->convergenceRadianceDeltaRmsThreshold = std::max(0.0, threshold);
  }

  double WavefrontRaytracer::convergenceRadianceDeltaRmsThreshold() const {
    return p->convergenceRadianceDeltaRmsThreshold;
  }

  WavefrontRenderMetrics WavefrontRaytracer::lastMetrics() const {
    return p->metrics.snapshot();
  }
}
