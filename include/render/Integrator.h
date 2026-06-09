#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"
#include "render/WavefrontIntersectionQueryTiming.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace render {
  class RayCaster;
  class SampleStream;
  class Scene;
  class State;
  class WavefrontIntersectionBackend;

  struct IntegratorRaySample {
    Rayd ray{Rayd::undefined};
    double timeSample{0.0};
    std::shared_ptr<SampleStream> ownedSampleStream;
    SampleStream* borrowedSampleStream{nullptr};
    double animationFrame{0.0};
    double animationTime{0.0};

    SampleStream* sampleStream() const {
      return borrowedSampleStream ? borrowedSampleStream : ownedSampleStream.get();
    }
  };

  struct IntegratorBatchMetrics {
    bool usedScalarFallback{false};
    std::vector<std::uint64_t> activeSamplesPerDepth;
    std::vector<std::uint64_t> frontierRayHitsPerDepth;
    std::vector<std::uint64_t> frontierRayMissesPerDepth;
    std::vector<std::uint64_t> frontierPacketChunksPerDepth;
    std::vector<std::uint64_t> frontierPacketRaysPerDepth;
    std::vector<std::uint64_t> frontierClosestHitBatchChunksPerDepth;
    std::vector<std::uint64_t> frontierClosestHitBatchRaysPerDepth;
    std::vector<std::uint64_t> directLightAnyHitBatchChunksPerDepth;
    std::vector<std::uint64_t> directLightAnyHitBatchRaysPerDepth;
    std::vector<std::uint64_t> frontierRay4PacketChunksPerDepth;
    std::vector<std::uint64_t> frontierRay8PacketChunksPerDepth;
    std::vector<std::uint64_t> frontierScalarRaysPerDepth;
    std::vector<std::uint64_t> frontierPacketScalarFallbackRaysPerDepth;
    std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
    std::vector<std::uint64_t> frontierPacketRefinedRaysPerDepth;
    std::map<std::string, std::uint64_t> frontierPacketRefinedRaysByMaterial;
    std::uint64_t activeSampleDepthsProcessed{0};
    std::vector<double> radianceDeltaSquaredSumPerDepth;
    std::vector<double> maxRadianceDeltaPerDepth;
    std::uint64_t compatibilityShadeSamples{0};
    std::uint64_t unsupportedPathMaterialSamples{0};
    std::uint64_t emitterHitSamples{0};
    std::uint64_t primaryEmitterHitSamples{0};
    std::uint64_t deltaEmitterHitSamples{0};
    std::uint64_t bsdfEmitterHitSamples{0};
    std::uint64_t misWeightedEmitterHitSamples{0};
    std::uint64_t directLightSamples{0};
    std::uint64_t directLightContributingSamples{0};
    std::uint64_t directLightOccludedSamples{0};
    double emittedRadianceLuminanceSum{0.0};
    double directLightRadianceLuminanceSum{0.0};
    double primaryDirectLightRadianceLuminanceSum{0.0};
    double secondaryDirectLightRadianceLuminanceSum{0.0};
    double ambientRadianceLuminanceSum{0.0};
    double missRadianceLuminanceSum{0.0};
    double compatibilityShadeRadianceLuminanceSum{0.0};
    bool stoppedByConvergence{false};
    std::uint64_t stoppedAfterDepth{0};
    std::string intersectionBackendRequest;
    std::string intersectionBackend;
    std::string intersectionBackendPlatform;
    std::string intersectionBackendAvailability;
    std::string intersectionBackendFallbackReason;
    std::string intersectionBackendExecutionPath;
    bool intersectionBackendPlatformGpuDeviceAvailable{false};
    bool intersectionBackendPlatformGpuRenderPathAvailable{false};
    bool intersectionSceneCompiled{false};
    std::uint64_t intersectionSceneBvhNodes{0};
    std::uint64_t intersectionScenePrimitives{0};
    std::uint64_t intersectionSceneTriangles{0};
    std::uint64_t intersectionSceneSpheres{0};
    std::uint64_t intersectionScenePlanes{0};
    std::uint64_t intersectionSceneRectangles{0};
    std::uint64_t intersectionSceneDisks{0};
    std::uint64_t intersectionSceneOpenCylinders{0};
    std::uint64_t intersectionSceneTransforms{0};
    std::uint64_t intersectionSceneUnsupportedPrimitives{0};
    std::uint64_t intersectionSceneUploadBytes{0};
    bool intersectionSceneTriangleClosestHitEligible{false};
    bool intersectionSceneBasicHitEligible{false};
    bool intersectionScenePackedClosestHitEligible{false};
    bool intersectionScenePackedAnyHitEligible{false};
    std::uint64_t intersectionEstimatedRayUploadBytes{0};
    std::uint64_t intersectionEstimatedClosestHitReadbackBytes{0};
    std::uint64_t intersectionEstimatedAnyHitReadbackBytes{0};
    std::uint64_t intersectionEstimatedQueryTransferBytes{0};
    double intersectionBackendUploadWorkerSeconds{0.0};
    double intersectionBackendKernelWorkerSeconds{0.0};
    double intersectionBackendReadbackWorkerSeconds{0.0};
    std::uint64_t intersectionRaysSubmitted{0};
    std::uint64_t closestHitRaysSubmitted{0};
    std::uint64_t anyHitRaysSubmitted{0};
    std::uint64_t closestHitQueries{0};
    std::uint64_t anyHitQueries{0};
    bool intersectionBackendPrefersClosestHitBatch{false};
    bool intersectionBackendPrefersAnyHitBatch{false};
    double intersectionWorkerSeconds{0.0};
    double shadingWorkerSeconds{0.0};
    double pathSetupWorkerSeconds{0.0};
    double frontierPartitionWorkerSeconds{0.0};
    double frontierBookkeepingWorkerSeconds{0.0};
    double progressSnapshotWorkerSeconds{0.0};
    double convergenceTestWorkerSeconds{0.0};
    std::uint64_t observerConvergenceFeedbackDepths{0};
    std::vector<std::uint64_t> retainedActiveSamplesPerDepth;

    void reset(bool scalarFallback);
    void recordActiveDepth(std::uint64_t activeSamples);
    void recordRetainedActiveDepth(std::uint64_t activeSamples);
    void recordFrontierIntersections(std::uint64_t hitRays, std::uint64_t missRays);
    void recordFrontierTraversal(std::uint64_t packetChunks, std::uint64_t packetRays,
                                 std::uint64_t ray4PacketChunks, std::uint64_t ray8PacketChunks,
                                 std::uint64_t scalarRays, std::uint64_t packetScalarFallbackRays,
                                 std::uint64_t packetRefinedRays);
    void recordFrontierClosestHitBatch(std::uint64_t batchChunks, std::uint64_t batchRays);
    void recordDirectLightAnyHitBatch(std::uint64_t depth, std::uint64_t batchChunks,
                                      std::uint64_t batchRays);
    void recordPacketScalarFallbacksByReason(const std::map<std::string, std::uint64_t>& reasons);
    void recordPacketHitRefinement(const std::string& materialLabel);
    void recordIntersectionBackend(const WavefrontIntersectionBackend& backend);
    void recordIntersectionQueryFallbackReason(const WavefrontIntersectionBackend& backend,
                                               const WavefrontIntersectionQueryTiming& timing);
    void recordClosestHitQuery(const WavefrontIntersectionBackend& backend,
                               std::uint64_t submittedRays,
                               const WavefrontIntersectionQueryTiming& timing = {});
    void recordAnyHitQuery(const WavefrontIntersectionBackend& backend, std::uint64_t submittedRays,
                           const WavefrontIntersectionQueryTiming& timing = {});
    void mergeIntersectionBackendMetrics(const IntegratorBatchMetrics& source);
    void recordRadianceDeltaDepth(double squaredSum, double maxDelta);
    void recordUnsupportedPathMaterial();
    void recordEmitterHit(bool sampledFromBsdf, bool bsdfSampleDelta, bool misWeighted);
    void recordDirectLightSample(bool occluded, bool contributing);
    void recordEmittedRadiance(const Colord& contribution);
    void recordDirectLightRadiance(const Colord& contribution, bool primaryBounce);
    void recordAmbientRadiance(const Colord& contribution);
    void recordMissRadiance(const Colord& contribution);
    void recordCompatibilityShadeRadiance(const Colord& contribution);
    double contributionLuminance(const Colord& contribution) const;
  };

  struct IntegratorBatchFeedback {
    std::optional<double> convergenceRadianceDeltaRms;
  };

  class IntegratorBatchObserver {
  public:
    virtual ~IntegratorBatchObserver() = default;
    virtual IntegratorBatchFeedback depthCompleted(std::uint64_t completedDepth,
                                                   const std::vector<Colord>& sampleColors,
                                                   std::uint64_t activeSamples) = 0;
  };

  struct IntegratorBatchSettings {
    bool convergenceEnabled{false};
    double activeSampleFractionThreshold{0.0};
    double radianceDeltaRmsThreshold{0.0};
    IntegratorBatchObserver* progressObserver{nullptr};
    const WavefrontIntersectionBackend* intersectionBackend{nullptr};

    const WavefrontIntersectionBackend& resolvedIntersectionBackend() const;
  };

  /**
    * @brief Single-ray radiance evaluator.
    *
    * `Integrator` owns the policy for evaluating the radiance carried by one
    * ray through a scene. It is intentionally narrower than `RenderEngine`:
    * it has no camera, framebuffer, tonemap, tile scheduler, cancellation UI,
    * or worker-thread ownership. Those remain engine responsibilities.
    *
    * The first contract is shaped around the current Whitted renderer:
    *
    *  - `scene` is the non-owning scene being queried.
    *  - `ray` is the immutable ray to evaluate.
    *  - `state` is the mutable per-primary-ray recursion / statistics state.
    *  - `recursiveRayCaster` is the callback used by legacy materials to trace
    *    reflection, refraction, and portal rays.
    *
    * That split lets `Raytracer` remain the owner of image rendering and
    * single-ray probes while future code can move the Whitted radiance
    * algorithm behind this interface without changing cameras or materials.
    *
    * @see RenderEngine — owns camera / scene / framebuffer rendering.
    * @see RayCaster — compatibility callback for recursive material shading.
    * @see State — mutable bookkeeping threaded through radiance evaluation.
    */
  class Integrator {
  public:
    using CancellationCallback = std::function<bool()>;

    virtual ~Integrator() = default;

    /**
      * Creates an independent copy of this integrator.
      *
      * Render engines clone themselves for background render threads, so the
      * selected radiance policy must travel with that engine snapshot without
      * sharing mutable configuration.
      */
    virtual std::unique_ptr<Integrator> clone() const = 0;

    /**
      * Stable diagnostic identifier for graph traces and render metrics.
      */
    virtual const char* diagnosticName() const;

    /**
      * Describes how `radianceBatch(...)` schedules samples.
      *
      * The base implementation loops over scalar `radiance(...)` calls.
      * Integrators that override batch execution should override this too so
      * callers can expose the scheduling mode without probing concrete types.
      */
    virtual const char* batchExecutionMode() const;

    /**
      * @returns true when scalar camera rendering should publish the running
      * sample average after each sample pass. Path tracing benefits from this
      * because users can see the noisy estimate converge; Whitted-style
      * integrators keep the historical write-once-per-pixel behavior.
      */
    virtual bool prefersProgressiveSamplePublishing() const;

    /**
      * Estimate how many scene-intersection queries one primary camera sample
      * can generate. Wavefront backends use this only for coarse backend
      * selection; exact query counts remain runtime metrics.
      */
    virtual std::uint64_t estimatedIntersectionRaysPerPrimarySample() const;

    /**
      * Evaluate the radiance carried by `ray` in `scene`.
      *
      * Implementations may mutate `state` for recursion depth, hit-point
      * probes, event tracing, and performance counters. The scene and ray are
      * borrowed for the duration of the call; the integrator must not retain
      * references to them. Recursive Whitted-style implementations call back
      * through `recursiveRayCaster.rayColor(...)` when a material needs a
      * secondary ray evaluated.
      */
    virtual Colord radiance(const Scene& scene, const Rayd& ray, State& state,
                            const RayCaster& recursiveRayCaster) const = 0;

    /**
      * Evaluate a batch of primary ray samples.
      *
      * The default implementation loops over `radiance(...)` sample by sample.
      * Integrators with a better scheduling strategy can override this; for
      * example, `PathTracingIntegrator` processes the batch depth-major.
      */
    virtual std::vector<Colord> radianceBatch(const Scene& scene,
                                              const std::vector<IntegratorRaySample>& samples,
                                              const RayCaster& recursiveRayCaster,
                                              IntegratorBatchMetrics* metrics = nullptr,
                                              const IntegratorBatchSettings& settings = {}) const;

    /**
      * Configure the maximum ray depth when this integrator has a bounded
      * recursion / bounce count. Integrators that do not use this concept may
      * ignore it.
      */
    virtual void setMaximumRecursionDepth(int depth);

    /**
      * Configure a cooperative cancellation callback. Integrators that do not
      * perform long-running internal work may ignore it.
      */
    virtual void setCancellationCallback(CancellationCallback callback);

  protected:
    double radianceDeltaSquared(const Colord& before, const Colord& after) const;
  };
}
