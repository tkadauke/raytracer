#pragma once

#include "core/math/Rect.h"
#include "render/GpuPrimaryPathDescriptor.h"
#include "render/GpuTracingScene.h"
#include "render/TracingAccumulationLayout.h"
#include "render/TracingExecutionCapability.h"
#include "render/tonemap/Tonemap.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  class Camera;

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

  struct alignas(16) GpuDiffusePathDenoiserFeatureRecord {
    std::uint32_t pixelIndex{0};
    std::uint32_t primarySampleIndex{0};
    std::uint32_t flags{0};
    std::uint32_t reserved{0};
    std::array<float, 4> albedo{};
    std::array<float, 4> normal{};
    float depth{0.0f};
    std::array<float, 3> reservedDepth{};
  };

  inline constexpr std::uint32_t gpuDiffusePathDenoiserFeatureValidFlag = 1u << 0u;

  struct GpuDiffusePathStepResult {
    std::vector<GpuIntersectionHitRecord> closestHitRecords;
    // Compact next frontier: only surviving diffuse continuations are emitted.
    std::vector<GpuDiffusePathStateRecord> pathStates;
    std::vector<GpuDiffusePathStateRecord> terminatedPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuDiffusePathDenoiserFeatureRecord> denoiserFeatureRecords;
    std::vector<GpuIntersectionRay> directLightShadowRays;
    std::vector<GpuIntersectionOcclusionRecord> directLightOcclusionRecords;
    GpuDiffusePathStepMetrics metrics;
    double radianceDeltaSquaredSum{0.0};
    double maxRadianceDelta{0.0};
  };

  struct GpuDiffusePathFrontierCompactionResult {
    std::vector<GpuDiffusePathStateRecord> retainedRecords;
    std::vector<std::uint32_t> retainedPathIndices;
    std::string executionPath{"cpu_diffuse_frontier_compaction"};
    std::string pathStateResidency{"cpu_host"};
    std::uint64_t inputPathCount{0};
    double uploadWorkerSeconds{0.0};
    double kernelWorkerSeconds{0.0};
    double readbackWorkerSeconds{0.0};

    [[nodiscard]] std::uint64_t retainedPathCount() const;
    [[nodiscard]] std::uint64_t removedPathCount() const;
    [[nodiscard]] std::uint64_t movedPathCount() const;
    [[nodiscard]] std::uint64_t retainedIndexBytes() const;
  };

  class GpuDiffusePathFrontierCompactionBackend {
  public:
    virtual ~GpuDiffusePathFrontierCompactionBackend() = default;

    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual const char* pathStateResidency() const = 0;
    [[nodiscard]] virtual GpuDiffusePathFrontierCompactionResult
    compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const = 0;
  };

  class CpuReferenceGpuDiffusePathFrontierCompactionBackend final
      : public GpuDiffusePathFrontierCompactionBackend {
  public:
    [[nodiscard]] static const CpuReferenceGpuDiffusePathFrontierCompactionBackend& instance();

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] const char* pathStateResidency() const override;
    [[nodiscard]] GpuDiffusePathFrontierCompactionResult
    compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const override;
  };

  struct GpuDiffusePathLoopChunkProgress {
    std::uint32_t sampleOffset{0};
    std::uint32_t sampleCount{0};
    std::uint32_t totalSampleCount{0};
    std::uint32_t completedSampleCount{0};
    bool firstChunk{false};
    bool finalChunk{false};
    const std::vector<unsigned int>* resolvedDisplayPixels{nullptr};
  };

  using GpuDiffusePathLoopChunkProgressObserver =
    std::function<void(const GpuDiffusePathLoopChunkProgress&)>;
  using GpuDiffusePathLoopCancellationCallback = std::function<bool()>;

  struct GpuDiffusePathLoopSettings {
    std::uint32_t maxDepth{8};
    std::uint32_t frontierDispatchDepthLimit{0};
    std::uint32_t russianRouletteDepth{3};
    std::uint32_t directLightSamples{1};
    bool captureDiagnostics{true};
    bool captureMetrics{true};
    bool captureDenoiserFeatures{false};
    bool capturePlatformAccumulation{true};
    bool captureResolvedDisplay{false};
    bool interactiveDisplay{false};
    bool convergenceEnabled{false};
    double convergenceActiveSampleFractionThreshold{0.0};
    double convergenceRadianceDeltaRmsThreshold{0.0};
    GpuDisplayResolveTonemap displayResolveTonemap{GpuDisplayResolveTonemap::Linear};
    std::uint32_t primarySampleChunkSize{0};
    GpuDiffusePathLoopChunkProgressObserver chunkProgressObserver;
    GpuDiffusePathLoopCancellationCallback cancellationCallback;
  };

  [[nodiscard]] bool gpuDiffusePathLoopReachedConvergence(
    std::uint64_t activePathCount, std::uint64_t retainedPathCount, std::uint64_t totalPathCount,
    double radianceDeltaSquaredSum, const GpuDiffusePathLoopSettings& settings);

  inline constexpr std::uint32_t gpuDiffusePathLoopAccumulationTargetPixel = 0u;
  inline constexpr std::uint32_t gpuDiffusePathLoopAccumulationTargetPath = 1u;
  inline constexpr std::uint32_t gpuDiffusePathLoopAccumulationTargetSampleSlot = 2u;
  inline constexpr const char* gpuDiffusePathLoopScheduleDepthFrontier = "depth_frontier";

  struct GpuDiffusePathLoopResult {
    std::vector<GpuDiffusePathStateRecord> resolvedPathStates;
    std::vector<GpuDiffusePathStateRecord> retainedFrontierPathStates;
    std::vector<GpuDiffusePathStepRecord> stepRecords;
    std::vector<GpuDiffusePathDenoiserFeatureRecord> denoiserFeatureRecords;
    bool denoiserFeatureRecordsCaptured{false};
    std::vector<std::uint64_t> activePathsPerDepth;
    GpuDiffusePathStepMetrics metrics;
    std::string executionPath{"compiled_cpu_reference"};
    std::string schedule{gpuDiffusePathLoopScheduleDepthFrontier};
    std::string pathStateResidency{"cpu_host"};
    std::string frontierCompactionExecutionPath{"cpu_diffuse_frontier_compaction"};
    std::string frontierCompactionPathStateResidency{"cpu_host"};
    bool retainedFrontierDispatchesIndirect{false};
    std::string platformName;
    std::uint64_t initialPathCount{0};
    std::uint64_t depthCount{0};
    std::uint64_t maxDepthTerminatedPaths{0};
    bool convergenceEnabled{false};
    double convergenceActiveSampleFractionThreshold{0.0};
    double convergenceRadianceDeltaRmsThreshold{0.0};
    bool stoppedByConvergence{false};
    std::uint64_t stoppedAfterDepth{0};
    std::vector<double> radianceDeltaSquaredSumPerDepth;
    std::vector<double> maxRadianceDeltaPerDepth;
    std::uint64_t retainedIndexBytes{0};
    std::uint64_t roundTrips{1};
    std::uint64_t savedHostReadbacks{0};
    std::uint64_t savedHostReadbackBytes{0};
    bool platformSceneUploadCacheHit{false};
    std::uint64_t platformSceneUploadBytesWritten{0};
    double frontierCompactionUploadWorkerSeconds{0.0};
    double frontierCompactionKernelWorkerSeconds{0.0};
    double frontierCompactionReadbackWorkerSeconds{0.0};
    std::vector<std::array<float, 4>> platformAccumulationColorSums;
    std::vector<std::uint32_t> platformAccumulationSampleCounts;
    std::vector<unsigned int> platformResolvedDisplayPixels;
    std::uint64_t platformResolvedDisplayReadbacks{0};
    std::uint64_t platformAccumulationAddedSamples{0};
    std::string platformAccumulationBackend;
    std::string platformAccumulationResidency;
    std::uint32_t platformAccumulationTargetMode{gpuDiffusePathLoopAccumulationTargetPixel};
    std::uint32_t platformAccumulationWidth{0};
    std::uint32_t platformAccumulationHeight{0};

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
    [[nodiscard]] bool hasPlatformAccumulation() const;
    [[nodiscard]] bool hasPlatformResolvedDisplay() const;
    [[nodiscard]] std::string platformLabel() const;
    [[nodiscard]] TracingExecutionCapabilityRecords
    tracingCapabilities(const TracingAccumulationDiagnostics& accumulation) const;
    [[nodiscard]] double removedPathFraction() const;
    [[nodiscard]] double movedRetainedPathFraction() const;
  };

  struct GpuDiffusePrimaryPathStateGeneration {
    std::vector<GpuDiffusePathStateRecord> pathStates;
    std::optional<GpuPrimaryPathDescriptor> primaryPathDescriptor;
    std::string primaryPathExecutionPath{"cpu_camera_primary_ray_generator"};
    Recti requestedRect;
    Recti actualRect;
    std::uint64_t generatedPrimarySamples{0};
    std::uint64_t skippedPrimarySamples{0};

    [[nodiscard]] bool canGeneratePrimaryPathsOnDevice() const;
  };

  struct GpuDiffusePrimaryPathStateGenerationOptions {
    bool materializeHostPathStates{true};
    bool forceHostPrimaryRayGenerator{false};
    std::uint32_t sampleOffset{0};
    std::optional<std::uint32_t> sampleCount;
  };

  class GpuDiffusePrimaryPathStateGenerator {
  public:
    [[nodiscard]] GpuDiffusePrimaryPathStateGeneration
    generate(const Camera& camera, const Recti& rect,
             std::optional<std::uint64_t> tileSeed = std::nullopt, std::uint32_t sampleSeed = 0,
             GpuDiffusePrimaryPathStateGenerationOptions options = {}) const;
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
    [[nodiscard]] GpuDiffusePathLoopResult
    run(const GpuTracingSceneSections& scene,
        const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
        const GpuDiffusePathLoopSettings& settings,
        const GpuDiffusePathFrontierCompactionBackend& compactionBackend) const;
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
