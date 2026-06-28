#pragma once

#include "render/GpuDiffusePathLoopLaunch.h"
#include "render/GpuDiffusePathStepReference.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
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
    std::vector<std::array<float, 4>> accumulationColorSums;
    std::vector<std::uint32_t> accumulationSampleCounts;
    std::vector<unsigned int> resolvedDisplayPixels;
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
  };

  [[nodiscard]] bool canChunkGpuDiffusePrimarySamples(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings);
  [[nodiscard]] std::vector<GpuDiffusePrimaryPathSampleChunk> gpuDiffusePrimarySampleChunksFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings);
  void mergePlatformGpuDiffusePathLoopChunkResult(GpuDiffusePathLoopPlatformResult& merged,
                                                  GpuDiffusePathLoopPlatformResult&& chunkResult);
  void notifyGpuDiffusePathLoopChunkProgress(
    const GpuDiffusePathLoopSettings& settings,
    const GpuDiffusePrimaryPathStateGeneration& fullPrimaryPathGeneration,
    const GpuDiffusePrimaryPathSampleChunk& chunk,
    const GpuDiffusePathLoopPlatformResult& chunkResult);
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
  [[nodiscard]] GpuDiffusePathLoopResult makePlatformGpuDiffusePathLoopResult(
    std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
    GpuDiffusePathLoopPlatformResult&& platformResult, const char* backendDisplayName,
    const char* platformName, const char* pathStateResidency, const char* accumulationBackend,
    const char* accumulationResidency);
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
