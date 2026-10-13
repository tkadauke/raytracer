#pragma once

#include "render/GpuDiffusePathLoopLaunch.h"
#include "render/GpuDiffusePathStepReference.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace render {
  class GpuDiffusePathLoopBackend;

  struct GpuDiffusePathLoopBackendSupport {
    bool supported{false};
    std::string reason;
  };

  struct GpuDiffusePathLoopBackendChoice {
    std::shared_ptr<const GpuDiffusePathLoopBackend> backend;
    std::string fallbackReason;
  };

  struct GpuDiffusePathLoopPlatformAccumulationPlan {
    TracingAccumulationLayout layout;
    std::uint32_t targetMode{gpuDiffusePathLoopAccumulationTargetPixel};
  };

  struct GpuDiffusePathLoopPlatformResult {
    GpuDiffusePathLoopLaunchParameters echoedParameters;
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStateRecord> nextPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuDiffusePathDenoiserFeatureRecord> denoiserFeatureRecords;
    std::uint32_t retainedPathCount{0};
    std::vector<std::uint32_t> activePathCountsPerDepth;
    std::vector<double> radianceDeltaSquaredSumPerDepth;
    std::vector<double> maxRadianceDeltaPerDepth;
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
    std::vector<unsigned int> resolvedDisplayPixels;
    std::uint64_t resolvedDisplayReadbacks{0};
    std::string executionPath;
    std::string schedule{gpuDiffusePathLoopScheduleDepthFrontier};
    std::string pathStateResidency;
    bool retainedFrontierDispatchesIndirect{false};
    bool sceneUploadCacheHit{false};
    std::uint64_t sceneUploadBytesWritten{0};
    double uploadWorkerSeconds{0.0};
    double kernelWorkerSeconds{0.0};
    double readbackWorkerSeconds{0.0};
  };

  struct GpuDiffusePrimaryPathSampleChunk {
    GpuDiffusePrimaryPathStateGeneration primaryPathGeneration;
    bool firstChunk{false};
    bool finalChunk{false};
    bool completesSampleRange{false};
  };

  [[nodiscard]] bool canChunkGpuDiffusePrimarySamples(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings);
  [[nodiscard]] std::uint32_t resolvedGpuDiffusePrimarySampleChunkSize(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings);
  [[nodiscard]] std::vector<GpuDiffusePrimaryPathSampleChunk> gpuDiffusePrimarySampleChunksFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings);
  [[nodiscard]] bool shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(
    const GpuDiffusePathLoopSettings& settings, const GpuDiffusePrimaryPathSampleChunk& chunk);
  void mergePlatformGpuDiffusePathLoopChunkResult(GpuDiffusePathLoopPlatformResult& merged,
                                                  GpuDiffusePathLoopPlatformResult&& chunkResult);
  void notifyGpuDiffusePathLoopChunkProgress(
    const GpuDiffusePathLoopSettings& settings,
    const GpuDiffusePrimaryPathStateGeneration& fullPrimaryPathGeneration,
    const GpuDiffusePrimaryPathSampleChunk& chunk,
    const GpuDiffusePathLoopPlatformResult& chunkResult);
  [[nodiscard]] bool gpuDiffusePathLoopCancelled(const GpuDiffusePathLoopSettings& settings);
  void throwIfGpuDiffusePathLoopCancelled(const GpuDiffusePathLoopSettings& settings);
  [[nodiscard]] GpuDiffusePathLoopPlatformAccumulationPlan
  platformGpuDiffusePathLoopAccumulationPlanFor(
    const std::vector<GpuDiffusePathStateRecord>& pathStates, const char* backendDisplayName);
  [[nodiscard]] GpuDiffusePathLoopPlatformAccumulationPlan
  platformGpuDiffusePathLoopAccumulationPlanFor(const GpuPrimaryPathDescriptor& descriptor,
                                                const char* backendDisplayName);
  [[nodiscard]] GpuDiffusePathLoopPlatformAccumulationPlan
  platformGpuDiffusePathLoopAccumulationPlanFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const char* backendDisplayName);
  [[nodiscard]] GpuDiffusePathLoopPlatformAccumulationPlan
  platformGpuDiffusePathLoopAccumulationPlanFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings, const char* backendDisplayName);
  [[nodiscard]] GpuDiffusePathLoopResult makePlatformGpuDiffusePathLoopResult(
    std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
    GpuDiffusePathLoopPlatformResult&& platformResult, const char* backendDisplayName,
    const char* platformName, const char* pathStateResidency, const char* accumulationBackend,
    const char* accumulationResidency);

  /**
    * Converts a platform kernel result (any struct exposing the same field
    * names) into the shared platform result consumed by
    * makePlatformGpuDiffusePathLoopResult().
    */
  template<class KernelResult>
  [[nodiscard]] GpuDiffusePathLoopPlatformResult
  platformResultFromKernelResult(KernelResult&& result) {
    GpuDiffusePathLoopPlatformResult platform;
    platform.echoedParameters = result.echoedParameters;
    platform.resolvedPathStates = std::move(result.resolvedPathStates);
    platform.nextPathStates = std::move(result.nextPathStates);
    platform.stepRecords = std::move(result.stepRecords);
    platform.denoiserFeatureRecords = std::move(result.denoiserFeatureRecords);
    platform.retainedPathCount = result.retainedPathCount;
    platform.activePathCountsPerDepth = std::move(result.activePathCountsPerDepth);
    platform.radianceDeltaSquaredSumPerDepth = std::move(result.radianceDeltaSquaredSumPerDepth);
    platform.maxRadianceDeltaPerDepth = std::move(result.maxRadianceDeltaPerDepth);
    platform.accumulationColorSums = std::move(result.accumulationColorSums);
    platform.accumulationSampleCounts = std::move(result.accumulationSampleCounts);
    platform.resolvedDisplayPixels = std::move(result.resolvedDisplayPixels);
    platform.resolvedDisplayReadbacks = platform.resolvedDisplayPixels.empty() ? 0u : 1u;
    platform.executionPath = std::move(result.executionPath);
    platform.schedule = std::move(result.schedule);
    platform.pathStateResidency = std::move(result.pathStateResidency);
    platform.retainedFrontierDispatchesIndirect = result.retainedFrontierDispatchesIndirect;
    platform.sceneUploadCacheHit = result.sceneUploadCacheHit;
    platform.sceneUploadBytesWritten = result.sceneUploadBytesWritten;
    platform.uploadWorkerSeconds = result.uploadWorkerSeconds;
    platform.kernelWorkerSeconds = result.kernelWorkerSeconds;
    platform.readbackWorkerSeconds = result.readbackWorkerSeconds;
    return platform;
  }
  [[nodiscard]] GpuDiffusePathLoopBackendChoice selectFullGpuDiffusePathLoopBackend(
    const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>>& backends,
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings);

  /**
    * Backend boundary for the compiled diffuse path-loop subset.
    *
    * The default implementation is the CPU reference path loop. Platform
    * backends can implement this interface so the graph pass dispatches the
    * GPU-facing path-loop contract without constructing a specific backend.
    */
  class GpuDiffusePathLoopBackend {
  public:
    virtual ~GpuDiffusePathLoopBackend() = default;

    [[nodiscard]] static std::shared_ptr<const GpuDiffusePathLoopBackend>
    defaultBackendForGpuRequest();
    [[nodiscard]] static std::shared_ptr<const GpuDiffusePathLoopBackend>
    defaultFullGpuBackendForGpuRequest();
    [[nodiscard]] static GpuDiffusePathLoopBackendChoice
    defaultFullGpuBackendForGpuRequest(const GpuTracingSceneSections& scene,
                                       const GpuDiffusePathLoopSettings& settings);

    virtual const char* name() const = 0;
    [[nodiscard]] virtual bool fullGpuPathLoopAvailable() const;
    [[nodiscard]] virtual const char* fullGpuPathLoopUnavailableReason() const;
    [[nodiscard]] virtual const char* platformName() const;
    [[nodiscard]] virtual GpuDiffusePathLoopBackendSupport
    fullGpuPathLoopSupport(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] virtual GpuDiffusePathLoopBackendSupport
    fullGpuPathLoopSupport(const GpuTracingSceneSections& scene,
                           const GpuDiffusePathLoopSettings& settings) const;

    [[nodiscard]] virtual GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const = 0;
    [[nodiscard]] virtual GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
        const GpuDiffusePathLoopSettings& settings = {}) const;
  };

  class CpuReferenceGpuDiffusePathLoopBackend final : public GpuDiffusePathLoopBackend {
  public:
    [[nodiscard]] static std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend>
    sharedInstance();

    const char* name() const override;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const override;
  };

  class CompactingGpuDiffusePathLoopBackend final : public GpuDiffusePathLoopBackend {
  public:
    explicit CompactingGpuDiffusePathLoopBackend(
      std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> compactionBackend);

    const char* name() const override;
    [[nodiscard]] const GpuDiffusePathFrontierCompactionBackend& compactionBackend() const;
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings = {}) const override;

  private:
    std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> m_compactionBackend;
  };
}
