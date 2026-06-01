#include "engine/wavefront/WavefrontRaytracer.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"
#include "core/math/HitPointInterval.h"
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
#include "render/denoise/Denoiser.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
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

  QJsonObject WavefrontRenderMetrics::toJson() const {
    QJsonObject inputJson;
    inputJson["width"] = input.width;
    inputJson["height"] = input.height;
    inputJson["samplesPerPixel"] = input.samplesPerPixel;
    inputJson["renderedPixels"] = static_cast<double>(input.renderedPixels);
    inputJson["primarySamples"] = static_cast<double>(input.primarySamples);

    QJsonObject tilingJson;
    tilingJson["tileCount"] = static_cast<double>(tiling.tileCount);
    tilingJson["nonEmptyTileCount"] = static_cast<double>(tiling.nonEmptyTileCount);

    QJsonObject schedulingJson;
    schedulingJson["configuredQueueSize"] = static_cast<double>(scheduling.configuredQueueSize);
    schedulingJson["resolvedQueueSize"] = static_cast<double>(scheduling.resolvedQueueSize);
    schedulingJson["decision"] = QString::fromStdString(scheduling.decision);

    QJsonObject batchingJson;
    QJsonArray activeSamplesPerDepth;
    for (const std::uint64_t count : batching.activeSamplesPerDepth) {
      activeSamplesPerDepth.push_back(static_cast<double>(count));
    }
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
    batchingJson["compatibilityShadeSamples"] =
      static_cast<double>(batching.compatibilityShadeSamples);
    batchingJson["activeSamplesPerDepth"] = activeSamplesPerDepth;
    batchingJson["radianceDeltaL2PerDepth"] = radianceDeltaL2PerDepth;
    batchingJson["radianceDeltaRmsPerDepth"] = radianceDeltaRmsPerDepth;
    batchingJson["maxRadianceDeltaPerDepth"] = maxRadianceDeltaPerDepth;

    QJsonObject timingsJson;
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
    }

    QJsonObject convergenceJson;
    convergenceJson["enabled"] = convergence.enabled;
    convergenceJson["activeSampleFractionThreshold"] = convergence.activeSampleFractionThreshold;
    convergenceJson["radianceDeltaRmsThreshold"] = convergence.radianceDeltaRmsThreshold;
    convergenceJson["stoppedTileCount"] = static_cast<double>(convergence.stoppedTileCount);
    convergenceJson["earliestStoppedAfterDepth"] =
      static_cast<double>(convergence.earliestStoppedAfterDepth);
    convergenceJson["latestStoppedAfterDepth"] =
      static_cast<double>(convergence.latestStoppedAfterDepth);
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
    std::unique_ptr<render::Denoiser> denoiser;
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

    using TileProgressPublisher = std::function<void(const std::vector<TilePixel>&)>;

    struct DenoiserFeatureSet {
      explicit DenoiserFeatureSet(int width, int height)
          : albedo(width, height),
            normal(width, height),
            depth(width, height) {
        albedo.clear(Colord::black());
        normal.clear(Vector3d::null);
        depth.clear(0.0);
      }

      Buffer<Colord> albedo;
      Buffer<Vector3d> normal;
      Buffer<double> depth;
    };

    class TileProgressObserver final : public render::IntegratorBatchObserver {
    public:
      TileProgressObserver(std::vector<TilePixel>& pixels,
                           const std::vector<std::size_t>& samplePixelIndices, double sampleScale,
                           TileProgressPublisher publisher)
          : m_pixels(pixels),
            m_samplePixelIndices(samplePixelIndices),
            m_sampleScale(sampleScale),
            m_publisher(std::move(publisher)) {
      }

      void depthCompleted(std::uint64_t completedDepth, const std::vector<Colord>& sampleColors,
                          std::uint64_t activeSamples) override {
        (void)completedDepth;
        (void)activeSamples;
        if (!m_publisher) {
          return;
        }

        applySampleColors(sampleColors);
        m_publisher(m_pixels);
      }

      void applySampleColors(const std::vector<Colord>& sampleColors) const {
        for (auto& pixel : m_pixels) {
          pixel.color = Colord::black();
        }

        const std::size_t count = std::min(sampleColors.size(), m_samplePixelIndices.size());
        for (std::size_t index = 0; index != count; ++index) {
          const std::size_t pixelIndex = m_samplePixelIndices[index];
          if (pixelIndex < m_pixels.size()) {
            m_pixels[pixelIndex].color += sampleColors[index] * m_sampleScale;
          }
        }
      }

    private:
      std::vector<TilePixel>& m_pixels;
      const std::vector<std::size_t>& m_samplePixelIndices;
      double m_sampleScale;
      TileProgressPublisher m_publisher;
    };

    render::IntegratorBatchSettings batchSettings() const {
      render::IntegratorBatchSettings settings;
      settings.convergenceEnabled = convergenceEnabled;
      settings.activeSampleFractionThreshold = convergenceActiveSampleFractionThreshold;
      settings.radianceDeltaRmsThreshold = convergenceRadianceDeltaRmsThreshold;
      return settings;
    }

    TileTraceResult traceTile(render::Camera& camera, const render::RayCaster& rayCaster,
                              const render::Scene& scene, const Recti& actualRect,
                              std::optional<std::uint64_t> tileSeed,
                              const std::function<void(const Recti&)>& markProgress,
                              TileProgressPublisher publishProgress) const {
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

      const double sampleScale = sampleCount > 0 ? 1.0 / sampleCount : 0.0;
      TileProgressObserver progressObserver(result.pixels, samplePixelIndices, sampleScale,
                                            std::move(publishProgress));
      render::IntegratorBatchSettings settings = batchSettings();
      settings.progressObserver = samples.empty() ? nullptr : &progressObserver;

      const std::vector<Colord> sampleColors =
        integrator->radianceBatch(scene, samples, rayCaster, &result.batchMetrics, settings);
      progressObserver.applySampleColors(sampleColors);
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

    void writeDisplayBuffer(Buffer<unsigned int>& displayBuffer, const Buffer<Colord>& hdrBuffer,
                            std::shared_ptr<render::Tonemap> tonemap) const {
      for (int y = 0; y != hdrBuffer.height(); ++y) {
        for (int x = 0; x != hdrBuffer.width(); ++x) {
          displayBuffer[y][x] = (tonemap ? tonemap->apply(hdrBuffer[y][x]) : hdrBuffer[y][x]).rgb();
        }
      }
    }

    void denoise(Buffer<Colord>& buffer, const DenoiserFeatureSet* features = nullptr) const {
      if (!denoiser) {
        return;
      }

      const auto denoiseStart = WavefrontClock::now();
      render::DenoiserFrame frame(buffer);
      if (features) {
        frame.features.albedo = &features->albedo;
        frame.features.normal = &features->normal;
        frame.features.depth = &features->depth;
      }
      denoiser->denoiseFrame(frame);
      const double seconds =
        std::chrono::duration<double>(WavefrontClock::now() - denoiseStart).count();
      std::lock_guard<std::mutex> lock(metricsMutex);
      lastMetrics.denoise.albedoFeature = frame.features.albedo != nullptr;
      lastMetrics.denoise.normalFeature = frame.features.normal != nullptr;
      lastMetrics.denoise.depthFeature = frame.features.depth != nullptr;
      lastMetrics.denoise.seconds += seconds;
    }

    void writeDenoiserFeature(DenoiserFeatureSet& features, const Recti& footprint,
                              const Colord& albedo, const Vector3d& normal, double depth) const {
      for (int y = footprint.top(); y != footprint.bottom(); ++y) {
        for (int x = footprint.left(); x != footprint.right(); ++x) {
          features.albedo[y][x] = albedo;
          features.normal[y][x] = normal;
          features.depth[y][x] = depth;
        }
      }
    }

    void recordDenoiserFeatureSeconds(WavefrontClock::time_point start) const {
      const double seconds = std::chrono::duration<double>(WavefrontClock::now() - start).count();
      std::lock_guard<std::mutex> lock(metricsMutex);
      lastMetrics.denoise.featureSeconds += seconds;
    }

    void copyDenoiserFeatureTile(const DenoiserFeatureSet& source, DenoiserFeatureSet& target,
                                 const Recti& actualRect) const {
      for (int y = actualRect.top(); y != actualRect.bottom(); ++y) {
        for (int x = actualRect.left(); x != actualRect.right(); ++x) {
          const int tileX = x - actualRect.left();
          const int tileY = y - actualRect.top();
          if (x >= 0 && y >= 0 && x < source.albedo.width() && y < source.albedo.height()) {
            target.albedo[tileY][tileX] = source.albedo[y][x];
            target.normal[tileY][tileX] = source.normal[y][x];
            target.depth[tileY][tileX] = source.depth[y][x];
          }
        }
      }
    }

    Colord averageFootprintColor(const Buffer<Colord>& buffer, const Recti& footprint,
                                 const Recti& actualRect) const {
      Colord sum = Colord::black();
      int count = 0;
      for (int y = footprint.top(); y != footprint.bottom(); ++y) {
        for (int x = footprint.left(); x != footprint.right(); ++x) {
          const int tileX = x - actualRect.left();
          const int tileY = y - actualRect.top();
          if (tileX >= 0 && tileY >= 0 && tileX < buffer.width() && tileY < buffer.height()) {
            sum += buffer[tileY][tileX];
            ++count;
          }
        }
      }
      return count > 0 ? sum * (1.0 / static_cast<double>(count)) : Colord::black();
    }

    std::vector<TilePixel> denoisedProgressPixels(const std::vector<TilePixel>& pixels,
                                                  const Recti& actualRect,
                                                  const DenoiserFeatureSet* features,
                                                  const render::Denoiser& progressDenoiser) const {
      if (pixels.empty() || actualRect.width() <= 0 || actualRect.height() <= 0) {
        return pixels;
      }

      Buffer<Colord> beauty(actualRect.width(), actualRect.height());
      beauty.clear(Colord::black());
      for (const auto& pixel : pixels) {
        for (int y = pixel.footprint.top(); y != pixel.footprint.bottom(); ++y) {
          for (int x = pixel.footprint.left(); x != pixel.footprint.right(); ++x) {
            const int tileX = x - actualRect.left();
            const int tileY = y - actualRect.top();
            if (tileX >= 0 && tileY >= 0 && tileX < beauty.width() && tileY < beauty.height()) {
              beauty[tileY][tileX] = pixel.color;
            }
          }
        }
      }

      std::unique_ptr<DenoiserFeatureSet> tileFeatures;
      render::DenoiserFrame frame(beauty);
      if (features) {
        tileFeatures =
          std::make_unique<DenoiserFeatureSet>(actualRect.width(), actualRect.height());
        copyDenoiserFeatureTile(*features, *tileFeatures, actualRect);
        frame.features.albedo = &tileFeatures->albedo;
        frame.features.normal = &tileFeatures->normal;
        frame.features.depth = &tileFeatures->depth;
      }
      progressDenoiser.denoiseFrame(frame);

      std::vector<TilePixel> result = pixels;
      for (auto& pixel : result) {
        pixel.color = averageFootprintColor(beauty, pixel.footprint, actualRect);
      }
      return result;
    }

    std::unique_ptr<DenoiserFeatureSet> buildDenoiserFeatures(render::Camera& camera,
                                                              const render::Scene& scene,
                                                              const Recti& rect) const {
      if (!denoiser) {
        return nullptr;
      }

      const auto featureStart = WavefrontClock::now();
      auto features = std::make_unique<DenoiserFeatureSet>(rect.width(), rect.height());
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0) {
        recordDenoiserFeatureSeconds(featureStart);
        return features;
      }

      auto plane = camera.viewPlane();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled()) {
          break;
        }

        const auto sample = camera.primaryRaySample(pixel, /*sampleIndex=*/0, samplingSeed);
        if (!sample) {
          continue;
        }

        render::State state;
        state.timeSample = sample->timeSample;
        state.sampleStream = sample->sampleStream.get();
        HitPointInterval hitPoints;
        const render::Primitive* primitive = scene.intersect(sample->ray, hitPoints, state);
        if (!primitive) {
          continue;
        }

        const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
        Colord albedo = Colord::black();
        if (const auto material = primitive->material()) {
          albedo = material->denoisingAlbedo(sample->ray, hitPoint);
        }
        writeDenoiserFeature(*features, pixel.footprintWithin(actualRect), albedo,
                             hitPoint.normal().normalizedOrZero(1e-12), hitPoint.distance());
      }
      recordDenoiserFeatureSeconds(featureStart);
      return features;
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
      if (denoiser) {
        const render::DenoiserDiagnostics diagnostics = denoiser->diagnostics();
        lastMetrics.denoise.enabled = true;
        lastMetrics.denoise.denoiser = diagnostics.name;
        for (const auto& parameter : diagnostics.numericParameters) {
          lastMetrics.denoise.numericParameters.push_back(
            WavefrontRenderMetrics::DenoiseSummary::NumericParameter{parameter.name,
                                                                     parameter.value});
        }
      }
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
        lastMetrics.batching.compatibilityShadeSamples +=
          result.batchMetrics.compatibilityShadeSamples;
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
        if (result.batchMetrics.stoppedByConvergence) {
          ++lastMetrics.convergence.stoppedTileCount;
          const std::uint64_t depth = result.batchMetrics.stoppedAfterDepth;
          if (lastMetrics.convergence.earliestStoppedAfterDepth == 0 ||
              depth < lastMetrics.convergence.earliestStoppedAfterDepth) {
            lastMetrics.convergence.earliestStoppedAfterDepth = depth;
          }
          lastMetrics.convergence.latestStoppedAfterDepth =
            std::max(lastMetrics.convergence.latestStoppedAfterDepth, depth);
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
      if (!lastMetrics.convergence.enabled) {
        lastMetrics.convergence.decision = "disabled";
      } else if (lastMetrics.convergence.stoppedTileCount > 0) {
        lastMetrics.convergence.decision = "stopped_some_tiles";
      } else {
        lastMetrics.convergence.decision = "not_reached";
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    const render::Scene& scene, Buffer<Colord>& buffer, const Recti& rect,
                    std::optional<std::uint64_t> tileSeed,
                    const DenoiserFeatureSet* denoiserFeatures = nullptr) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      const auto progressDenoiser = denoiser ? denoiser->clone() : nullptr;
      const auto result = traceTile(
        camera, rayCaster, scene, actualRect, tileSeed,
        [&](const Recti& footprint) { writeColor(buffer, footprint, Colord(1, 0, 0)); },
        [&](const std::vector<TilePixel>& pixels) {
          const std::vector<TilePixel> displayPixels =
            progressDenoiser
              ? denoisedProgressPixels(pixels, actualRect, denoiserFeatures, *progressDenoiser)
              : pixels;
          for (const auto& pixel : displayPixels) {
            writeColor(buffer, pixel.footprint, pixel.color);
          }
        });
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

      const auto result = traceTile(
        camera, rayCaster, scene, actualRect, tileSeed,
        [&](const Recti& footprint) { writeRGB(buffer, footprint, 0xffff0000); },
        [&](const std::vector<TilePixel>& pixels) {
          for (const auto& pixel : pixels) {
            writeRGB(buffer, pixel.footprint,
                     (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
          }
        });
      recordTileMetrics(result);
      for (const auto& pixel : result.pixels) {
        writeRGB(buffer, pixel.footprint,
                 (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
      }
    }

    void renderTile(render::Camera& camera, const render::RayCaster& rayCaster,
                    const render::Scene& scene, Buffer<Colord>& hdrBuffer,
                    Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                    const Recti& rect, std::optional<std::uint64_t> tileSeed,
                    const DenoiserFeatureSet* denoiserFeatures = nullptr) const {
      const Recti actualRect = camera.renderableRect(rect);
      if (actualRect.width() <= 0 || actualRect.height() <= 0)
        return;

      const auto progressDenoiser = denoiser ? denoiser->clone() : nullptr;
      const auto result = traceTile(
        camera, rayCaster, scene, actualRect, tileSeed,
        [&](const Recti& footprint) {
          writeColor(hdrBuffer, footprint, Colord(1, 0, 0));
          writeRGB(displayBuffer, footprint, 0xffff0000);
        },
        [&](const std::vector<TilePixel>& pixels) {
          const std::vector<TilePixel> displayPixels =
            progressDenoiser
              ? denoisedProgressPixels(pixels, actualRect, denoiserFeatures, *progressDenoiser)
              : pixels;
          for (const auto& pixel : displayPixels) {
            writeColor(hdrBuffer, pixel.footprint, pixel.color);
            writeRGB(displayBuffer, pixel.footprint,
                     (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
          }
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
    if (p->denoiser) {
      result->setDenoiser(p->denoiser->clone());
    }
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
    const auto denoiserFeatures = p->buildDenoiserFeatures(*m_camera, *m_scene, buffer.rect());
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(tilePlan, *p->threadPool, p->tasks,
                              [this, rayCaster, camera, bufferPtr, samplingSeed,
                               denoiserFeaturePtr](const Recti& rect, std::size_t tileIndex) {
                                const std::optional<std::uint64_t> tileSeed =
                                  samplingSeed
                                    ? std::optional<std::uint64_t>(
                                        render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
                                    : std::nullopt;
                                p->renderTile(*camera, *rayCaster, *m_scene, *bufferPtr, rect,
                                              tileSeed, denoiserFeaturePtr);
                              });
    p->denoise(buffer, denoiserFeatures.get());
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

    if (p->denoiser) {
      Buffer<Colord> hdrBuffer(buffer.width(), buffer.height());
      render(hdrBuffer, buffer, tonemap());
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
    const auto denoiserFeatures = p->buildDenoiserFeatures(*m_camera, *m_scene, hdrBuffer.rect());
    const auto* denoiserFeaturePtr = denoiserFeatures.get();
    const auto samplingSeed = p->samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, *p->threadPool, p->tasks,
      [this, rayCaster, camera, hdrBufferPtr, displayBufferPtr, displayTonemap, samplingSeed,
       denoiserFeaturePtr](const Recti& rect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          samplingSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*samplingSeed, tileIndex))
            : std::nullopt;
        p->renderTile(*camera, *rayCaster, *m_scene, *hdrBufferPtr, *displayBufferPtr,
                      displayTonemap, rect, tileSeed, denoiserFeaturePtr);
      });
    p->denoise(hdrBuffer, denoiserFeatures.get());
    p->writeDisplayBuffer(displayBuffer, hdrBuffer, displayTonemap);
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

  void WavefrontRaytracer::setDenoiser(std::unique_ptr<render::Denoiser> denoiser) {
    p->denoiser = std::move(denoiser);
  }

  void WavefrontRaytracer::clearDenoiser() {
    p->denoiser.reset();
  }

  const render::Denoiser* WavefrontRaytracer::denoiser() const {
    return p->denoiser.get();
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
