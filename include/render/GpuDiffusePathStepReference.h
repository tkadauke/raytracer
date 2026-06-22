#pragma once

#include "core/math/Rect.h"
#include "render/GpuTracingScene.h"
#include "render/TracingAccumulationLayout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  class Camera;
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
    std::uint32_t russianRouletteDepth{3};
    std::uint32_t directLightSamples{1};
  };

  struct GpuDiffusePathLoopResult {
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<std::uint64_t> activePathsPerDepth;
    GpuDiffusePathStepMetrics metrics;
    std::string executionPath{"compiled_cpu_reference"};
    std::string pathStateResidency{"cpu_host"};
    std::uint64_t initialPathCount{0};
    std::uint64_t depthCount{0};
    std::uint64_t maxDepthTerminatedPaths{0};
    std::uint64_t retainedIndexBytes{0};
    std::uint64_t roundTrips{1};
    std::uint64_t savedHostReadbacks{0};
    std::uint64_t savedHostReadbackBytes{0};

    [[nodiscard]] std::uint64_t pathStateBytesPerPath() const;
    [[nodiscard]] std::uint64_t residentPathStateBytes() const;
    [[nodiscard]] std::uint64_t inputPathStateBytes() const;
    [[nodiscard]] std::uint64_t retainedPathStateBytes() const;
    [[nodiscard]] std::uint64_t removedPathStateBytes() const;
    [[nodiscard]] std::uint64_t retainedPathIndexBytes() const;
    [[nodiscard]] std::uint64_t compactionPassCount() const;
    [[nodiscard]] std::uint64_t inputPathCount() const;
    [[nodiscard]] std::uint64_t retainedPathCount() const;
    [[nodiscard]] std::uint64_t removedPathCount() const;
    [[nodiscard]] std::uint64_t movedPathCount() const;
    [[nodiscard]] std::uint64_t peakActivePathCount() const;
    [[nodiscard]] std::uint64_t lastActivePathCount() const;
    [[nodiscard]] std::uint64_t submittedIntersectionRayCount() const;
    [[nodiscard]] bool fullGpuPathLoopSupported() const;
    [[nodiscard]] bool fullGpuPathLoopUnavailable() const;
    [[nodiscard]] double removedPathFraction() const;
    [[nodiscard]] double movedRetainedPathFraction() const;
  };

  struct GpuDiffusePrimaryPathStateGeneration {
    std::vector<GpuDiffusePathStateRecord> pathStates;
    Recti requestedRect;
    Recti actualRect;
    std::uint64_t generatedPrimarySamples{0};
    std::uint64_t skippedPrimarySamples{0};
  };

  class GpuDiffusePrimaryPathStateGenerator {
  public:
    [[nodiscard]] GpuDiffusePrimaryPathStateGeneration
    generate(const Camera& camera, const Recti& rect,
             std::optional<std::uint64_t> tileSeed = std::nullopt,
             std::uint32_t sampleSeed = 0) const;
  };

  class GpuDiffusePathStep {
  public:
    [[nodiscard]] GpuDiffusePathStepResult
    step(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& pathStates,
         const GpuDiffusePathLoopSettings& settings = {}) const;
  };

  class GpuDiffusePathStepReference {
  public:
    [[nodiscard]] GpuDiffusePathStepResult
    step(const GpuTracingSceneSections& scene,
         const std::vector<GpuDiffusePathStateRecord>& pathStates,
         const std::vector<GpuIntersectionHitRecord>& closestHits,
         const GpuDiffusePathLoopSettings& settings = {}) const;
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
                                 const TracingAccumulationLayout& layout, Buffer<Colord>& target);
  [[nodiscard]] TracingAccumulationDiagnostics
  resolveGpuDiffusePathLoopImage(const GpuDiffusePathLoopResult& result,
                                 const TracingAccumulationLayout& layout, Buffer<Colord>& target);
  [[nodiscard]] TracingAccumulationDiagnostics
  resolveGpuDiffusePathLoopImage(const std::vector<GpuDiffusePathStateRecord>& records,
                                 const TracingAccumulationLayout& layout,
                                 Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);
  [[nodiscard]] TracingAccumulationDiagnostics
  resolveGpuDiffusePathLoopImage(const GpuDiffusePathLoopResult& result,
                                 const TracingAccumulationLayout& layout,
                                 Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);
}
