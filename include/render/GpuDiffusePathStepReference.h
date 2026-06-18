#pragma once

#include "render/GpuTracingScene.h"
#include "render/TracingAccumulationLayout.h"

#include <cstdint>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  class Tonemap;

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

    void merge(const GpuDiffusePathStepMetrics& source);
  };

  struct GpuDiffusePathStepResult {
    std::vector<GpuIntersectionHitRecord> closestHitRecords;
    // Compact next frontier: only surviving diffuse continuations are emitted.
    std::vector<GpuDiffusePathStateRecord> pathStates;
    std::vector<GpuDiffusePathStateRecord> terminatedPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuIntersectionRay> directLightShadowRays;
    std::vector<GpuIntersectionOcclusionRecord> directLightOcclusionRecords;
    GpuDiffusePathStepMetrics metrics;
  };

  struct GpuDiffusePathLoopSettings {
    std::uint32_t maxDepth{8};
  };

  struct GpuDiffusePathLoopResult {
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<std::uint64_t> activePathsPerDepth;
    GpuDiffusePathStepMetrics metrics;
    std::uint64_t initialPathCount{0};
    std::uint64_t depthCount{0};
    std::uint64_t maxDepthTerminatedPaths{0};
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

  class GpuDiffusePathLoop {
  public:
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const;
  };

  [[nodiscard]] TracingAccumulationDiagnostics
  resolveGpuDiffusePathLoopImage(const std::vector<GpuDiffusePathStateRecord>& records,
                                 const TracingAccumulationLayout& layout,
                                 Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);
  [[nodiscard]] TracingAccumulationDiagnostics
  resolveGpuDiffusePathLoopImage(const GpuDiffusePathLoopResult& result,
                                 const TracingAccumulationLayout& layout,
                                 Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);
}
