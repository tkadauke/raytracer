#include "render/IntersectionService.h"

#include "render/primitives/Scene.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace render {
  namespace {
    std::string nonNullString(const char* value) {
      return value ? value : "";
    }

    void mergeLabel(std::string& target, const std::string& label) {
      if (label.empty()) {
        return;
      }
      if (target.empty() || target == label) {
        target = label;
        return;
      }
      target = "mixed";
    }

    void validateResultCount(std::size_t actual, std::uint64_t expected, const char* message) {
      if (static_cast<std::uint64_t>(actual) != expected) {
        throw std::logic_error(message);
      }
    }

    std::uint64_t saturatedAdd(std::uint64_t lhs, std::uint64_t rhs) {
      if (std::numeric_limits<std::uint64_t>::max() - lhs < rhs) {
        return std::numeric_limits<std::uint64_t>::max();
      }
      return lhs + rhs;
    }

    std::uint64_t countClosestHits(const std::vector<WavefrontClosestHitResult>& results) {
      std::uint64_t count = 0;
      for (const WavefrontClosestHitResult& result : results) {
        if (result.hit()) {
          ++count;
        }
      }
      return count;
    }

    std::uint64_t countOccludedResults(const WavefrontOcclusionFlags& results) {
      std::uint64_t count = 0;
      for (const unsigned char result : results) {
        if (result != 0) {
          ++count;
        }
      }
      return count;
    }
  }

  IntersectionService::IntersectionService(
    const Scene& scene, WavefrontIntersectionBackendChoice backendChoice,
    WavefrontIntersectionBackendSelectionContext selectionContext)
      : m_scene(&scene),
        m_backendChoice(std::move(backendChoice)),
        m_selectionContext(selectionContext),
        m_backend(m_backendChoice.createBackendForScene(scene, m_selectionContext)) {
    refreshBackendDiagnostics();
  }

  IntersectionService::IntersectionService(const Scene& scene,
                                           const WavefrontIntersectionBackend& preparedBackend)
      : m_scene(&scene),
        m_backendChoice(WavefrontIntersectionBackendChoice::cpu()),
        m_backend(&preparedBackend, [](const WavefrontIntersectionBackend*) {}) {
    refreshBackendDiagnostics();
  }

  const Scene& IntersectionService::scene() const {
    return *m_scene;
  }

  const WavefrontIntersectionBackend& IntersectionService::backend() const {
    return *m_backend;
  }

  const IntersectionServiceDiagnostics& IntersectionService::diagnostics() const {
    return m_diagnostics;
  }

  WavefrontClosestHitResult IntersectionService::closestHit(const Rayd& ray, State& state) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontClosestHitResult result =
      m_backend->intersectClosestResult(*m_scene, ray, state, &timing);
    recordClosestHitWork(1, result.hit() ? 1 : 0, timing);
    return result;
  }

  std::vector<WavefrontClosestHitResult>
  IntersectionService::closestHits(std::vector<WavefrontClosestHitQuery> queries) {
    auto frontier = m_backend->createClosestHitFrontier(std::move(queries));
    if (!frontier) {
      throw std::logic_error(
        "IntersectionService closest-hit batch did not create a closest-hit frontier");
    }
    return closestHits(*frontier);
  }

  std::vector<WavefrontClosestHitResult>
  IntersectionService::closestHits(const WavefrontClosestHitFrontier& frontier) {
    WavefrontIntersectionQueryTiming timing;
    std::vector<WavefrontClosestHitResult> results =
      m_backend->intersectClosestFrontier(*m_scene, frontier, &timing);
    validateResultCount(results.size(), frontier.rayCount(),
                        "IntersectionService closest-hit frontier returned a result count that "
                        "does not match its ray count");
    recordClosestHitFrontierQuery(frontier, countClosestHits(results), timing);
    return results;
  }

  bool IntersectionService::anyHit(const Rayd& ray, double maxDistance, State& state) {
    WavefrontIntersectionQueryTiming timing;
    const bool occluded = m_backend->intersectAny(*m_scene, ray, maxDistance, state, &timing);
    recordAnyHitWork(1, occluded ? 1 : 0, timing);
    return occluded;
  }

  WavefrontOcclusionFlags IntersectionService::anyHits(std::vector<WavefrontAnyHitQuery> queries) {
    auto frontier = m_backend->createAnyHitFrontier(std::move(queries));
    if (!frontier) {
      throw std::logic_error(
        "IntersectionService any-hit batch did not create an any-hit frontier");
    }
    return anyHits(*frontier);
  }

  WavefrontOcclusionFlags IntersectionService::anyHits(const WavefrontAnyHitFrontier& frontier) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontOcclusionFlags results = m_backend->intersectAnyFrontier(*m_scene, frontier, &timing);
    validateResultCount(results.size(), frontier.rayCount(),
                        "IntersectionService any-hit frontier returned an occlusion count that "
                        "does not match its ray count");
    recordAnyHitFrontierQuery(frontier, countOccludedResults(results), timing);
    return results;
  }

  WavefrontOcclusionFlags
  IntersectionService::resolveDirectLightVisibility(std::vector<WavefrontAnyHitQuery> queries) {
    WavefrontDirectLightVisibilityBatchResult result =
      m_backend->resolveDirectLightVisibilityBatch(*m_scene, std::move(queries));
    if (!result.frontier) {
      throw std::logic_error(
        "IntersectionService direct-light visibility batch resolved without an any-hit frontier");
    }
    validateResultCount(result.occluded.size(), result.frontier->rayCount(),
                        "IntersectionService direct-light visibility batch returned an occlusion "
                        "count that does not match its ray count");
    recordAnyHitFrontierQuery(*result.frontier, countOccludedResults(result.occluded),
                              result.timing);
    return result.occluded;
  }

  void IntersectionService::refreshBackendDiagnostics() {
    m_diagnostics.requestedBackend = nonNullString(m_backend->requestedName());
    m_diagnostics.selectedBackend = nonNullString(m_backend->name());
    m_diagnostics.availability = nonNullString(m_backend->availability());
    m_diagnostics.platformName = nonNullString(m_backend->platformName());
    m_diagnostics.executionPath = nonNullString(m_backend->executionPath());
    m_diagnostics.closestHitExecutionPath = nonNullString(m_backend->closestHitExecutionPath());
    m_diagnostics.anyHitExecutionPath = nonNullString(m_backend->anyHitExecutionPath());
    m_diagnostics.fallbackReason = nonNullString(m_backend->fallbackReason());
    m_diagnostics.scene = m_backend->compiledSceneDiagnostics();
    applyRecordedQueryDiagnostics();
  }

  void IntersectionService::applyRecordedQueryDiagnostics() {
    const bool hasClosestTiming = !m_diagnostics.lastClosestHitTiming.executionPath.empty();
    const bool hasAnyTiming = !m_diagnostics.lastAnyHitTiming.executionPath.empty();

    if (hasClosestTiming) {
      m_diagnostics.closestHitExecutionPath = m_diagnostics.lastClosestHitTiming.executionPath;
    }
    if (hasAnyTiming) {
      m_diagnostics.anyHitExecutionPath = m_diagnostics.lastAnyHitTiming.executionPath;
    }
    if (hasClosestTiming || hasAnyTiming) {
      std::string observedExecutionPath;
      mergeLabel(observedExecutionPath, m_diagnostics.lastClosestHitTiming.executionPath);
      mergeLabel(observedExecutionPath, m_diagnostics.lastAnyHitTiming.executionPath);
      m_diagnostics.executionPath = observedExecutionPath;
    }

    std::string observedFallbackReason = m_diagnostics.fallbackReason;
    mergeLabel(observedFallbackReason, m_diagnostics.lastClosestHitTiming.fallbackReason);
    mergeLabel(observedFallbackReason, m_diagnostics.lastAnyHitTiming.fallbackReason);
    m_diagnostics.fallbackReason = observedFallbackReason;
  }

  void IntersectionService::recordClosestHitTiming(const WavefrontIntersectionQueryTiming& timing) {
    recordTiming(&IntersectionServiceDiagnostics::lastClosestHitTiming, timing);
  }

  void IntersectionService::recordAnyHitTiming(const WavefrontIntersectionQueryTiming& timing) {
    recordTiming(&IntersectionServiceDiagnostics::lastAnyHitTiming, timing);
  }

  void IntersectionService::recordClosestHitWork(std::uint64_t queryCount, std::uint64_t hitCount,
                                                 const WavefrontIntersectionQueryTiming& timing) {
    recordClosestHitTiming(timing);
    const std::uint64_t uploadBytes = m_backend->estimatedClosestHitRayUploadBytes(queryCount);
    const std::uint64_t readbackBytes = m_backend->estimatedClosestHitReadbackBytes(queryCount);
    m_diagnostics.closestHitQueryCount =
      saturatedAdd(m_diagnostics.closestHitQueryCount, queryCount);
    m_diagnostics.closestHitHitCount = saturatedAdd(m_diagnostics.closestHitHitCount, hitCount);
    m_diagnostics.closestHitRayUploadBytesEstimate =
      saturatedAdd(m_diagnostics.closestHitRayUploadBytesEstimate, uploadBytes);
    m_diagnostics.closestHitReadbackBytesEstimate =
      saturatedAdd(m_diagnostics.closestHitReadbackBytesEstimate, readbackBytes);
    m_diagnostics.queryTransferBytesEstimate = saturatedAdd(
      m_diagnostics.queryTransferBytesEstimate, saturatedAdd(uploadBytes, readbackBytes));
  }

  void IntersectionService::recordAnyHitWork(std::uint64_t queryCount, std::uint64_t occludedCount,
                                             const WavefrontIntersectionQueryTiming& timing) {
    recordAnyHitTiming(timing);
    const std::uint64_t uploadBytes = m_backend->estimatedAnyHitRayUploadBytes(queryCount);
    const std::uint64_t readbackBytes = m_backend->estimatedAnyHitReadbackBytes(queryCount);
    m_diagnostics.anyHitQueryCount = saturatedAdd(m_diagnostics.anyHitQueryCount, queryCount);
    m_diagnostics.anyHitOccludedCount =
      saturatedAdd(m_diagnostics.anyHitOccludedCount, occludedCount);
    m_diagnostics.anyHitRayUploadBytesEstimate =
      saturatedAdd(m_diagnostics.anyHitRayUploadBytesEstimate, uploadBytes);
    m_diagnostics.anyHitReadbackBytesEstimate =
      saturatedAdd(m_diagnostics.anyHitReadbackBytesEstimate, readbackBytes);
    m_diagnostics.queryTransferBytesEstimate = saturatedAdd(
      m_diagnostics.queryTransferBytesEstimate, saturatedAdd(uploadBytes, readbackBytes));
  }

  void IntersectionService::recordClosestHitFrontier(const WavefrontClosestHitFrontier& frontier) {
    mergeLabel(m_diagnostics.closestHitFrontierResidency, nonNullString(frontier.residency()));
    m_diagnostics.closestHitFrontierPackedRayBytes =
      saturatedAdd(m_diagnostics.closestHitFrontierPackedRayBytes, frontier.packedRayBytes());
    m_diagnostics.closestHitFrontierHostQueryBytes =
      saturatedAdd(m_diagnostics.closestHitFrontierHostQueryBytes, frontier.hostQueryBytes());
    m_diagnostics.closestHitFrontierStateHandleBytes =
      saturatedAdd(m_diagnostics.closestHitFrontierStateHandleBytes, frontier.stateHandleBytes());
  }

  void IntersectionService::recordAnyHitFrontier(const WavefrontAnyHitFrontier& frontier) {
    mergeLabel(m_diagnostics.anyHitFrontierResidency, nonNullString(frontier.residency()));
    m_diagnostics.anyHitFrontierPackedRayBytes =
      saturatedAdd(m_diagnostics.anyHitFrontierPackedRayBytes, frontier.packedRayBytes());
    m_diagnostics.anyHitFrontierHostQueryBytes =
      saturatedAdd(m_diagnostics.anyHitFrontierHostQueryBytes, frontier.hostQueryBytes());
    m_diagnostics.anyHitFrontierStateHandleBytes =
      saturatedAdd(m_diagnostics.anyHitFrontierStateHandleBytes, frontier.stateHandleBytes());
  }

  void IntersectionService::recordClosestHitFrontierQuery(
    const WavefrontClosestHitFrontier& frontier, std::uint64_t hitCount,
    const WavefrontIntersectionQueryTiming& timing) {
    recordClosestHitFrontier(frontier);
    recordClosestHitWork(frontier.rayCount(), hitCount, timing);
  }

  void
  IntersectionService::recordAnyHitFrontierQuery(const WavefrontAnyHitFrontier& frontier,
                                                 std::uint64_t occludedCount,
                                                 const WavefrontIntersectionQueryTiming& timing) {
    recordAnyHitFrontier(frontier);
    recordAnyHitWork(frontier.rayCount(), occludedCount, timing);
  }

  void IntersectionService::recordTiming(
    WavefrontIntersectionQueryTiming IntersectionServiceDiagnostics::* member,
    const WavefrontIntersectionQueryTiming& timing) {
    refreshBackendDiagnostics();
    m_diagnostics.*member = timing;
    applyRecordedQueryDiagnostics();
  }
}
