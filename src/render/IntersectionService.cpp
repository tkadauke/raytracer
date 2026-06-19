#include "render/IntersectionService.h"

#include "render/primitives/Scene.h"

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
    recordClosestHitTiming(timing);
    return result;
  }

  std::vector<WavefrontClosestHitResult>
  IntersectionService::closestHits(const std::vector<WavefrontClosestHitQuery>& queries) {
    WavefrontIntersectionQueryTiming timing;
    std::vector<WavefrontClosestHitResult> results =
      m_backend->intersectClosestBatch(*m_scene, queries, &timing);
    recordClosestHitTiming(timing);
    return results;
  }

  bool IntersectionService::anyHit(const Rayd& ray, double maxDistance, State& state) {
    WavefrontIntersectionQueryTiming timing;
    const bool occluded = m_backend->intersectAny(*m_scene, ray, maxDistance, state, &timing);
    recordAnyHitTiming(timing);
    return occluded;
  }

  WavefrontOcclusionFlags
  IntersectionService::anyHits(const std::vector<WavefrontAnyHitQuery>& queries) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontOcclusionFlags results = m_backend->intersectAnyBatch(*m_scene, queries, &timing);
    recordAnyHitTiming(timing);
    return results;
  }

  WavefrontOcclusionFlags IntersectionService::anyHits(const WavefrontAnyHitFrontier& frontier) {
    WavefrontIntersectionQueryTiming timing;
    WavefrontOcclusionFlags results = m_backend->intersectAnyFrontier(*m_scene, frontier, &timing);
    recordAnyHitTiming(timing);
    return results;
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

  void IntersectionService::recordTiming(
    WavefrontIntersectionQueryTiming IntersectionServiceDiagnostics::* member,
    const WavefrontIntersectionQueryTiming& timing) {
    refreshBackendDiagnostics();
    m_diagnostics.*member = timing;
    applyRecordedQueryDiagnostics();
  }
}
