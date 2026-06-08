#pragma once

#include "core/math/Rect.h"
#include "render/RenderEngine.h"

#include <cstdint>
#include <list>
#include <map>
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
  struct IntegratorBatchMetrics;
  class Scene;
  class TilePlan;
  class WavefrontIntersectionBackendChoice;
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
      std::uint64_t tileRows = 0;
      std::uint64_t tileColumns = 0;
      std::uint64_t maxTileWidth = 0;
      std::uint64_t maxTileHeight = 0;
      std::uint64_t maxTilePixels = 0;
      double averageTilePixels = 0.0;
      std::uint64_t nonEmptyTileCount = 0;
      std::uint64_t minNonEmptyTileSamples = 0;
      std::uint64_t maxTileSamples = 0;
      double averageNonEmptyTileSamples = 0.0;

      void resetFromTilePlan(const render::TilePlan& tilePlan);
    } tiling;

    struct SchedulingSummary {
      std::uint64_t configuredQueueSize = 0;
      std::uint64_t resolvedQueueSize = 0;
      std::string decision;
    } scheduling;

    struct BatchSummary {
      std::string integrator;
      std::string executionMode;
      std::string intersectionBackendRequest;
      std::string intersectionBackend;
      std::string intersectionBackendAvailability;
      std::string intersectionBackendFallbackReason;
      std::string intersectionBackendExecutionPath;
      bool intersectionSceneCompiled = false;
      std::uint64_t intersectionSceneBvhNodes = 0;
      std::uint64_t intersectionScenePrimitives = 0;
      std::uint64_t intersectionSceneTriangles = 0;
      std::uint64_t intersectionSceneSpheres = 0;
      std::uint64_t intersectionScenePlanes = 0;
      std::uint64_t intersectionSceneRectangles = 0;
      std::uint64_t intersectionSceneDisks = 0;
      std::uint64_t intersectionSceneTransforms = 0;
      std::uint64_t intersectionSceneUnsupportedPrimitives = 0;
      std::uint64_t intersectionSceneUploadBytes = 0;
      bool intersectionSceneTriangleClosestHitEligible = false;
      bool intersectionSceneBasicHitEligible = false;
      bool intersectionScenePackedClosestHitEligible = false;
      bool intersectionScenePackedAnyHitEligible = false;
      std::uint64_t intersectionEstimatedRayUploadBytes = 0;
      std::uint64_t intersectionEstimatedClosestHitReadbackBytes = 0;
      std::uint64_t intersectionEstimatedAnyHitReadbackBytes = 0;
      std::uint64_t intersectionEstimatedQueryTransferBytes = 0;
      double intersectionBackendUploadWorkerSeconds = 0.0;
      double intersectionBackendKernelWorkerSeconds = 0.0;
      double intersectionBackendReadbackWorkerSeconds = 0.0;
      std::uint64_t batches = 0;
      std::uint64_t samplesSubmitted = 0;
      std::uint64_t maxBatchSize = 0;
      double averageBatchSize = 0.0;
      std::uint64_t intersectionRaysSubmitted = 0;
      std::uint64_t closestHitQueries = 0;
      std::uint64_t anyHitQueries = 0;
      std::uint64_t activeSampleDepthsProcessed = 0;
      std::uint64_t compatibilityShadeSamples = 0;
      std::uint64_t unsupportedPathMaterialSamples = 0;
      std::uint64_t emitterHitSamples = 0;
      std::uint64_t primaryEmitterHitSamples = 0;
      std::uint64_t deltaEmitterHitSamples = 0;
      std::uint64_t bsdfEmitterHitSamples = 0;
      std::uint64_t misWeightedEmitterHitSamples = 0;
      std::uint64_t directLightSamples = 0;
      std::uint64_t directLightContributingSamples = 0;
      std::uint64_t directLightOccludedSamples = 0;
      double emittedRadianceLuminanceSum = 0.0;
      double directLightRadianceLuminanceSum = 0.0;
      double primaryDirectLightRadianceLuminanceSum = 0.0;
      double secondaryDirectLightRadianceLuminanceSum = 0.0;
      double ambientRadianceLuminanceSum = 0.0;
      double missRadianceLuminanceSum = 0.0;
      double compatibilityShadeRadianceLuminanceSum = 0.0;
      std::vector<std::uint64_t> activeSamplesPerDepth;
      std::vector<std::uint64_t> retainedActiveSamplesPerDepth;
      std::vector<std::uint64_t> frontierRayHitsPerDepth;
      std::vector<std::uint64_t> frontierRayMissesPerDepth;
      std::vector<std::uint64_t> frontierPacketChunksPerDepth;
      std::vector<std::uint64_t> frontierPacketRaysPerDepth;
      std::vector<std::uint64_t> frontierRay4PacketChunksPerDepth;
      std::vector<std::uint64_t> frontierRay8PacketChunksPerDepth;
      std::vector<std::uint64_t> frontierScalarRaysPerDepth;
      std::vector<std::uint64_t> frontierPacketScalarFallbackRaysPerDepth;
      std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
      std::vector<std::uint64_t> frontierPacketRefinedRaysPerDepth;
      std::map<std::string, std::uint64_t> frontierPacketRefinedRaysByMaterial;
      std::vector<double> radianceDeltaSquaredSumPerDepth;
      std::vector<double> maxRadianceDeltaPerDepth;
      std::uint64_t sampleVariancePixelArea = 0;
      double sampleRadianceVarianceSum = 0.0;
      double maxSampleRadianceStddev = 0.0;

      void addIntegratorMetrics(const render::IntegratorBatchMetrics& metrics);
      void addIntersectionBackendMetrics(const render::IntegratorBatchMetrics& metrics);
    } batching;

    struct ConvergenceSummary {
      bool enabled = false;
      double activeSampleFractionThreshold = 0.0;
      double radianceDeltaRmsThreshold = 0.0;
      std::uint64_t stoppedTileCount = 0;
      std::uint64_t earliestStoppedAfterDepth = 0;
      std::uint64_t latestStoppedAfterDepth = 0;
      std::uint64_t feedbackDepthCount = 0;
      std::vector<std::uint64_t> stoppedTileDepthHistogram;
      std::string decision;

      void recordStoppedTileAfterDepth(std::uint64_t depth);
    } convergence;

    struct AdaptiveSamplingSummary {
      bool enabled = false;
      int minimumSamples = 1;
      double stddevThreshold = 0.0;
      std::uint64_t maximumPrimarySamples = 0;
      std::uint64_t skippedPrimarySamples = 0;
      double skippedPrimarySampleFraction = 0.0;
    } adaptiveSampling;

    struct DenoiseSummary {
      struct NumericParameter {
        std::string name;
        double value = 0.0;
      };

      bool enabled = false;
      std::string denoiser;
      std::vector<NumericParameter> numericParameters;
      bool albedoFeature = false;
      bool normalFeature = false;
      bool depthFeature = false;
      std::uint64_t featureTileCount = 0;
      std::uint64_t completedFeatureTileCount = 0;
      std::uint64_t featurePixels = 0;
      double featureSeconds = 0.0;
      double seconds = 0.0;
    } denoise;

    struct TimingSummary {
      double sampleGenerationWorkerSeconds = 0.0;
      double sampleStreamWorkerSeconds = 0.0;
      double primaryRayWorkerSeconds = 0.0;
      double sampleEnqueueWorkerSeconds = 0.0;
      double integratorBatchWorkerSeconds = 0.0;
      double integratorIntersectionWorkerSeconds = 0.0;
      double integratorShadingWorkerSeconds = 0.0;
      double integratorOverheadWorkerSeconds = 0.0;
      double integratorPathSetupWorkerSeconds = 0.0;
      double integratorFrontierPartitionWorkerSeconds = 0.0;
      double integratorFrontierBookkeepingWorkerSeconds = 0.0;
      double integratorProgressSnapshotWorkerSeconds = 0.0;
      double integratorConvergenceTestWorkerSeconds = 0.0;
      double integratorResidualWorkerSeconds = 0.0;
      double totalRenderSeconds = 0.0;

      void recordIntegratorBatch(double batchSeconds,
                                 const render::IntegratorBatchMetrics& batchMetrics);
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
    void setMetricsEnabled(bool enabled);
    bool metricsEnabled() const;

    /**
      * Configure the maximum ray recursion / bounce depth for the selected
      * integrator. The configured value is retained by the raytracer and
      * applied again when `setIntegrator(...)` installs a replacement.
      */
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
    void setAdaptiveSamplingEnabled(bool enabled);
    bool adaptiveSamplingEnabled() const;
    void setAdaptiveMinimumSamples(int samples);
    int adaptiveMinimumSamples() const;
    void setAdaptiveStddevThreshold(double threshold);
    double adaptiveStddevThreshold() const;
    void setSampleRadianceStddevCaptureEnabled(bool enabled);
    bool sampleRadianceStddevCaptureEnabled() const;
    void setIntersectionBackend(render::WavefrontIntersectionBackendChoice backend);
    render::WavefrontIntersectionBackendChoice intersectionBackend() const;
    std::shared_ptr<const Buffer<double>> lastSampleRadianceStddev() const;
    std::shared_ptr<const Buffer<Colord>> lastSampleRadianceStddevColor() const;
    WavefrontRenderMetrics lastMetrics() const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
