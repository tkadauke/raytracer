#include <gtest/gtest.h>

#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "render/Integrator.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/PinholeCamera.h"
#include "render/denoise/BoxDenoiser.h"
#include "render/denoise/Denoiser.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/Buffer.h"
#include "core/math/Constants.h"

#include "test/helpers/ColorTestHelper.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace WavefrontRaytracerTest {
  using engine::wavefront::WavefrontRaytracer;

  class FillDenoiser final : public render::Denoiser {
  public:
    explicit FillDenoiser(const Colord& color)
        : m_color(color) {
    }

    std::unique_ptr<render::Denoiser> clone() const override {
      return std::make_unique<FillDenoiser>(m_color);
    }

    const char* diagnosticName() const override {
      return "fill";
    }

    void denoiseFrame(render::DenoiserFrame& frame) const override {
      frame.beauty.clear(m_color);
    }

  private:
    Colord m_color;
  };

  class FeatureRecordingDenoiser final : public render::Denoiser {
  public:
    std::unique_ptr<render::Denoiser> clone() const override {
      return std::make_unique<FeatureRecordingDenoiser>();
    }

    const char* diagnosticName() const override {
      return "feature_recording";
    }

    render::DenoiserFeatureRequest requestedFeatures() const override {
      return render::DenoiserFeatureRequest{true, true, true};
    }

    void denoiseFrame(render::DenoiserFrame& frame) const override {
      sawAlbedo = frame.features.albedo != nullptr;
      sawNormal = frame.features.normal != nullptr;
      sawDepth = frame.features.depth != nullptr;
      if (sawAlbedo && sawNormal && sawDepth) {
        featureWidth = frame.features.albedo->width();
        featureHeight = frame.features.albedo->height();
        for (int y = 0; y != featureHeight; ++y) {
          for (int x = 0; x != featureWidth; ++x) {
            maxAlbedo = std::max(maxAlbedo, (*frame.features.albedo)[y][x].max());
            maxNormalLength = std::max(maxNormalLength, (*frame.features.normal)[y][x].length());
            maxDepth = std::max(maxDepth, (*frame.features.depth)[y][x]);
          }
        }
      }
    }

    mutable bool sawAlbedo{false};
    mutable bool sawNormal{false};
    mutable bool sawDepth{false};
    mutable int featureWidth{0};
    mutable int featureHeight{0};
    mutable double maxAlbedo{0.0};
    mutable double maxNormalLength{0.0};
    mutable double maxDepth{0.0};
  };

  struct SharedProgressState {
    int batchesWithProgressObserver{0};
    int batchesWithMetrics{0};
  };

  class ProgressPublishingIntegrator final : public render::Integrator {
  public:
    explicit ProgressPublishingIntegrator(std::shared_ptr<SharedProgressState> state = nullptr)
        : m_state(std::move(state)) {
    }

    std::unique_ptr<render::Integrator> clone() const override {
      return std::make_unique<ProgressPublishingIntegrator>(m_state);
    }

    const char* diagnosticName() const override {
      return "progress_publishing";
    }

    Colord radiance(const render::Scene&, const Rayd&, render::State&,
                    const render::RayCaster&) const override {
      return Colord::black();
    }

    std::vector<Colord>
    radianceBatch(const render::Scene&, const std::vector<render::IntegratorRaySample>& samples,
                  const render::RayCaster&, render::IntegratorBatchMetrics* metrics = nullptr,
                  const render::IntegratorBatchSettings& settings = {}) const override {
      if (metrics) {
        metrics->reset(/*scalarFallback=*/false);
        metrics->recordActiveDepth(samples.size());
        metrics->recordRetainedActiveDepth(samples.size());
        metrics->recordFrontierIntersections(samples.size(), 0);
        if (m_state) {
          ++m_state->batchesWithMetrics;
        }
      }
      if (settings.progressObserver) {
        if (m_state) {
          ++m_state->batchesWithProgressObserver;
        }
        const render::IntegratorBatchFeedback feedback = settings.progressObserver->depthCompleted(
          1, std::vector<Colord>(samples.size(), Colord(1.0, 0.0, 0.0)), samples.size());
        if (metrics && feedback.convergenceRadianceDeltaRms) {
          ++metrics->observerConvergenceFeedbackDepths;
        }
      }
      return std::vector<Colord>(samples.size(), Colord::black());
    }

  private:
    std::shared_ptr<SharedProgressState> m_state;
  };

  class DepthRecordingIntegrator final : public render::Integrator {
  public:
    std::unique_ptr<render::Integrator> clone() const override {
      auto result = std::make_unique<DepthRecordingIntegrator>();
      result->m_maximumRecursionDepth = m_maximumRecursionDepth;
      return result;
    }

    Colord radiance(const render::Scene&, const Rayd&, render::State&,
                    const render::RayCaster&) const override {
      return Colord::black();
    }

    void setMaximumRecursionDepth(int depth) override {
      m_maximumRecursionDepth = depth;
    }

    int maximumRecursionDepth() const {
      return m_maximumRecursionDepth;
    }

  private:
    int m_maximumRecursionDepth{0};
  };

  struct SharedDenoiserCallState {
    int calls{0};
    int featureCalls{0};
  };

  class SharedRecordingDenoiser final : public render::Denoiser {
  public:
    explicit SharedRecordingDenoiser(std::shared_ptr<SharedDenoiserCallState> state)
        : m_state(std::move(state)) {
    }

    std::unique_ptr<render::Denoiser> clone() const override {
      return std::make_unique<SharedRecordingDenoiser>(m_state);
    }

    const char* diagnosticName() const override {
      return "shared_recording";
    }

    render::DenoiserFeatureRequest requestedFeatures() const override {
      return render::DenoiserFeatureRequest{true, true, true};
    }

    void denoiseFrame(render::DenoiserFrame& frame) const override {
      ++m_state->calls;
      if (frame.features.albedo && frame.features.normal && frame.features.depth) {
        ++m_state->featureCalls;
      }
      frame.beauty.clear(Colord(0.25, 0.5, 0.75));
    }

  private:
    std::shared_ptr<SharedDenoiserCallState> m_state;
  };

  class SlowMissPrimitive final : public render::Primitive {
  public:
    explicit SlowMissPrimitive(std::chrono::milliseconds delay)
        : m_delay(delay) {
    }

    const render::Primitive* intersect(const Rayd&, HitPointInterval&,
                                       render::State&) const override {
      std::this_thread::sleep_for(m_delay);
      return nullptr;
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override {
      return BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    }

  private:
    std::chrono::milliseconds m_delay;
  };

  std::shared_ptr<render::Scene> testScene() {
    auto scene = std::make_shared<render::Scene>(Colord(0.1, 0.2, 0.3));
    scene->setAmbient(Colord::white());
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.0);
    sphere->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord::white())));
    scene->add(sphere);
    return scene;
  }

  std::shared_ptr<render::Scene> featureScene() {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->setAmbient(Colord::white());
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.0e6);
    sphere->setMaterial(std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord(0.7, 0.5, 0.25))));
    scene->add(sphere);
    return scene;
  }

  std::shared_ptr<render::PinholeCamera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  TEST(WavefrontRaytracer, DefaultsToWhittedIntegrator) {
    WavefrontRaytracer renderer(std::make_shared<render::Scene>());

    EXPECT_NE(nullptr, dynamic_cast<const render::WhittedIntegrator*>(&renderer.integrator()));
    EXPECT_FALSE(renderer.metricsEnabled());
    EXPECT_FALSE(renderer.convergenceEnabled());
    EXPECT_DOUBLE_EQ(RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD,
                     renderer.convergenceActiveSampleFractionThreshold());
    EXPECT_DOUBLE_EQ(RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD,
                     renderer.convergenceRadianceDeltaRmsThreshold());
  }

  TEST(WavefrontRaytracer, AppliesMaximumRecursionDepthToCurrentIntegrator) {
    WavefrontRaytracer renderer(std::make_shared<render::Scene>());
    renderer.setIntegrator(std::make_unique<DepthRecordingIntegrator>());

    renderer.setMaximumRecursionDepth(7);

    const auto* integrator = dynamic_cast<const DepthRecordingIntegrator*>(&renderer.integrator());
    ASSERT_NE(nullptr, integrator);
    EXPECT_EQ(7, integrator->maximumRecursionDepth());
  }

  TEST(WavefrontRaytracer, ReappliesMaximumRecursionDepthWhenIntegratorChanges) {
    WavefrontRaytracer renderer(std::make_shared<render::Scene>());

    renderer.setMaximumRecursionDepth(6);
    renderer.setIntegrator(std::make_unique<DepthRecordingIntegrator>());

    const auto* integrator = dynamic_cast<const DepthRecordingIntegrator*>(&renderer.integrator());
    ASSERT_NE(nullptr, integrator);
    EXPECT_EQ(6, integrator->maximumRecursionDepth());
  }

  TEST(WavefrontRaytracer, ClonesConfigurationForRenderThreadSnapshots) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(3);
    renderer->setSamplingSeed(42);
    renderer->setShowProgressIndicators(true);
    renderer->setProgressiveDisplayEnabled(false);
    renderer->setMetricsEnabled(true);
    renderer->setConvergenceEnabled(true);
    renderer->setConvergenceActiveSampleFractionThreshold(0.25);
    renderer->setConvergenceRadianceDeltaRmsThreshold(0.002);
    renderer->setDenoiser(std::make_unique<render::BoxDenoiser>(2));

    auto clone = std::dynamic_pointer_cast<WavefrontRaytracer>(renderer->cloneForRender());
    ASSERT_NE(nullptr, clone);
    ASSERT_TRUE(clone->samplingSeed().has_value());
    EXPECT_EQ(42u, *clone->samplingSeed());
    EXPECT_TRUE(clone->convergenceEnabled());
    EXPECT_DOUBLE_EQ(0.25, clone->convergenceActiveSampleFractionThreshold());
    EXPECT_DOUBLE_EQ(0.002, clone->convergenceRadianceDeltaRmsThreshold());
    EXPECT_FALSE(clone->progressiveDisplayEnabled());
    EXPECT_TRUE(clone->metricsEnabled());
    ASSERT_NE(nullptr, clone->denoiser());
    EXPECT_STREQ("box", clone->denoiser()->diagnosticName());
    EXPECT_NE(renderer->denoiser(), clone->denoiser());
    EXPECT_NE(renderer->camera(), clone->camera());
    EXPECT_EQ(renderer->scene(), clone->scene());
  }

  TEST(WavefrontRaytracer, AppliesDenoiserAfterHdrRender) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setDenoiser(std::make_unique<FillDenoiser>(Colord(0.25, 0.5, 0.75)));

    Buffer<Colord> buffer(4, 3);
    renderer->render(buffer);

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), buffer[y][x], 1e-12);
      }
    }
  }

  TEST(WavefrontRaytracer, AppliesDenoiserBeforeDisplayConversion) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setDenoiser(std::make_unique<FillDenoiser>(Colord(0.25, 0.5, 0.75)));

    Buffer<unsigned int> buffer(4, 3);
    renderer->render(buffer);

    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        EXPECT_EQ(Colord(0.25, 0.5, 0.75).rgb(), buffer[y][x]);
      }
    }
  }

  TEST(WavefrontRaytracer, DenoisesDualOutputHdrBeforeDisplayConversion) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setDenoiser(std::make_unique<FillDenoiser>(Colord(0.25, 0.5, 0.75)));

    Buffer<Colord> hdrBuffer(4, 3);
    Buffer<unsigned int> displayBuffer(4, 3);
    renderer->render(hdrBuffer, displayBuffer, nullptr);

    for (int y = 0; y != hdrBuffer.height(); ++y) {
      for (int x = 0; x != hdrBuffer.width(); ++x) {
        ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), hdrBuffer[y][x], 1e-12);
        EXPECT_EQ(Colord(0.25, 0.5, 0.75).rgb(), displayBuffer[y][x]);
      }
    }
  }

  TEST(WavefrontRaytracer, CancellationStopsTileSampleSubmission) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->cancel();

    Buffer<Colord> buffer(4, 3);
    renderer->render(buffer);

    const auto metrics = renderer->lastMetrics();
    EXPECT_EQ(1u, metrics.tiling.tileCount);
    EXPECT_EQ(0u, metrics.tiling.nonEmptyTileCount);
    EXPECT_EQ(0u, metrics.input.renderedPixels);
    EXPECT_EQ(0u, metrics.input.primarySamples);
    EXPECT_EQ(0u, metrics.batching.batches);
    EXPECT_EQ(0.0, metrics.batching.averageBatchSize);
  }

  TEST(WavefrontRaytracer, RecordsDenoiserMetrics) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setMetricsEnabled(true);
    renderer->setDenoiser(std::make_unique<render::BoxDenoiser>(2));

    Buffer<Colord> buffer(4, 3);
    renderer->render(buffer);

    const auto metrics = renderer->lastMetrics();
    EXPECT_TRUE(metrics.denoise.enabled);
    EXPECT_EQ("box", metrics.denoise.denoiser);
    ASSERT_EQ(1u, metrics.denoise.numericParameters.size());
    EXPECT_EQ("radius", metrics.denoise.numericParameters.front().name);
    EXPECT_DOUBLE_EQ(2.0, metrics.denoise.numericParameters.front().value);
    EXPECT_FALSE(metrics.denoise.albedoFeature);
    EXPECT_FALSE(metrics.denoise.normalFeature);
    EXPECT_FALSE(metrics.denoise.depthFeature);
    EXPECT_EQ(0u, metrics.denoise.featureTileCount);
    EXPECT_EQ(0u, metrics.denoise.completedFeatureTileCount);
    EXPECT_EQ(0u, metrics.denoise.featurePixels);
    EXPECT_DOUBLE_EQ(0.0, metrics.denoise.featureSeconds);
    EXPECT_GE(metrics.denoise.seconds, 0.0);

    const QJsonObject denoise = metrics.toJson().value("denoise").toObject();
    EXPECT_TRUE(denoise.value("enabled").toBool());
    EXPECT_EQ("box", denoise.value("denoiser").toString().toStdString());
    EXPECT_DOUBLE_EQ(2.0, denoise.value("parameters").toObject().value("radius").toDouble());
    EXPECT_FALSE(denoise.value("features").toObject().value("albedo").toBool());
    EXPECT_FALSE(denoise.value("features").toObject().value("normal").toBool());
    EXPECT_FALSE(denoise.value("features").toObject().value("depth").toBool());
    const QJsonObject prepass = denoise.value("featurePrepass").toObject();
    EXPECT_EQ(0.0, prepass.value("tileCount").toDouble());
    EXPECT_EQ(0.0, prepass.value("completedTileCount").toDouble());
    EXPECT_EQ(0.0, prepass.value("pixels").toDouble());
    EXPECT_DOUBLE_EQ(0.0, prepass.value("seconds").toDouble());
    EXPECT_DOUBLE_EQ(0.0, denoise.value("featureSeconds").toDouble());
    EXPECT_GE(denoise.value("seconds").toDouble(), 0.0);
  }

  TEST(WavefrontRaytracer, ReportsActiveTilesDuringDenoiserFeaturePrepass) {
    auto scene = std::make_shared<render::Scene>(Colord::black());
    scene->add(std::make_shared<SlowMissPrimitive>(std::chrono::milliseconds(2)));
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), scene);
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setDenoiser(std::make_unique<FeatureRecordingDenoiser>());

    Buffer<unsigned int> buffer(8, 8);
    std::atomic<bool> done{false};
    std::thread renderThread([&] {
      renderer->render(buffer);
      done.store(true, std::memory_order_release);
    });

    bool sawFeaturePrepassTile = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
      const auto metrics = renderer->lastMetrics();
      if (!renderer->activeTiles().empty() && metrics.input.renderedPixels == 0 &&
          metrics.denoise.completedFeatureTileCount < metrics.denoise.featureTileCount) {
        sawFeaturePrepassTile = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    renderThread.join();
    EXPECT_TRUE(sawFeaturePrepassTile);
    EXPECT_EQ(64u, renderer->lastMetrics().denoise.featurePixels);
  }

  TEST(WavefrontRaytracer, SuppliesPrimaryHitFeatureBuffersToDenoiser) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), featureScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    auto denoiser = std::make_unique<FeatureRecordingDenoiser>();
    const auto* recordingDenoiser = denoiser.get();
    renderer->setDenoiser(std::move(denoiser));

    Buffer<Colord> buffer(8, 6);
    renderer->render(buffer);

    EXPECT_TRUE(recordingDenoiser->sawAlbedo);
    EXPECT_TRUE(recordingDenoiser->sawNormal);
    EXPECT_TRUE(recordingDenoiser->sawDepth);
    EXPECT_EQ(8, recordingDenoiser->featureWidth);
    EXPECT_EQ(6, recordingDenoiser->featureHeight);
    EXPECT_GT(recordingDenoiser->maxAlbedo, 0.0);
    EXPECT_GT(recordingDenoiser->maxNormalLength, 0.0);
    EXPECT_GT(recordingDenoiser->maxDepth, 0.0);
  }

  TEST(WavefrontRaytracer, DenoisesDepthProgressSnapshotsBeforeFinalDenoise) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), featureScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setIntegrator(std::make_unique<ProgressPublishingIntegrator>());
    auto denoiserCalls = std::make_shared<SharedDenoiserCallState>();
    renderer->setDenoiser(std::make_unique<SharedRecordingDenoiser>(denoiserCalls));

    Buffer<Colord> buffer(4, 3);
    renderer->render(buffer);

    EXPECT_GE(denoiserCalls->calls, 2);
    EXPECT_GE(denoiserCalls->featureCalls, 2);
  }

  TEST(WavefrontRaytracer, SkipsDepthProgressSnapshotsWhenProgressiveDisplayDisabled) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), featureScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setProgressiveDisplayEnabled(false);
    auto progressState = std::make_shared<SharedProgressState>();
    renderer->setIntegrator(std::make_unique<ProgressPublishingIntegrator>(progressState));

    Buffer<unsigned int> buffer(4, 3);
    renderer->render(buffer);

    EXPECT_EQ(0, progressState->batchesWithProgressObserver);
  }

  TEST(WavefrontRaytracer, UsesDenoisedProgressForConvergenceFeedbackWhenDisplayIsDisabled) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), featureScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setProgressiveDisplayEnabled(false);
    renderer->setConvergenceEnabled(true);
    renderer->setMetricsEnabled(true);
    auto progressState = std::make_shared<SharedProgressState>();
    renderer->setIntegrator(std::make_unique<ProgressPublishingIntegrator>(progressState));
    renderer->setDenoiser(std::make_unique<render::BoxDenoiser>(1));

    Buffer<Colord> buffer(4, 3);
    renderer->render(buffer);

    EXPECT_GT(progressState->batchesWithProgressObserver, 0);
    const auto metrics = renderer->lastMetrics();
    EXPECT_GT(metrics.convergence.feedbackDepthCount, 0u);
    EXPECT_EQ(
      metrics.convergence.feedbackDepthCount,
      metrics.toJson().value("convergence").toObject().value("feedbackDepthCount").toDouble());
  }

  TEST(WavefrontRaytracer, SendsBatchMetricsOnlyWhenMetricsEnabled) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), featureScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    auto progressState = std::make_shared<SharedProgressState>();
    renderer->setIntegrator(std::make_unique<ProgressPublishingIntegrator>(progressState));

    Buffer<unsigned int> buffer(4, 3);
    renderer->render(buffer);

    EXPECT_EQ(0, progressState->batchesWithMetrics);

    renderer->setMetricsEnabled(true);
    renderer->render(buffer);

    EXPECT_GT(progressState->batchesWithMetrics, 0);
  }

  TEST(WavefrontRaytracer, ClearsLastMetricsWhenMetricsAreDisabled) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);

    Buffer<unsigned int> buffer(4, 3);
    renderer->setMetricsEnabled(true);
    renderer->render(buffer);
    EXPECT_GT(renderer->lastMetrics().input.primarySamples, 0u);

    renderer->setMetricsEnabled(false);
    renderer->render(buffer);

    const auto metrics = renderer->lastMetrics();
    EXPECT_EQ(0, metrics.input.width);
    EXPECT_EQ(0, metrics.input.height);
    EXPECT_EQ(0u, metrics.input.primarySamples);
    EXPECT_EQ(0u, metrics.tiling.tileCount);
  }

  TEST(WavefrontRaytracer, MatchesRecursiveRaytracerForSimpleWhittedScene) {
    auto scene = testScene();
    auto recursive = std::make_shared<engine::raytracer::Raytracer>(camera(), scene);
    auto wavefront = std::make_shared<WavefrontRaytracer>(camera(), scene);
    recursive->setMaximumThreads(1);
    recursive->setQueueSize(1);
    wavefront->setMaximumThreads(1);
    wavefront->setQueueSize(1);

    Buffer<Colord> recursiveBuffer(16, 12);
    Buffer<Colord> wavefrontBuffer(16, 12);
    recursive->render(recursiveBuffer);
    wavefront->render(wavefrontBuffer);

    for (int y = 0; y != recursiveBuffer.height(); ++y) {
      for (int x = 0; x != recursiveBuffer.width(); ++x) {
        ASSERT_COLOR_NEAR(recursiveBuffer[y][x], wavefrontBuffer[y][x], 1e-12);
      }
    }
  }

  TEST(WavefrontRaytracer, RecordsLastRenderMetrics) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(1);
    renderer->setMetricsEnabled(true);
    renderer->setConvergenceEnabled(true);
    renderer->setConvergenceActiveSampleFractionThreshold(0.5);
    renderer->setConvergenceRadianceDeltaRmsThreshold(0.01);

    Buffer<Colord> buffer(8, 6);
    renderer->render(buffer);

    const auto metrics = renderer->lastMetrics();
    EXPECT_EQ(8, metrics.input.width);
    EXPECT_EQ(6, metrics.input.height);
    EXPECT_EQ(1, metrics.input.samplesPerPixel);
    EXPECT_EQ(48u, metrics.input.renderedPixels);
    EXPECT_EQ(48u, metrics.input.primarySamples);
    EXPECT_EQ(1u, metrics.tiling.tileCount);
    EXPECT_EQ(1u, metrics.tiling.tileRows);
    EXPECT_EQ(1u, metrics.tiling.tileColumns);
    EXPECT_EQ(8u, metrics.tiling.maxTileWidth);
    EXPECT_EQ(6u, metrics.tiling.maxTileHeight);
    EXPECT_EQ(48u, metrics.tiling.maxTilePixels);
    EXPECT_DOUBLE_EQ(48.0, metrics.tiling.averageTilePixels);
    EXPECT_EQ(1u, metrics.tiling.nonEmptyTileCount);
    EXPECT_EQ(48u, metrics.tiling.minNonEmptyTileSamples);
    EXPECT_EQ(48u, metrics.tiling.maxTileSamples);
    EXPECT_DOUBLE_EQ(48.0, metrics.tiling.averageNonEmptyTileSamples);
    EXPECT_EQ(1u, metrics.scheduling.configuredQueueSize);
    EXPECT_EQ(1u, metrics.scheduling.resolvedQueueSize);
    EXPECT_EQ("single_tile", metrics.scheduling.decision);
    EXPECT_EQ("whitted", metrics.batching.integrator);
    EXPECT_EQ("depth_major_whitted", metrics.batching.executionMode);
    EXPECT_EQ(1u, metrics.batching.batches);
    EXPECT_EQ(48u, metrics.batching.samplesSubmitted);
    EXPECT_EQ(48u, metrics.batching.activeSampleDepthsProcessed);
    EXPECT_EQ(48u, metrics.batching.maxBatchSize);
    EXPECT_DOUBLE_EQ(48.0, metrics.batching.averageBatchSize);
    EXPECT_EQ(0u, metrics.batching.compatibilityShadeSamples);
    ASSERT_EQ(1u, metrics.batching.activeSamplesPerDepth.size());
    EXPECT_EQ(48u, metrics.batching.activeSamplesPerDepth[0]);
    ASSERT_EQ(1u, metrics.batching.retainedActiveSamplesPerDepth.size());
    EXPECT_EQ(0u, metrics.batching.retainedActiveSamplesPerDepth[0]);
    ASSERT_EQ(1u, metrics.batching.frontierRayHitsPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierRayMissesPerDepth.size());
    EXPECT_EQ(48u, metrics.batching.frontierRayHitsPerDepth[0] +
                     metrics.batching.frontierRayMissesPerDepth[0]);
    ASSERT_EQ(1u, metrics.batching.frontierPacketChunksPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierPacketRaysPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierRay4PacketChunksPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierRay8PacketChunksPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierScalarRaysPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierPacketScalarFallbackRaysPerDepth.size());
    ASSERT_EQ(1u, metrics.batching.frontierPacketRefinedRaysPerDepth.size());
    EXPECT_EQ(6u, metrics.batching.frontierPacketChunksPerDepth[0]);
    EXPECT_EQ(48u, metrics.batching.frontierPacketRaysPerDepth[0]);
    EXPECT_EQ(0u, metrics.batching.frontierRay4PacketChunksPerDepth[0]);
    EXPECT_EQ(6u, metrics.batching.frontierRay8PacketChunksPerDepth[0]);
    EXPECT_EQ(0u, metrics.batching.frontierScalarRaysPerDepth[0]);
    EXPECT_EQ(0u, metrics.batching.frontierPacketScalarFallbackRaysPerDepth[0]);
    EXPECT_TRUE(metrics.batching.frontierPacketScalarFallbackRaysByReason.empty());
    EXPECT_EQ(0u, metrics.batching.frontierPacketRefinedRaysPerDepth[0]);
    EXPECT_TRUE(metrics.batching.frontierPacketRefinedRaysByMaterial.empty());
    EXPECT_TRUE(metrics.convergence.enabled);
    EXPECT_DOUBLE_EQ(0.5, metrics.convergence.activeSampleFractionThreshold);
    EXPECT_DOUBLE_EQ(0.01, metrics.convergence.radianceDeltaRmsThreshold);
    EXPECT_EQ(0u, metrics.convergence.stoppedTileCount);
    EXPECT_EQ(0u, metrics.convergence.feedbackDepthCount);
    EXPECT_TRUE(metrics.convergence.stoppedTileDepthHistogram.empty());
    EXPECT_EQ("not_reached", metrics.convergence.decision);
    ASSERT_EQ(1u, metrics.batching.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_GT(metrics.batching.radianceDeltaSquaredSumPerDepth[0], 0.0);
    ASSERT_EQ(1u, metrics.batching.maxRadianceDeltaPerDepth.size());
    EXPECT_GT(metrics.batching.maxRadianceDeltaPerDepth[0], 0.0);
    EXPECT_GE(metrics.timings.sampleGenerationWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.sampleStreamWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.primaryRayWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.sampleEnqueueWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorBatchWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorIntersectionWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorShadingWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorOverheadWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorPathSetupWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorFrontierBookkeepingWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorProgressSnapshotWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorConvergenceTestWorkerSeconds, 0.0);
    EXPECT_GE(metrics.timings.integratorResidualWorkerSeconds, 0.0);
    EXPECT_GT(metrics.timings.totalRenderSeconds, 0.0);

    const QJsonObject json = metrics.toJson();
    const QJsonObject tiling = json.value("tiling").toObject();
    EXPECT_EQ(1.0, tiling.value("tileCount").toDouble());
    EXPECT_EQ(1.0, tiling.value("tileRows").toDouble());
    EXPECT_EQ(1.0, tiling.value("tileColumns").toDouble());
    EXPECT_EQ(8.0, tiling.value("maxTileWidth").toDouble());
    EXPECT_EQ(6.0, tiling.value("maxTileHeight").toDouble());
    EXPECT_EQ(48.0, tiling.value("maxTilePixels").toDouble());
    EXPECT_DOUBLE_EQ(48.0, tiling.value("averageTilePixels").toDouble());
    EXPECT_EQ(1.0, tiling.value("nonEmptyTileCount").toDouble());
    EXPECT_EQ(48.0, tiling.value("minNonEmptyTileSamples").toDouble());
    EXPECT_EQ(48.0, tiling.value("maxTileSamples").toDouble());
    EXPECT_DOUBLE_EQ(48.0, tiling.value("averageNonEmptyTileSamples").toDouble());
    EXPECT_EQ("whitted",
              json.value("batching").toObject().value("integrator").toString().toStdString());
    EXPECT_EQ(48.0, json.value("batching").toObject().value("samplesSubmitted").toDouble());
    EXPECT_EQ(48.0,
              json.value("batching").toObject().value("activeSampleDepthsProcessed").toDouble());
    EXPECT_EQ(0.0, json.value("batching").toObject().value("compatibilityShadeSamples").toDouble());
    const QJsonArray activeSamples =
      json.value("batching").toObject().value("activeSamplesPerDepth").toArray();
    ASSERT_EQ(1, activeSamples.size());
    EXPECT_EQ(48.0, activeSamples.at(0).toDouble());
    const QJsonArray retainedActiveSamples =
      json.value("batching").toObject().value("retainedActiveSamplesPerDepth").toArray();
    ASSERT_EQ(1, retainedActiveSamples.size());
    EXPECT_EQ(0.0, retainedActiveSamples.at(0).toDouble());
    const QJsonArray frontierHits =
      json.value("batching").toObject().value("frontierRayHitsPerDepth").toArray();
    const QJsonArray frontierMisses =
      json.value("batching").toObject().value("frontierRayMissesPerDepth").toArray();
    ASSERT_EQ(1, frontierHits.size());
    ASSERT_EQ(1, frontierMisses.size());
    EXPECT_EQ(48.0, frontierHits.at(0).toDouble() + frontierMisses.at(0).toDouble());
    const QJsonArray frontierPacketChunks =
      json.value("batching").toObject().value("frontierPacketChunksPerDepth").toArray();
    const QJsonArray frontierPacketRays =
      json.value("batching").toObject().value("frontierPacketRaysPerDepth").toArray();
    const QJsonArray frontierRay4PacketChunks =
      json.value("batching").toObject().value("frontierRay4PacketChunksPerDepth").toArray();
    const QJsonArray frontierRay8PacketChunks =
      json.value("batching").toObject().value("frontierRay8PacketChunksPerDepth").toArray();
    const QJsonArray frontierScalarRays =
      json.value("batching").toObject().value("frontierScalarRaysPerDepth").toArray();
    const QJsonArray frontierPacketScalarFallbackRays =
      json.value("batching").toObject().value("frontierPacketScalarFallbackRaysPerDepth").toArray();
    const QJsonArray frontierPacketRefinedRays =
      json.value("batching").toObject().value("frontierPacketRefinedRaysPerDepth").toArray();
    ASSERT_EQ(1, frontierPacketChunks.size());
    ASSERT_EQ(1, frontierPacketRays.size());
    ASSERT_EQ(1, frontierRay4PacketChunks.size());
    ASSERT_EQ(1, frontierRay8PacketChunks.size());
    ASSERT_EQ(1, frontierScalarRays.size());
    ASSERT_EQ(1, frontierPacketScalarFallbackRays.size());
    ASSERT_EQ(1, frontierPacketRefinedRays.size());
    EXPECT_EQ(6.0, frontierPacketChunks.at(0).toDouble());
    EXPECT_EQ(48.0, frontierPacketRays.at(0).toDouble());
    EXPECT_EQ(0.0, frontierRay4PacketChunks.at(0).toDouble());
    EXPECT_EQ(6.0, frontierRay8PacketChunks.at(0).toDouble());
    EXPECT_EQ(0.0, frontierScalarRays.at(0).toDouble());
    EXPECT_EQ(0.0, frontierPacketScalarFallbackRays.at(0).toDouble());
    EXPECT_EQ(0.0, frontierPacketRefinedRays.at(0).toDouble());
    EXPECT_TRUE(json.value("batching")
                  .toObject()
                  .value("frontierPacketScalarFallbackRaysByReason")
                  .toObject()
                  .isEmpty());
    EXPECT_TRUE(json.value("batching")
                  .toObject()
                  .value("frontierPacketRefinedRaysByMaterial")
                  .toObject()
                  .isEmpty());
    EXPECT_TRUE(json.value("convergence").toObject().value("enabled").toBool());
    EXPECT_DOUBLE_EQ(
      0.5, json.value("convergence").toObject().value("activeSampleFractionThreshold").toDouble());
    EXPECT_DOUBLE_EQ(
      0.01, json.value("convergence").toObject().value("radianceDeltaRmsThreshold").toDouble());
    EXPECT_EQ(0.0, json.value("convergence").toObject().value("stoppedTileCount").toDouble());
    EXPECT_EQ(0.0, json.value("convergence").toObject().value("feedbackDepthCount").toDouble());
    EXPECT_TRUE(
      json.value("convergence").toObject().value("stoppedTileDepthHistogram").toArray().empty());
    EXPECT_EQ("not_reached",
              json.value("convergence").toObject().value("decision").toString().toStdString());
    EXPECT_FALSE(json.value("denoise").toObject().value("enabled").toBool());
    const QJsonArray deltaL2 =
      json.value("batching").toObject().value("radianceDeltaL2PerDepth").toArray();
    ASSERT_EQ(1, deltaL2.size());
    EXPECT_GT(deltaL2.at(0).toDouble(), 0.0);
    const QJsonArray deltaRms =
      json.value("batching").toObject().value("radianceDeltaRmsPerDepth").toArray();
    ASSERT_EQ(1, deltaRms.size());
    EXPECT_GT(deltaRms.at(0).toDouble(), 0.0);
    const QJsonArray maxDelta =
      json.value("batching").toObject().value("maxRadianceDeltaPerDepth").toArray();
    ASSERT_EQ(1, maxDelta.size());
    EXPECT_GT(maxDelta.at(0).toDouble(), 0.0);
    const QJsonObject timings = json.value("timings").toObject();
    EXPECT_TRUE(timings.contains("sampleStreamWorkerSeconds"));
    EXPECT_TRUE(timings.contains("primaryRayWorkerSeconds"));
    EXPECT_TRUE(timings.contains("sampleEnqueueWorkerSeconds"));
    EXPECT_TRUE(timings.contains("sampleGenerationOverheadWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorIntersectionWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorShadingWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorOverheadWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorPathSetupWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorFrontierBookkeepingWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorProgressSnapshotWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorConvergenceTestWorkerSeconds"));
    EXPECT_TRUE(timings.contains("integratorResidualWorkerSeconds"));
    EXPECT_GE(timings.value("sampleGenerationWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleStreamWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("primaryRayWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleEnqueueWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("sampleGenerationOverheadWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorBatchWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorIntersectionWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorShadingWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorOverheadWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorPathSetupWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorFrontierBookkeepingWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorProgressSnapshotWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorConvergenceTestWorkerSeconds").toDouble(), 0.0);
    EXPECT_GE(timings.value("integratorResidualWorkerSeconds").toDouble(), 0.0);
  }
}
