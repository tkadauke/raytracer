#include <gtest/gtest.h>

#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/PinholeCamera.h"
#include "render/denoise/BoxDenoiser.h"
#include "render/denoise/Denoiser.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/Buffer.h"

#include "test/helpers/ColorTestHelper.h"

#include <QJsonArray>
#include <QJsonObject>

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

    void denoise(Buffer<Colord>& buffer) const override {
      buffer.clear(m_color);
    }

  private:
    Colord m_color;
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

  std::shared_ptr<render::PinholeCamera> camera() {
    return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
  }

  TEST(WavefrontRaytracer, DefaultsToWhittedIntegrator) {
    WavefrontRaytracer renderer(std::make_shared<render::Scene>());

    EXPECT_NE(nullptr, dynamic_cast<const render::WhittedIntegrator*>(&renderer.integrator()));
  }

  TEST(WavefrontRaytracer, ClonesConfigurationForRenderThreadSnapshots) {
    auto renderer = std::make_shared<WavefrontRaytracer>(camera(), testScene());
    renderer->setMaximumThreads(1);
    renderer->setQueueSize(3);
    renderer->setSamplingSeed(42);
    renderer->setShowProgressIndicators(true);
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
    EXPECT_EQ(1u, metrics.tiling.nonEmptyTileCount);
    EXPECT_EQ(1u, metrics.scheduling.configuredQueueSize);
    EXPECT_EQ(1u, metrics.scheduling.resolvedQueueSize);
    EXPECT_EQ("single_tile", metrics.scheduling.decision);
    EXPECT_EQ("whitted", metrics.batching.integrator);
    EXPECT_EQ("depth_major_whitted", metrics.batching.executionMode);
    EXPECT_EQ(1u, metrics.batching.batches);
    EXPECT_EQ(48u, metrics.batching.samplesSubmitted);
    EXPECT_EQ(48u, metrics.batching.maxBatchSize);
    EXPECT_DOUBLE_EQ(48.0, metrics.batching.averageBatchSize);
    EXPECT_EQ(0u, metrics.batching.compatibilityShadeSamples);
    ASSERT_EQ(1u, metrics.batching.activeSamplesPerDepth.size());
    EXPECT_EQ(48u, metrics.batching.activeSamplesPerDepth[0]);
    EXPECT_TRUE(metrics.convergence.enabled);
    EXPECT_DOUBLE_EQ(0.5, metrics.convergence.activeSampleFractionThreshold);
    EXPECT_DOUBLE_EQ(0.01, metrics.convergence.radianceDeltaRmsThreshold);
    EXPECT_EQ(0u, metrics.convergence.stoppedTileCount);
    EXPECT_EQ("not_reached", metrics.convergence.decision);
    ASSERT_EQ(1u, metrics.batching.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_GT(metrics.batching.radianceDeltaSquaredSumPerDepth[0], 0.0);
    ASSERT_EQ(1u, metrics.batching.maxRadianceDeltaPerDepth.size());
    EXPECT_GT(metrics.batching.maxRadianceDeltaPerDepth[0], 0.0);
    EXPECT_GT(metrics.timings.totalRenderSeconds, 0.0);

    const QJsonObject json = metrics.toJson();
    EXPECT_EQ("whitted",
              json.value("batching").toObject().value("integrator").toString().toStdString());
    EXPECT_EQ(48.0, json.value("batching").toObject().value("samplesSubmitted").toDouble());
    EXPECT_EQ(0.0, json.value("batching").toObject().value("compatibilityShadeSamples").toDouble());
    const QJsonArray activeSamples =
      json.value("batching").toObject().value("activeSamplesPerDepth").toArray();
    ASSERT_EQ(1, activeSamples.size());
    EXPECT_EQ(48.0, activeSamples.at(0).toDouble());
    EXPECT_TRUE(json.value("convergence").toObject().value("enabled").toBool());
    EXPECT_DOUBLE_EQ(
      0.5, json.value("convergence").toObject().value("activeSampleFractionThreshold").toDouble());
    EXPECT_DOUBLE_EQ(
      0.01, json.value("convergence").toObject().value("radianceDeltaRmsThreshold").toDouble());
    EXPECT_EQ(0.0, json.value("convergence").toObject().value("stoppedTileCount").toDouble());
    EXPECT_EQ("not_reached",
              json.value("convergence").toObject().value("decision").toString().toStdString());
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
  }
}
