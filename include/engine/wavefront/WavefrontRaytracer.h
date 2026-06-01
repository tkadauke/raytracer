#pragma once

#include "core/math/Rect.h"
#include "render/RenderEngine.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

template<class T>
class Buffer;

class QJsonObject;

namespace render {
  class Camera;
  class Denoiser;
  class Integrator;
  class Scene;
}

namespace engine::wavefront {
  struct WavefrontRenderMetrics {
    struct InputSummary {
      int width = 0;
      int height = 0;
      int samplesPerPixel = 0;
      std::uint64_t renderedPixels = 0;
      std::uint64_t primarySamples = 0;
    } input;

    struct TilingSummary {
      std::uint64_t tileCount = 0;
      std::uint64_t nonEmptyTileCount = 0;
    } tiling;

    struct SchedulingSummary {
      std::uint64_t configuredQueueSize = 0;
      std::uint64_t resolvedQueueSize = 0;
      std::string decision;
    } scheduling;

    struct BatchSummary {
      std::string integrator;
      std::string executionMode;
      std::uint64_t batches = 0;
      std::uint64_t samplesSubmitted = 0;
      std::uint64_t maxBatchSize = 0;
      double averageBatchSize = 0.0;
      std::uint64_t compatibilityShadeSamples = 0;
      std::vector<std::uint64_t> activeSamplesPerDepth;
      std::vector<double> radianceDeltaSquaredSumPerDepth;
      std::vector<double> maxRadianceDeltaPerDepth;
    } batching;

    struct ConvergenceSummary {
      bool enabled = false;
      double activeSampleFractionThreshold = 0.0;
      double radianceDeltaRmsThreshold = 0.0;
      std::uint64_t stoppedTileCount = 0;
      std::uint64_t earliestStoppedAfterDepth = 0;
      std::uint64_t latestStoppedAfterDepth = 0;
      std::string decision;
    } convergence;

    struct DenoiseSummary {
      struct NumericParameter {
        std::string name;
        double value = 0.0;
      };

      bool enabled = false;
      std::string denoiser;
      std::vector<NumericParameter> numericParameters;
      double seconds = 0.0;
    } denoise;

    struct TimingSummary {
      double totalRenderSeconds = 0.0;
    } timings;

    QJsonObject toJson() const;
  };

  /**
    * @brief Depth-major ray rendering engine scaffold.
    *
    * `WavefrontRaytracer` is a sibling to `engine::raytracer::Raytracer`,
    * not a replacement. It owns framebuffer/tile scheduling and progressive
    * display state like every `RenderEngine`, but deliberately does not expose
    * the public `RayCaster` probe API (`rayColor`, `rayState`,
    * `primitiveForRay`). Single-ray picking stays on the recursive raytracer.
    *
    * The implementation reuses the existing camera/sample-stream contract and
    * submits tile samples through the selected integrator's batch API. Whitted
    * batches process material-published continuation rays as explicit
    * depth-major queues and keep a private recursive adapter for legacy
    * material callbacks that have not exposed continuations yet.
    */
  class WavefrontRaytracer : public render::RenderEngine {
  public:
    explicit WavefrontRaytracer(std::shared_ptr<render::Scene> scene);
    explicit WavefrontRaytracer(std::shared_ptr<render::Camera> camera,
                                std::shared_ptr<render::Scene> scene);
    ~WavefrontRaytracer() override;

    using RenderEngine::render;
    std::shared_ptr<render::RenderEngine> cloneForRender() const override;

    void render(Buffer<Colord>& buffer) override;
    void render(Buffer<unsigned int>& buffer) override;
    void render(Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                std::shared_ptr<render::Tonemap> displayTonemap);

    void cancel() override;
    void uncancel() override;
    std::list<Recti> activeTiles() const override;
    std::list<Recti> completedTiles() const override;

    void setIntegrator(std::unique_ptr<render::Integrator> integrator);
    const render::Integrator& integrator() const;
    void setDenoiser(std::unique_ptr<render::Denoiser> denoiser);
    void clearDenoiser();
    const render::Denoiser* denoiser() const;
    void setMaximumRecursionDepth(int depth);

    void setSamplingSeed(std::uint64_t seed);
    void clearSamplingSeed();
    std::optional<std::uint64_t> samplingSeed() const;

    void setMaximumThreads(int threads);
    void setQueueSize(int queue);
    void setShowProgressIndicators(bool show);
    void setConvergenceEnabled(bool enabled);
    bool convergenceEnabled() const;
    void setConvergenceActiveSampleFractionThreshold(double fraction);
    double convergenceActiveSampleFractionThreshold() const;
    void setConvergenceRadianceDeltaRmsThreshold(double threshold);
    double convergenceRadianceDeltaRmsThreshold() const;
    WavefrontRenderMetrics lastMetrics() const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
