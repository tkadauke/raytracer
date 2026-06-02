#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"

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

  struct IntegratorRaySample {
    Rayd ray;
    double timeSample{0.0};
    std::shared_ptr<SampleStream> ownedSampleStream;
    SampleStream* borrowedSampleStream{nullptr};

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
    bool stoppedByConvergence{false};
    std::uint64_t stoppedAfterDepth{0};
    double intersectionWorkerSeconds{0.0};
    double shadingWorkerSeconds{0.0};
    double pathSetupWorkerSeconds{0.0};
    double frontierBookkeepingWorkerSeconds{0.0};
    double progressSnapshotWorkerSeconds{0.0};
    double convergenceTestWorkerSeconds{0.0};
    std::uint64_t observerConvergenceFeedbackDepths{0};

    void reset(bool scalarFallback);
    void recordActiveDepth(std::uint64_t activeSamples);
    void recordFrontierIntersections(std::uint64_t hitRays, std::uint64_t missRays);
    void recordFrontierTraversal(std::uint64_t packetChunks, std::uint64_t packetRays,
                                 std::uint64_t ray4PacketChunks, std::uint64_t ray8PacketChunks,
                                 std::uint64_t scalarRays, std::uint64_t packetScalarFallbackRays,
                                 std::uint64_t packetRefinedRays);
    void recordPacketScalarFallbacksByReason(const std::map<std::string, std::uint64_t>& reasons);
    void recordPacketHitRefinement(const std::string& materialLabel);
    void recordRadianceDeltaDepth(double squaredSum, double maxDelta);
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
