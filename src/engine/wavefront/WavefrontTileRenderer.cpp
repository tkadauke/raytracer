#include "engine/wavefront/detail/WavefrontTileRenderer.h"

#include "core/math/HitPointInterval.h"
#include "engine/TileRenderTask.h"
#include "render/RayCaster.h"
#include "render/SamplingSeed.h"
#include "render/State.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/denoise/Denoiser.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "render/tonemap/Tonemap.h"

#include <QThreadPool>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace engine::wavefront::detail {
  namespace {
    using TileProgressPublisher = std::function<void(const std::vector<WavefrontTilePixel>&)>;

    class TileProgressObserver final : public render::IntegratorBatchObserver {
    public:
      TileProgressObserver(std::vector<WavefrontTilePixel>& pixels,
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
      std::vector<WavefrontTilePixel>& m_pixels;
      const std::vector<std::size_t>& m_samplePixelIndices;
      double m_sampleScale;
      TileProgressPublisher m_publisher;
    };

    void writeColor(Buffer<Colord>& buffer, const Recti& footprint, const Colord& color) {
      for (int y = footprint.top(); y != footprint.bottom(); ++y) {
        for (int x = footprint.left(); x != footprint.right(); ++x) {
          buffer[y][x] = color;
        }
      }
    }

    void writeRGB(Buffer<unsigned int>& buffer, const Recti& footprint, unsigned int rgb) {
      for (int y = footprint.top(); y != footprint.bottom(); ++y) {
        for (int x = footprint.left(); x != footprint.right(); ++x) {
          buffer[y][x] = rgb;
        }
      }
    }

    render::IntegratorBatchSettings batchSettings(const WavefrontTileRenderConfig& config) {
      render::IntegratorBatchSettings settings;
      settings.convergenceEnabled = config.convergenceEnabled;
      settings.activeSampleFractionThreshold = config.convergenceActiveSampleFractionThreshold;
      settings.radianceDeltaRmsThreshold = config.convergenceRadianceDeltaRmsThreshold;
      return settings;
    }

    WavefrontTileTraceResult traceTile(const WavefrontTileRenderConfig& config,
                                       render::Camera& camera, const render::RayCaster& rayCaster,
                                       const render::Scene& scene, const Recti& actualRect,
                                       std::optional<std::uint64_t> tileSeed,
                                       const std::function<void(const Recti&)>& markProgress,
                                       TileProgressPublisher publishProgress,
                                       bool publishProgressSnapshots) {
      WavefrontTileTraceResult result;
      const auto sampleGenerationStart = WavefrontMetricsRecorder::Clock::now();
      std::vector<render::IntegratorRaySample> samples;
      render::SampleStreamStorage sampleStreams;
      std::vector<std::size_t> samplePixelIndices;
      const int sampleCount = camera.samplesPerPixel();
      const std::size_t estimatedPixelCount =
        static_cast<std::size_t>(std::max(0, actualRect.width())) *
        static_cast<std::size_t>(std::max(0, actualRect.height()));
      result.pixels.reserve(estimatedPixelCount);
      samples.reserve(estimatedPixelCount * static_cast<std::size_t>(std::max(0, sampleCount)));
      sampleStreams.reserve(samples.capacity());
      samplePixelIndices.reserve(samples.capacity());

      const auto measureValue = [&config](double& target, const auto& operation) {
        if (!config.metricsEnabled) {
          return operation();
        }
        const auto start = WavefrontMetricsRecorder::Clock::now();
        auto value = operation();
        target +=
          std::chrono::duration<double>(WavefrontMetricsRecorder::Clock::now() - start).count();
        return value;
      };
      const auto measureVoid = [&config](double& target, const auto& operation) {
        if (!config.metricsEnabled) {
          operation();
          return;
        }
        const auto start = WavefrontMetricsRecorder::Clock::now();
        operation();
        target +=
          std::chrono::duration<double>(WavefrontMetricsRecorder::Clock::now() - start).count();
      };

      auto plane = camera.viewPlane();
      const auto sampler = plane->sampler();
      const auto primaryRayGenerator = camera.primaryRayGenerator();
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled()) {
          break;
        }

        const std::size_t pixelIndex = result.pixels.size();
        const Recti footprint = pixel.footprintWithin(actualRect);
        if (config.showProgressIndicators) {
          markProgress(footprint);
        }
        result.pixels.push_back(WavefrontTilePixel{footprint, Colord::black()});
        const std::uint64_t pixelHash = camera.primaryRayPixelHash(pixel, tileSeed);

        for (int sampleIndex = 0; sampleIndex != sampleCount; ++sampleIndex) {
          if (camera.isCancelled()) {
            break;
          }

          render::SampleStream* stream = measureValue(result.sampleStreamWorkerSeconds, [&] {
            return sampler->appendStream(sampleStreams, sampleIndex, pixelHash);
          });
          auto sample = measureValue(result.primaryRayWorkerSeconds,
                                     [&] { return primaryRayGenerator->sample(pixel, *stream); });
          if (sample) {
            measureVoid(result.sampleEnqueueWorkerSeconds, [&] {
              samples.push_back(
                render::IntegratorRaySample{sample->ray, sample->timeSample, nullptr, stream});
              samplePixelIndices.push_back(pixelIndex);
            });
          }
        }
      }
      result.sampleGenerationWorkerSeconds =
        std::chrono::duration<double>(WavefrontMetricsRecorder::Clock::now() -
                                      sampleGenerationStart)
          .count();

      const double sampleScale = sampleCount > 0 ? 1.0 / sampleCount : 0.0;
      TileProgressObserver progressObserver(result.pixels, samplePixelIndices, sampleScale,
                                            std::move(publishProgress));
      render::IntegratorBatchSettings settings = batchSettings(config);
      settings.progressObserver =
        publishProgressSnapshots && !samples.empty() ? &progressObserver : nullptr;

      const auto integratorBatchStart = WavefrontMetricsRecorder::Clock::now();
      const std::vector<Colord> sampleColors =
        config.integrator.radianceBatch(scene, samples, rayCaster,
                                        config.metricsEnabled ? &result.batchMetrics : nullptr,
                                        settings);
      result.integratorBatchWorkerSeconds =
        std::chrono::duration<double>(WavefrontMetricsRecorder::Clock::now() -
                                      integratorBatchStart)
          .count();
      progressObserver.applySampleColors(sampleColors);
      result.sampleCount = samples.size();
      return result;
    }

    void buildDenoiserFeatureTile(WavefrontDenoiserFeatureSet& features, render::Camera& camera,
                                  const render::Scene& scene, const Recti& actualRect,
                                  std::optional<std::uint64_t> tileSeed) {
      if (actualRect.width() <= 0 || actualRect.height() <= 0) {
        return;
      }

      auto plane = camera.viewPlane();
      const auto sampler = plane->sampler();
      render::SampleStreamStorage sampleStreams;
      sampleStreams.reserve(static_cast<std::size_t>(std::max(0, actualRect.width())) *
                            static_cast<std::size_t>(std::max(0, actualRect.height())));
      for (render::ViewPlane::Iterator pixel = plane->begin(actualRect),
                                       end = plane->end(actualRect);
           pixel != end; ++pixel) {
        if (camera.isCancelled()) {
          break;
        }

        render::SampleStream* stream = sampler->appendStream(
          sampleStreams, /*sampleIndex=*/0, camera.primaryRayPixelHash(pixel, tileSeed));
        const auto sample = camera.primaryRaySample(pixel, *stream);
        if (!sample) {
          continue;
        }

        render::State state;
        state.timeSample = sample->timeSample;
        state.sampleStream = stream;
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
        features.write(pixel.footprintWithin(actualRect), albedo,
                       hitPoint.normal().normalizedOrZero(1e-12), hitPoint.distance());
      }
    }

    void copyDenoiserFeatureTile(const WavefrontDenoiserFeatureSet& source,
                                 WavefrontDenoiserFeatureSet& target, const Recti& actualRect) {
      for (int y = actualRect.top(); y != actualRect.bottom(); ++y) {
        for (int x = actualRect.left(); x != actualRect.right(); ++x) {
          const int tileX = x - actualRect.left();
          const int tileY = y - actualRect.top();
          if (source.hasSourcePixel(x, y)) {
            target.copyPixelFrom(source, tileX, tileY, x, y);
          }
        }
      }
    }

    Colord averageFootprintColor(const Buffer<Colord>& buffer, const Recti& footprint,
                                 const Recti& actualRect) {
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

    std::vector<WavefrontTilePixel>
    denoisedProgressPixels(const std::vector<WavefrontTilePixel>& pixels, const Recti& actualRect,
                           const WavefrontDenoiserFeatureSet* features,
                           const render::Denoiser& progressDenoiser) {
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

      std::unique_ptr<WavefrontDenoiserFeatureSet> tileFeatures;
      render::DenoiserFrame frame(beauty);
      if (features) {
        tileFeatures =
          std::make_unique<WavefrontDenoiserFeatureSet>(actualRect.width(), actualRect.height(),
                                                        features->requestedFeatures());
        copyDenoiserFeatureTile(*features, *tileFeatures, actualRect);
        frame.features = tileFeatures->buffers();
      }
      progressDenoiser.denoiseFrame(frame);

      std::vector<WavefrontTilePixel> result = pixels;
      for (auto& pixel : result) {
        pixel.color = averageFootprintColor(beauty, pixel.footprint, actualRect);
      }
      return result;
    }
  }

  WavefrontTileRenderer::WavefrontTileRenderer(WavefrontTileRenderConfig config,
                                               WavefrontMetricsRecorder& metrics)
      : m_config(config),
        m_metrics(metrics) {
  }

  void
  WavefrontTileRenderer::renderHdrTile(render::Camera& camera, const render::RayCaster& rayCaster,
                                       const render::Scene& scene, Buffer<Colord>& buffer,
                                       const Recti& rect, std::optional<std::uint64_t> tileSeed,
                                       bool publishProgressSnapshots,
                                       const WavefrontDenoiserFeatureSet* denoiserFeatures) const {
    const Recti actualRect = camera.renderableRect(rect);
    if (actualRect.width() <= 0 || actualRect.height() <= 0) {
      return;
    }

    const auto progressDenoiser = m_config.denoiser ? m_config.denoiser->clone() : nullptr;
    const auto result = traceTile(
      m_config, camera, rayCaster, scene, actualRect, tileSeed,
      [&](const Recti& footprint) { writeColor(buffer, footprint, Colord(1, 0, 0)); },
      [&](const std::vector<WavefrontTilePixel>& pixels) {
        const std::vector<WavefrontTilePixel> displayPixels =
          progressDenoiser
            ? denoisedProgressPixels(pixels, actualRect, denoiserFeatures, *progressDenoiser)
            : pixels;
        for (const auto& pixel : displayPixels) {
          writeColor(buffer, pixel.footprint, pixel.color);
        }
      },
      publishProgressSnapshots);
    if (m_config.metricsEnabled) {
      m_metrics.recordTile(result);
    }
    for (const auto& pixel : result.pixels) {
      writeColor(buffer, pixel.footprint, pixel.color);
    }
  }

  void WavefrontTileRenderer::renderDisplayTile(
    render::Camera& camera, const render::RayCaster& rayCaster, const render::Scene& scene,
    Buffer<unsigned int>& buffer, std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
    std::optional<std::uint64_t> tileSeed, bool publishProgressSnapshots) const {
    const Recti actualRect = camera.renderableRect(rect);
    if (actualRect.width() <= 0 || actualRect.height() <= 0) {
      return;
    }

    const auto result = traceTile(
      m_config, camera, rayCaster, scene, actualRect, tileSeed,
      [&](const Recti& footprint) { writeRGB(buffer, footprint, 0xffff0000); },
      [&](const std::vector<WavefrontTilePixel>& pixels) {
        for (const auto& pixel : pixels) {
          writeRGB(buffer, pixel.footprint,
                   (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
        }
      },
      publishProgressSnapshots);
    if (m_config.metricsEnabled) {
      m_metrics.recordTile(result);
    }
    for (const auto& pixel : result.pixels) {
      writeRGB(buffer, pixel.footprint,
               (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
    }
  }

  void WavefrontTileRenderer::renderDualOutputTile(
    render::Camera& camera, const render::RayCaster& rayCaster, const render::Scene& scene,
    Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
    std::shared_ptr<render::Tonemap> tonemap, const Recti& rect,
    std::optional<std::uint64_t> tileSeed, bool publishProgressSnapshots,
    const WavefrontDenoiserFeatureSet* denoiserFeatures) const {
    const Recti actualRect = camera.renderableRect(rect);
    if (actualRect.width() <= 0 || actualRect.height() <= 0) {
      return;
    }

    const auto progressDenoiser = m_config.denoiser ? m_config.denoiser->clone() : nullptr;
    const auto result = traceTile(
      m_config, camera, rayCaster, scene, actualRect, tileSeed,
      [&](const Recti& footprint) {
        writeColor(hdrBuffer, footprint, Colord(1, 0, 0));
        writeRGB(displayBuffer, footprint, 0xffff0000);
      },
      [&](const std::vector<WavefrontTilePixel>& pixels) {
        const std::vector<WavefrontTilePixel> displayPixels =
          progressDenoiser
            ? denoisedProgressPixels(pixels, actualRect, denoiserFeatures, *progressDenoiser)
            : pixels;
        for (const auto& pixel : displayPixels) {
          writeColor(hdrBuffer, pixel.footprint, pixel.color);
          writeRGB(displayBuffer, pixel.footprint,
                   (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
        }
      },
      publishProgressSnapshots);
    if (m_config.metricsEnabled) {
      m_metrics.recordTile(result);
    }
    for (const auto& pixel : result.pixels) {
      writeColor(hdrBuffer, pixel.footprint, pixel.color);
      writeRGB(displayBuffer, pixel.footprint,
               (tonemap ? tonemap->apply(pixel.color) : pixel.color).rgb());
    }
  }

  std::unique_ptr<WavefrontDenoiserFeatureSet> WavefrontTileRenderer::buildDenoiserFeatures(
    render::Camera& camera, const render::Scene& scene, const Recti& rect,
    const render::TilePlan& tilePlan, QThreadPool& threadPool,
    std::list<std::shared_ptr<engine::TileRenderTask>>& tasks) const {
    if (!m_config.denoiser) {
      return nullptr;
    }

    const render::DenoiserFeatureRequest requestedFeatures = m_config.denoiser->requestedFeatures();
    if (!requestedFeatures.any()) {
      return nullptr;
    }

    const auto featureStart = WavefrontMetricsRecorder::Clock::now();
    auto features =
      std::make_unique<WavefrontDenoiserFeatureSet>(rect.width(), rect.height(), requestedFeatures);
    const auto renderSeed = m_config.samplingSeed;
    engine::dispatchTileTasks(
      tilePlan, threadPool, tasks, [&](const Recti& tileRect, std::size_t tileIndex) {
        const std::optional<std::uint64_t> tileSeed =
          renderSeed
            ? std::optional<std::uint64_t>(render::SamplingSeed::tileSeed(*renderSeed, tileIndex))
            : std::nullopt;
        const Recti actualRect = camera.renderableRect(tileRect);
        buildDenoiserFeatureTile(*features, camera, scene, actualRect, tileSeed);
        if (m_config.metricsEnabled) {
          m_metrics.recordDenoiserFeatureTile(actualRect);
        }
      });
    if (m_config.metricsEnabled) {
      m_metrics.recordDenoiserFeatureSeconds(featureStart);
    }
    return features;
  }

  void WavefrontTileRenderer::denoise(Buffer<Colord>& buffer,
                                      const WavefrontDenoiserFeatureSet* features) const {
    if (!m_config.denoiser) {
      return;
    }

    const auto denoiseStart = WavefrontMetricsRecorder::Clock::now();
    render::DenoiserFrame frame(buffer);
    if (features) {
      frame.features = features->buffers();
    }
    m_config.denoiser->denoiseFrame(frame);
    const double seconds =
      std::chrono::duration<double>(WavefrontMetricsRecorder::Clock::now() - denoiseStart).count();
    if (m_config.metricsEnabled) {
      m_metrics.recordDenoise(frame.features.albedo != nullptr, frame.features.normal != nullptr,
                              frame.features.depth != nullptr, seconds);
    }
  }

  void WavefrontTileRenderer::writeDisplayBuffer(Buffer<unsigned int>& displayBuffer,
                                                 const Buffer<Colord>& hdrBuffer,
                                                 std::shared_ptr<render::Tonemap> tonemap) const {
    for (int y = 0; y != hdrBuffer.height(); ++y) {
      for (int x = 0; x != hdrBuffer.width(); ++x) {
        displayBuffer[y][x] = (tonemap ? tonemap->apply(hdrBuffer[y][x]) : hdrBuffer[y][x]).rgb();
      }
    }
  }
}
