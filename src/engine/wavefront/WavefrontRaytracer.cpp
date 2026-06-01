#include "engine/wavefront/WavefrontRaytracer.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "core/util/BufferUtils.h"
#include "engine/TileRenderTask.h"
#include "render/Integrator.h"
#include "render/RayCaster.h"
#include "render/SamplingSeed.h"
#include "render/State.h"
#include "render/Stats.h"
#include "render/TilePlan.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace engine::wavefront {
  namespace {
    using WavefrontClock = std::chrono::steady_clock;

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

  QJsonObject wavefrontRenderMetricsToJson(const WavefrontRenderMetrics& metrics) {
    QJsonObject input;
    input["width"] = metrics.input.width;
    input["height"] = metrics.input.height;
    input["samplesPerPixel"] = metrics.input.samplesPerPixel;
    input["renderedPixels"] = static_cast<double>(metrics.input.renderedPixels);
    input["primarySamples"] = static_cast<double>(metrics.input.primarySamples);

    QJsonObject tiling;
    tiling["tileCount"] = static_cast<double>(metrics.tiling.tileCount);
    tiling["nonEmptyTileCount"] = static_cast<double>(metrics.tiling.nonEmptyTileCount);

    QJsonObject scheduling;
    scheduling["configuredQueueSize"] = static_cast<double>(metrics.scheduling.configuredQueueSize);
    scheduling["resolvedQueueSize"] = static_cast<double>(metrics.scheduling.resolvedQueueSize);
    scheduling["decision"] = QString::fromStdString(metrics.scheduling.decision);

    QJsonObject batching;
    QJsonArray activeSamplesPerDepth;
    for (const std::uint64_t count : metrics.batching.activeSamplesPerDepth) {
      activeSamplesPerDepth.push_back(static_cast<double>(count));
    }
    QJsonArray radianceDeltaL2PerDepth;
    QJsonArray radianceDeltaRmsPerDepth;
    for (std::size_t depth = 0; depth != metrics.batching.radianceDeltaSquaredSumPerDepth.size();
         ++depth) {
      const double squaredSum = metrics.batching.radianceDeltaSquaredSumPerDepth[depth];
      radianceDeltaL2PerDepth.push_back(std::sqrt(squaredSum));
      const std::uint64_t activeSamples = depth < metrics.batching.activeSamplesPerDepth.size()
                                            ? metrics.batching.activeSamplesPerDepth[depth]
                                            : 0;
      radianceDeltaRmsPerDepth.push_back(
        activeSamples == 0 ? 0.0 : std::sqrt(squaredSum / static_cast<double>(activeSamples)));
    }
    QJsonArray maxRadianceDeltaPerDepth;
    for (const double delta : metrics.batching.maxRadianceDeltaPerDepth) {
      maxRadianceDeltaPerDepth.push_back(delta);
    }
    batching["integrator"] = QString::fromStdString(metrics.batching.integrator);
    batching["executionMode"] = QString::fromStdString(metrics.batching.executionMode);
    batching["batches"] = static_cast<double>(metrics.batching.batches);
    batching["samplesSubmitted"] = static_cast<double>(metrics.batching.samplesSubmitted);
    batching["maxBatchSize"] = static_cast<double>(metrics.batching.maxBatchSize);
    batching["averageBatchSize"] = metrics.batching.averageBatchSize;
    batching["activeSamplesPerDepth"] = activeSamplesPerDepth;
    batching["radianceDeltaL2PerDepth"] = radianceDeltaL2PerDepth;
    batching["radianceDeltaRmsPerDepth"] = radianceDeltaRmsPerDepth;
    batching["maxRadianceDeltaPerDepth"] = maxRadianceDeltaPerDepth;

    QJsonObject timings;
    timings["totalRenderSeconds"] = metrics.timings.totalRenderSeconds;

    QJsonObject convergence;
    convergence["enabled"] = metrics.convergence.enabled;
    convergence["activeSampleFractionThreshold"] =
      metrics.convergence.activeSampleFractionThreshold;
    convergence["radianceDeltaRmsThreshold"] = metrics.convergence.radianceDeltaRmsThreshold;
    convergence["decision"] = QString::fromStdString(metrics.convergence.decision);

    QJsonObject object;
    object["input"] = input;
    object["tiling"] = tiling;
    object["scheduling"] = scheduling;
    object["batching"] = batching;
    object["convergence"] = convergence;
    object["timings"] = timings;
    return object;
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
    int queueSize;
    std::unique_ptr<render::Integrator> integrator;
    bool showProgressIndicators;
    bool convergenceEnabled{false};
    double convergenceActiveSampleFractionThreshold;
    double convergenceRadianceDeltaRmsThreshold;
    std::optional<std::uint64_t> samplingSeed;
    mutable WavefrontRenderMetrics lastMetrics;
    mutable std::mutex metricsMutex;

    struct TilePixel {
      Recti footprint;
      Colord color{Colord::black()};
    };

    struct TileTraceResult {
      std::vector<TilePixel> pixels;
      std::size_t sampleCount{0};
      render::IntegratorBatchMetrics batchMetrics;
    };

    TileTraceResult traceTile(render::Camera& camera, const render::RayCaster& rayCaster,
                              const render::Scene& scene, const Recti& actualRect,
                              std::optional<std::uint64_t> tileSeed,
                              const std::function<void(const Recti&)>& markProgress) const {
      TileTraceResult result;
      std::vector<render::IntegratorRaySample> samples;
      std::vector<std::size_t> samplePixelIndices;
      const int sampleCount = camera.samplesPerPixel();

      auto plane = camera.viewPlane();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled())
          break;

        const std::size_t pixelIndex = result.pixels.size();
        const Recti footprint = pixel.footprintWithin(actualRect);
        if (showProgressIndicators) {
          markProgress(footprint);
        }
        result.pixels.push_back(TilePixel{footprint, Colord::black()});

        for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
          if (camera.isCancelled())
            break;

          if (auto sample = camera.primaryRaySample(pixel, sampleIndex, tileSeed)) {
            samples.push_back(
              render::IntegratorRaySample{sample->ray, sample->timeSample, sample->sampleStream});
            samplePixelIndices.push_back(pixelIndex);
          }
        }
      }

      const std::vector<Colord> sampleColors =
        integrator->radianceBatch(scene, samples, rayCaster, &result.batchMetrics);
      const double sampleScale = 1.0 / sampleCount;
      for (std::size_t index = 0; index != sampleColors.size(); ++index) {
        result.pixels[samplePixelIndices[index]].color += sampleColors[index] * sampleScale;
      }
      result.sampleCount = samples.size();
      return result;
    }

    void writeColor(Buffer<Colord>& buffer, const Recti& footprint, const Colord& color) const {
      for (int y = footprint.top(); y != footprint.bottom(); ++y)
        for (int x = footprint.left(); x != footprint.right(); ++x)
          buffer[y][x] = color;
    }

    void writeRGB(Buffer<unsigned int>& buffer, const Recti& footprint, unsigned int rgb) const {
      for (int y = footprint.top(); y != footprint.bottom(); ++y)
        for (int x = footprint.left(); x != footprint.right(); ++x)
          buffer[y][x] = rgb;
    }

    void resetMetrics(render::Camera& camera, int width, int height,
                      const render::TilePlan& tilePlan) {
      std::lock_guard<std::mutex> lock(metricsMutex);
      lastMetrics = WavefrontRenderMetrics();
      lastMetrics.input.width = width;
      lastMetrics.input.height = height;
      lastMetrics.input.samplesPerPixel = camera.samplesPerPixel();
      lastMetrics.tiling.tileCount = tilePlan.size();
      lastMetrics.scheduling.configuredQueueSize =
        static_cast<std::uint64_t>(std::max(0, queueSize));
      lastMetrics.scheduling.resolvedQueueSize = tilePlan.size();
      lastMetrics.scheduling.decision = tilePlan.isSingleTile() ? "single_tile" : "tiled";
      lastMetrics.batching.integrator = integrator->diagnosticName();
      lastMetrics.batching.executionMode = integrator->batchExecutionMode();
      lastMetrics.convergence.enabled = convergenceEnabled;
      lastMetrics.convergence.activeSampleFractionThreshold =
        convergenceActiveSampleFractionThreshold;
      lastMetrics.convergence.radianceDeltaRmsThreshold = convergenceRadianceDeltaRmsThreshold;
      lastMetrics.convergence.decision = convergenceEnabled ? "configured" : "disabled";
    }

    void recordTileMetrics(const TileTraceResult& result) const {
      std::lock_guard<std::mutex> lock(metricsMutex);
      lastMetrics.input.renderedPixels += result.pixels.size();
      lastMetrics.input.primarySamples += result.sampleCount;
      if (result.sampleCount > 0) {
        ++lastMetrics.tiling.nonEmptyTileCount;
        ++lastMetrics.batching.batches;
        lastMetrics.batching.samplesSubmitted += result.sampleCount;
        lastMetrics.batching.maxBatchSize = std::max(
          lastMetrics.batching.maxBatchSize, static_cast<std::uint64_t>(result.sampleCount));
        if (lastMetrics.batching.activeSamplesPerDepth.size() <
            result.batchMetrics.activeSamplesPerDepth.size()) {
          lastMetrics.batching.activeSamplesPerDepth.resize(
            result.batchMetrics.activeSamplesPerDepth.size());
        }
        for (std::size_t depth = 0; depth != result.batchMetrics.activeSamplesPerDepth.size();
             ++depth) {
          lastMetrics.batching.activeSamplesPerDepth[depth] +=
            result.batchMetrics.activeSamplesPerDepth[depth];
        }
        if (lastMetrics.batching.radianceDeltaSquaredSumPerDepth.size() <
            result.batchMetrics.radianceDeltaSquaredSumPerDepth.size()) {
          lastMetrics.batching.radianceDeltaSquaredSumPerDepth.resize(
            result.batchMetrics.radianceDeltaSquaredSumPerDepth.size());
        }
        for (std::size_t depth = 0;
             depth != result.batchMetrics.radianceDeltaSquaredSumPerDepth.size(); ++depth) {
          lastMetrics.batching.radianceDeltaSquaredSumPerDepth[depth] +=
            result.batchMetrics.radianceDeltaSquaredSumPerDepth[depth];
        }
        if (lastMetrics.batching.maxRadianceDeltaPerDepth.size() <
            result.batchMetrics.maxRadianceDeltaPerDepth.size()) {
          lastMetrics.batching.maxRadianceDeltaPerDepth.resize(
            result.batchMetrics.maxRadianceDeltaPerDepth.size());
        }
        for (std::size_t depth = 0; depth != result.batchMetrics.maxRadianceDeltaPerDepth.size();
             ++depth) {
          lastMetrics.batching.maxRadianceDeltaPerDepth[depth] =
            std::max(lastMetrics.batching.maxRadianceDeltaPerDepth[depth],
                     result.batchMetrics.maxRadianceDeltaPerDepth[depth]);
        }
      }
    }

    void finishMetrics(WavefrontClock::time_point start) {
      std::lock_guard<std::mutex> lock(metricsMutex);
      lastMetrics.timings.totalRenderSeconds =
        std::chrono::duration<double>(WavefrontClock::now() - start).count();
      lastMetrics.batching.averageBatchSize =
        lastMetrics.batching.batches == 0
          ? 0.0
          : static_cast<double>(lastMetrics.batching.samplesSubmitted) /
              static_cast<double>(lastMetrics.batching.batches);
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    const render::Scene& scene, Buffer<Colord>& buffer, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      const auto result =
        traceTile(camera, rayCaster, scene, actualRect, tileSeed,
                  [&](const Recti& footprint) { writeColor(buffer, footprint, Colord(1, 0, 0)); });
      recordTileMetrics(result);
      for (const auto& pixel : result.pixels) {
        writeColor(buffer, pixel.footprint, pixel.color);
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    const render::Scene& scene, Buffer<unsigned int>& buffer,
                    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      const auto result =
        traceTile(camera, rayCaster, scene, actualRect, tileSeed,
                  [&](const Recti& footprint) { writeRGB(buffer, footprint, 0xffff0000); });
      recordTileMetrics(result);
      for (const auto& pixel : result.pixels) {
        writeRGB(buffer, pixel.footprint,
                 (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    const render::Scene& scene, Buffer<Colord>& hdrBuffer,
                    Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect, std::optional<std::uint64_t> tileSeed) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      const auto result =
        traceTile(camera, rayCaster, scene, actualRect, tileSeed, [&](const Recti& footprint) {
          writeColor(hdrBuffer, footprint, Colord(1, 0, 0));
          writeRGB(displayBuffer, footprint, 0xffff0000);
        });
      recordTileMetrics(result);
      for (const auto& pixel : result.pixels) {
        writeColor(hdrBuffer, pixel.footprint, pixel.color);
        writeRGB(displayBuffer, pixel.footprint,
                 (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
      }
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
    result->setTonemap(tonemap());
    result->setIntegrator(p->integrator->clone());
    result->setMaximumThreads(p->threadPool->maxThreadCount());
    result->setQueueSize(p->queueSize);
    result->setShowProgressIndicators(p->showProgressIndicators);
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
    const auto renderStart = WavefrontClock::now();
    p->resetMetrics(*m_camera, buffer.width(), buffer.height(), tilePlan);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, samplingSeed](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *m_scene, *bufferPtr, rect, tileSeed);
      });
    p->finishMetrics(renderStart);

#ifdef RAYTRACER_ENABLE_STATS
    ::render::stats::Counters::instance().dumpJson(std::cerr);
#endif
  }

  void WavefrontRaytracer::render(Buffer<unsigned int>& buffer) {
    if (!m_scene || !m_camera) {
      buffer.clear();
      return;
    }

    p->tasks.clear();

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
    const auto renderStart = WavefrontClock::now();
    p->resetMetrics(*m_camera, buffer.width(), buffer.height(), tilePlan);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, bufferPtr, tonemapOp, samplingSeed](const Recti& rect,
                                                                    std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *m_scene, *bufferPtr, tonemapOp, rect, tileSeed);
      });
    p->finishMetrics(renderStart);

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
    const auto renderStart = WavefrontClock::now();
    p->resetMetrics(*m_camera, hdrBuffer.width(), hdrBuffer.height(), tilePlan);
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, hdrBufferPtr, displayBufferPtr, displayTonemap,
       samplingSeed](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *m_scene, *hdrBufferPtr, *displayBufferPtr,
                      displayTonemap, rect, tileSeed);
      });
    p->finishMetrics(renderStart);

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
    p->configureIntegratorCancellation(*this);
  }

  const render::Integrator& WavefrontRaytracer::integrator() const {
    return *p->integrator;
  }

  void WavefrontRaytracer::setMaximumRecursionDepth(int depth) {
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
    std::lock_guard<std::mutex> lock(p->metricsMutex);
    return p->lastMetrics;
  }
}
