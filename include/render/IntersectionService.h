#pragma once

#include "core/math/Ray.h"
#include "render/WavefrontIntersectionBackend.h"

#include <memory>
#include <string>
#include <vector>

namespace render {
  class Scene;
  class State;
  class WavefrontAnyHitFrontier;

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
    closestHits(const std::vector<WavefrontClosestHitQuery>& queries);
    [[nodiscard]] bool anyHit(const Rayd& ray, double maxDistance, State& state);
    [[nodiscard]] WavefrontOcclusionFlags anyHits(const std::vector<WavefrontAnyHitQuery>& queries);
    [[nodiscard]] WavefrontOcclusionFlags anyHits(const WavefrontAnyHitFrontier& frontier);

  private:
    void refreshBackendDiagnostics();
    void applyRecordedQueryDiagnostics();
    void recordClosestHitTiming(const WavefrontIntersectionQueryTiming& timing);
    void recordAnyHitTiming(const WavefrontIntersectionQueryTiming& timing);
    void recordTiming(WavefrontIntersectionQueryTiming IntersectionServiceDiagnostics::* member,
                      const WavefrontIntersectionQueryTiming& timing);

    const Scene* m_scene;
    WavefrontIntersectionBackendChoice m_backendChoice;
    WavefrontIntersectionBackendSelectionContext m_selectionContext;
    std::shared_ptr<const WavefrontIntersectionBackend> m_backend;
    IntersectionServiceDiagnostics m_diagnostics;
  };
}
