#pragma once

#include "render/GpuTracingScene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace render {
  struct GpuDiffusePathStepMetrics {
    std::string closestHitExecutionPath;
    std::string emissionExecutionPath;
    std::string directLightVisibilityExecutionPath;
    std::string directLightContributionExecutionPath;
    std::uint64_t activePaths{0};
    std::uint64_t closestHitRays{0};
    std::uint64_t misses{0};
    std::uint64_t hits{0};
    std::uint64_t unsupportedHits{0};
    std::uint64_t emissiveHits{0};
    std::uint64_t emissionContributionEvaluations{0};
    std::uint64_t directLightSamples{0};
    std::uint64_t directLightVisibilityRays{0};
    std::uint64_t directLightContributionEvaluations{0};
    std::uint64_t directLightContributingSamples{0};
    std::uint64_t directLightOccludedSamples{0};
    std::uint64_t spawnedContinuations{0};
    std::uint64_t terminatedPaths{0};
  };

  struct GpuDiffusePathStepResult {
    std::vector<GpuIntersectionHitRecord> closestHitRecords;
    // Compact next frontier: only surviving diffuse continuations are emitted.
    std::vector<GpuDiffusePathStateRecord> pathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuIntersectionRay> directLightShadowRays;
    std::vector<GpuIntersectionOcclusionRecord> directLightOcclusionRecords;
    GpuDiffusePathStepMetrics metrics;
  };

  class GpuDiffusePathStep {
  public:
    [[nodiscard]] GpuDiffusePathStepResult
    step(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& pathStates) const;
  };

  class GpuDiffusePathStepReference {
  public:
    [[nodiscard]] GpuDiffusePathStepResult
    step(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& pathStates,
         const std::vector<GpuIntersectionHitRecord>& closestHits) const;
  };
}
