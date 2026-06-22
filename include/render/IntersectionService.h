#pragma once

#include "core/math/Ray.h"
#include "render/WavefrontIntersectionBackend.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace render {
  class Scene;
  class State;
  class WavefrontAnyHitFrontier;
  class WavefrontClosestHitFrontier;

  struct IntersectionServiceDiagnostics {
    std::string requestedBackend;
    std::string selectedBackend;
    std::string availability;
    std::string platformName;
    std::string executionPath;
    std::string closestHitExecutionPath;
    std::string anyHitExecutionPath;
    std::string fallbackReason;
    WavefrontIntersectionSceneDiagnostics scene;
    WavefrontIntersectionQueryTiming lastClosestHitTiming;
    WavefrontIntersectionQueryTiming lastAnyHitTiming;
    std::uint64_t closestHitQueryCount{0};
    std::uint64_t closestHitHitCount{0};
    std::uint64_t anyHitQueryCount{0};
    std::uint64_t anyHitOccludedCount{0};
    std::uint64_t closestHitRayUploadBytesEstimate{0};
    std::uint64_t closestHitReadbackBytesEstimate{0};
    std::uint64_t anyHitRayUploadBytesEstimate{0};
    std::uint64_t anyHitReadbackBytesEstimate{0};
    std::uint64_t queryTransferBytesEstimate{0};
    std::string closestHitFrontierResidency;
    std::string anyHitFrontierResidency;
    std::uint64_t closestHitFrontierPackedRayBytes{0};
    std::uint64_t closestHitFrontierHostQueryBytes{0};
    std::uint64_t closestHitFrontierStateHandleBytes{0};
    std::uint64_t anyHitFrontierPackedRayBytes{0};
    std::uint64_t anyHitFrontierHostQueryBytes{0};
    std::uint64_t anyHitFrontierStateHandleBytes{0};
  };

  /**
    * @brief Intersection-only entry point for closest-hit and any-hit work.
    *
    * This service intentionally exposes only ray-scene intersection queries.
    * It prepares the selected wavefront intersection backend for one scene and
    * keeps fallback/execution diagnostics visible without taking ownership of a
    * full renderer, integrator, path state, material evaluation, or
    * accumulation.
    */
  class IntersectionService {
  public:
    explicit IntersectionService(
      const Scene& scene,
      WavefrontIntersectionBackendChoice backendChoice = WavefrontIntersectionBackendChoice::cpu(),
      WavefrontIntersectionBackendSelectionContext selectionContext = {});
    explicit IntersectionService(const Scene& scene,
                                 const WavefrontIntersectionBackend& preparedBackend);

    [[nodiscard]] const Scene& scene() const;
    [[nodiscard]] const WavefrontIntersectionBackend& backend() const;
    [[nodiscard]] const IntersectionServiceDiagnostics& diagnostics() const;

    [[nodiscard]] WavefrontClosestHitResult closestHit(const Rayd& ray, State& state);
    [[nodiscard]] std::vector<WavefrontClosestHitResult>
    closestHits(std::vector<WavefrontClosestHitQuery> queries);
    [[nodiscard]] std::vector<WavefrontClosestHitResult>
    closestHits(const WavefrontClosestHitFrontier& frontier);
    [[nodiscard]] bool anyHit(const Rayd& ray, double maxDistance, State& state);
    [[nodiscard]] WavefrontOcclusionFlags anyHits(std::vector<WavefrontAnyHitQuery> queries);
    [[nodiscard]] WavefrontOcclusionFlags anyHits(const WavefrontAnyHitFrontier& frontier);
    [[nodiscard]] WavefrontOcclusionFlags
    resolveDirectLightVisibility(std::vector<WavefrontAnyHitQuery> queries);

  private:
    void refreshBackendDiagnostics();
    void applyRecordedQueryDiagnostics();
    void recordClosestHitTiming(const WavefrontIntersectionQueryTiming& timing);
    void recordAnyHitTiming(const WavefrontIntersectionQueryTiming& timing);
    void recordClosestHitWork(std::uint64_t queryCount, std::uint64_t hitCount,
                              const WavefrontIntersectionQueryTiming& timing);
    void recordAnyHitWork(std::uint64_t queryCount, std::uint64_t occludedCount,
                          const WavefrontIntersectionQueryTiming& timing);
    void recordClosestHitFrontier(const WavefrontClosestHitFrontier& frontier);
    void recordAnyHitFrontier(const WavefrontAnyHitFrontier& frontier);
    void recordTiming(WavefrontIntersectionQueryTiming IntersectionServiceDiagnostics::* member,
                      const WavefrontIntersectionQueryTiming& timing);

    const Scene* m_scene;
    WavefrontIntersectionBackendChoice m_backendChoice;
    WavefrontIntersectionBackendSelectionContext m_selectionContext;
    std::shared_ptr<const WavefrontIntersectionBackend> m_backend;
    IntersectionServiceDiagnostics m_diagnostics;
  };
}
