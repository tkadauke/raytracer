#include "render/VulkanGpuDiffusePathLoopBackend.h"

#include "render/GpuDiffusePathLoopSceneSupport.h"
#include "render/GpuFloat4.h"
#include "render/TracingAccumulationLayout.h"
#include "render/VulkanGpuDiffusePathLoopKernel.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kVulkanPathStateResidency = "vulkan_host_visible_diffuse_path_state";

    struct ActiveAccumulationTargetShape {
      std::uint64_t pixelCount{1};
      std::uint64_t sampleSlotCount{1};
      std::uint64_t activePathCount{0};
    };

    [[nodiscard]] ActiveAccumulationTargetShape
    activeAccumulationTargetShapeFor(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      std::uint64_t maxPixel = 0;
      std::uint64_t maxSampleSlot = 0;
      std::uint64_t activePathCount = 0;
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        ++activePathCount;
        maxPixel = std::max<std::uint64_t>(maxPixel, path.pixelIndex);
        maxSampleSlot = std::max<std::uint64_t>(maxSampleSlot, path.primarySampleIndex);
      }
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation sample index exceeds layout range");
      }
      return {activePathCount == 0u ? 1u : maxPixel + 1u,
              activePathCount == 0u ? 1u : maxSampleSlot + 1u, activePathCount};
    }

    [[nodiscard]] ActiveAccumulationTargetShape
    activeAccumulationTargetShapeFor(const GpuPrimaryPathDescriptor& descriptor) {
      if (descriptor.mode != gpuPrimaryPathGenerationModePinhole &&
          descriptor.mode != gpuPrimaryPathGenerationModeOrthographic &&
          descriptor.mode != gpuPrimaryPathGenerationModeThinLens &&
          descriptor.mode != gpuPrimaryPathGenerationModeEquirectangular &&
          descriptor.mode != gpuPrimaryPathGenerationModeSpherical &&
          descriptor.mode != gpuPrimaryPathGenerationModeFishEye &&
          descriptor.mode != gpuPrimaryPathGenerationModeTiltShift) {
        throw std::invalid_argument(
          "Vulkan diffuse path-loop backend requires a supported primary path descriptor");
      }
      const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor.rectilinear;
      if (rectilinear.actualWidth == 0u || rectilinear.actualHeight == 0u ||
          rectilinear.samplesPerPixel == 0u) {
        return {};
      }
      const std::int64_t maxColumnOffset = static_cast<std::int64_t>(rectilinear.actualLeft) -
                                           static_cast<std::int64_t>(rectilinear.requestedLeft) +
                                           static_cast<std::int64_t>(rectilinear.actualWidth) - 1;
      const std::int64_t maxRowOffset = static_cast<std::int64_t>(rectilinear.actualTop) -
                                        static_cast<std::int64_t>(rectilinear.requestedTop) +
                                        static_cast<std::int64_t>(rectilinear.actualHeight) - 1;
      if (maxColumnOffset < 0 || maxRowOffset < 0) {
        throw std::invalid_argument(
          "Vulkan diffuse path-loop primary descriptor is outside request");
      }
      const std::uint64_t maxPixel =
        static_cast<std::uint64_t>(maxRowOffset) * rectilinear.requestedWidth +
        static_cast<std::uint64_t>(maxColumnOffset);
      const std::uint64_t maxSampleSlot = rectilinear.samplesPerPixel - 1u;
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation sample index exceeds layout range");
      }
      return {maxPixel + 1u, maxSampleSlot + 1u, descriptor.pathCount()};
    }

    [[nodiscard]] bool
    hasDuplicateActivePixelTarget(const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                  const ActiveAccumulationTargetShape& shape) {
      std::vector<bool> seen(static_cast<std::size_t>(shape.pixelCount), false);
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        if (seen[path.pixelIndex]) {
          return true;
        }
        seen[path.pixelIndex] = true;
      }
      return false;
    }

    [[nodiscard]] bool
    hasDuplicateActiveSampleSlot(const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                 const ActiveAccumulationTargetShape& shape) {
      if (shape.pixelCount > std::numeric_limits<std::uint64_t>::max() / shape.sampleSlotCount) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend sample-slot accumulation shape overflows");
      }
      const std::uint64_t slotCount = shape.pixelCount * shape.sampleSlotCount;
      if (slotCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend sample-slot accumulation shape exceeds host range");
      }
      std::vector<bool> seen(static_cast<std::size_t>(slotCount), false);
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        const std::uint64_t slot =
          static_cast<std::uint64_t>(path.pixelIndex) * shape.sampleSlotCount +
          static_cast<std::uint64_t>(path.primarySampleIndex);
        if (seen[slot]) {
          return true;
        }
        seen[slot] = true;
      }
      return false;
    }

    [[nodiscard]] TracingAccumulationLayout
    pixelAccumulationLayoutFor(const ActiveAccumulationTargetShape& shape) {
      return TracingAccumulationLayout::image(static_cast<int>(shape.pixelCount), 1);
    }

    [[nodiscard]] TracingAccumulationLayout
    sampleSlotAccumulationLayoutFor(const ActiveAccumulationTargetShape& shape) {
      return TracingAccumulationLayout::image(static_cast<int>(shape.pixelCount),
                                              static_cast<int>(shape.sampleSlotCount));
    }

    [[nodiscard]] TracingAccumulationLayout
    pathAccumulationLayoutFor(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      if (pathStates.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend path accumulation index exceeds layout range");
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::size_t>(1u, pathStates.size())), 1);
    }

    [[nodiscard]] TracingAccumulationLayout
    pathAccumulationLayoutFor(const GpuPrimaryPathDescriptor& descriptor) {
      const std::uint64_t pathCount = descriptor.pathCount();
      if (pathCount >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend path accumulation index exceeds layout range");
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::uint64_t>(1u, pathCount)), 1);
    }

    struct VulkanAccumulationPlan {
      TracingAccumulationLayout layout;
      std::uint32_t targetMode{gpuDiffusePathLoopAccumulationTargetPixel};
    };

    [[nodiscard]] VulkanAccumulationPlan
    accumulationPlanFor(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      const ActiveAccumulationTargetShape shape = activeAccumulationTargetShapeFor(pathStates);
      if (!hasDuplicateActivePixelTarget(pathStates, shape)) {
        return {pixelAccumulationLayoutFor(shape), gpuDiffusePathLoopAccumulationTargetPixel};
      }

      const TracingAccumulationLayout pathLayout = pathAccumulationLayoutFor(pathStates);
      const TracingAccumulationLayout sampleSlotLayout = sampleSlotAccumulationLayoutFor(shape);
      if (!hasDuplicateActiveSampleSlot(pathStates, shape) &&
          sampleSlotLayout.pixelCount() <= pathLayout.pixelCount()) {
        return {sampleSlotLayout, gpuDiffusePathLoopAccumulationTargetSampleSlot};
      }
      return {pathLayout, gpuDiffusePathLoopAccumulationTargetPath};
    }

    [[nodiscard]] VulkanAccumulationPlan
    accumulationPlanFor(const GpuPrimaryPathDescriptor& descriptor) {
      const ActiveAccumulationTargetShape shape = activeAccumulationTargetShapeFor(descriptor);
      const bool hasDuplicatePixelTargets =
        descriptor.generatesOnDevice() && descriptor.rectilinear.samplesPerPixel > 1u;
      if (!hasDuplicatePixelTargets) {
        return {pixelAccumulationLayoutFor(shape), gpuDiffusePathLoopAccumulationTargetPixel};
      }

      const TracingAccumulationLayout pathLayout = pathAccumulationLayoutFor(descriptor);
      const TracingAccumulationLayout sampleSlotLayout = sampleSlotAccumulationLayoutFor(shape);
      if (sampleSlotLayout.pixelCount() <= pathLayout.pixelCount()) {
        return {sampleSlotLayout, gpuDiffusePathLoopAccumulationTargetSampleSlot};
      }
      return {pathLayout, gpuDiffusePathLoopAccumulationTargetPath};
    }

    [[nodiscard]] VulkanAccumulationPlan
    accumulationPlanFor(const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration) {
      if (primaryPathGeneration.canGeneratePrimaryPathsOnDevice() &&
          primaryPathGeneration.pathStates.empty()) {
        return accumulationPlanFor(*primaryPathGeneration.primaryPathDescriptor);
      }
      return accumulationPlanFor(primaryPathGeneration.pathStates);
    }

    void terminate(GpuDiffusePathStateRecord& path) {
      path.flags &= ~gpuDiffusePathStateActiveFlag;
      path.flags |= gpuDiffusePathStateTerminatedFlag;
    }

    [[nodiscard]] bool stepHasContinuation(const GpuDiffusePathStepRecord& step) {
      return gpuFloat4HasValue(step.continuationThroughput);
    }

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const VulkanGpuDiffusePathLoopKernelResult& vulkanResult,
                          const GpuDiffusePathLoopSettings& settings) {
      std::uint64_t activeSteps = 0;
      std::uint64_t countedActiveSteps = 0;
      for (const std::uint32_t count : vulkanResult.activePathCountsPerDepth) {
        countedActiveSteps += count;
      }
      loop.metrics.closestHitExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.emissionExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightVisibilityExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightContributionExecutionPath = kFullGpuSubsetExecutionPath;

      for (const GpuDiffusePathStepRecord& step : vulkanResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
        if (event != GpuDiffusePathStepEvent::Inactive) {
          ++activeSteps;
        }
        if (event == GpuDiffusePathStepEvent::Miss) {
          ++loop.metrics.misses;
        } else if (event == GpuDiffusePathStepEvent::Hit) {
          ++loop.metrics.hits;
          if (gpuFloat4HasValue(step.emittedRadiance)) {
            ++loop.metrics.emissiveHits;
            ++loop.metrics.emissionContributionEvaluations;
          }
          if (gpuFloat4HasValue(step.directLightRadiance)) {
            loop.metrics.directLightSamples +=
              std::max<std::uint32_t>(1u, settings.directLightSamples);
            ++loop.metrics.directLightContributionEvaluations;
            ++loop.metrics.directLightContributingSamples;
          }
          if (stepHasContinuation(step)) {
            ++loop.metrics.spawnedContinuations;
          }
        } else if (event == GpuDiffusePathStepEvent::Unsupported) {
          ++loop.metrics.unsupportedHits;
        }
      }
      if (countedActiveSteps != 0u) {
        activeSteps = countedActiveSteps;
      }
      loop.metrics.activePaths = activeSteps;
      loop.metrics.closestHitRays = activeSteps;
    }

    void recordDepthCounts(GpuDiffusePathLoopResult& loop,
                           const VulkanGpuDiffusePathLoopKernelResult& vulkanResult,
                           const GpuDiffusePathLoopSettings& settings) {
      std::vector<std::uint64_t> counts(settings.maxDepth, 0u);
      if (!vulkanResult.activePathCountsPerDepth.empty()) {
        for (std::size_t depth = 0;
             depth != vulkanResult.activePathCountsPerDepth.size() && depth != counts.size();
             ++depth) {
          counts[depth] = vulkanResult.activePathCountsPerDepth[depth];
        }
      }
      for (const GpuDiffusePathStepRecord& step : vulkanResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
        if (event == GpuDiffusePathStepEvent::Inactive || step.depth >= counts.size()) {
          continue;
        }
        if (!vulkanResult.activePathCountsPerDepth.empty()) {
          continue;
        }
        ++counts[step.depth];
      }
      for (const std::uint64_t count : counts) {
        if (count == 0u) {
          continue;
        }
        loop.activePathsPerDepth.push_back(count);
      }
      loop.depthCount = loop.activePathsPerDepth.size();
    }

    [[nodiscard]] GpuDiffusePathLoopResult
    makeLoopResult(std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
                   const VulkanGpuDiffusePathLoopKernelResult& vulkanResult) {
      GpuDiffusePathLoopResult loop;
      loop.executionPath = kFullGpuSubsetExecutionPath;
      loop.pathStateResidency = kVulkanPathStateResidency;
      loop.frontierCompactionExecutionPath = vulkanResult.executionPath;
      loop.frontierCompactionPathStateResidency = vulkanResult.pathStateResidency;
      loop.retainedFrontierDispatchesIndirect = vulkanResult.retainedFrontierDispatchesIndirect;
      loop.platformName = "vulkan";
      loop.initialPathCount = initialPathCount;
      loop.stepRecords = vulkanResult.stepRecords;
      loop.denoiserFeatureRecords = vulkanResult.denoiserFeatureRecords;
      loop.denoiserFeatureRecordsCaptured = settings.captureDenoiserFeatures;
      loop.retainedIndexBytes =
        static_cast<std::uint64_t>(vulkanResult.retainedPathCount) * sizeof(std::uint32_t);
      loop.roundTrips = 1;
      loop.frontierCompactionUploadWorkerSeconds = vulkanResult.uploadWorkerSeconds;
      loop.frontierCompactionKernelWorkerSeconds = vulkanResult.kernelWorkerSeconds;
      loop.frontierCompactionReadbackWorkerSeconds = vulkanResult.readbackWorkerSeconds;
      loop.platformAccumulationColorSums = vulkanResult.accumulationColorSums;
      loop.platformAccumulationSampleCounts = vulkanResult.accumulationSampleCounts;
      loop.platformResolvedDisplayPixels = vulkanResult.resolvedDisplayPixels;
      loop.platformAccumulationAddedSamples = initialPathCount;
      loop.platformAccumulationBackend = "vulkan_diffuse_path_loop";
      loop.platformAccumulationResidency = "vulkan_accumulation_buffer";
      loop.platformAccumulationTargetMode = vulkanResult.echoedParameters.accumulationTargetMode;
      loop.platformAccumulationWidth = vulkanResult.echoedParameters.imageWidth;
      loop.platformAccumulationHeight = vulkanResult.echoedParameters.imageHeight;
      recordDepthCounts(loop, vulkanResult, settings);
      mergeStepMetrics(loop, vulkanResult, settings);

      if (!settings.captureDiagnostics) {
        return loop;
      }

      std::vector<GpuDiffusePathStateRecord> nextPathStates = vulkanResult.nextPathStates.empty()
                                                                ? vulkanResult.resolvedPathStates
                                                                : vulkanResult.nextPathStates;
      if (nextPathStates.size() != initialPathCount) {
        throw std::logic_error(
          "Vulkan diffuse path-loop backend returned mismatched path-state count");
      }
      for (GpuDiffusePathStateRecord path : nextPathStates) {
        if (gpuDiffusePathStateIsActive(path) && path.depth >= settings.maxDepth) {
          terminate(path);
          ++loop.maxDepthTerminatedPaths;
          ++loop.metrics.terminatedPaths;
        } else if (gpuDiffusePathStateIsTerminated(path) && path.depth >= settings.maxDepth) {
          ++loop.maxDepthTerminatedPaths;
          ++loop.metrics.terminatedPaths;
        } else if (gpuDiffusePathStateIsTerminated(path)) {
          ++loop.metrics.terminatedPaths;
        }
        if (gpuDiffusePathStateIsTerminated(path)) {
          loop.resolvedPathStates.push_back(path);
        }
      }
      return loop;
    }

    [[nodiscard]] GpuDiffusePathLoopResult
    makeLoopResult(const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                   const GpuDiffusePathLoopSettings& settings,
                   const VulkanGpuDiffusePathLoopKernelResult& vulkanResult) {
      return makeLoopResult(static_cast<std::uint64_t>(initialPathStates.size()), settings,
                            vulkanResult);
    }
#endif
  }

  std::shared_ptr<const VulkanGpuDiffusePathLoopBackend>
  VulkanGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const VulkanGpuDiffusePathLoopBackend> instance =
      std::make_shared<VulkanGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* VulkanGpuDiffusePathLoopBackend::name() const {
    return "vulkan_diffuse_path_loop";
  }

  bool VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    return VulkanGpuDiffusePathLoopKernel().launchPathAvailable();
#else
    return false;
#endif
  }

  const char* VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    static thread_local std::string reason;
    reason = VulkanGpuDiffusePathLoopKernel().launchPathUnavailableReason();
    if (reason.empty()) {
      reason = "Vulkan diffuse path-loop launch path is available";
    }
    return reason.c_str();
#else
    return "Vulkan diffuse path-loop backend is not enabled in this build";
#endif
  }

  const char* VulkanGpuDiffusePathLoopBackend::platformName() const {
    return "vulkan";
  }

  GpuDiffusePathLoopBackendSupport VulkanGpuDiffusePathLoopBackend::fullGpuPathLoopSupport(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      return {false, kernel.launchPathUnavailableReason()};
    }
    return GpuDiffusePathLoopSceneSupport().support(
      scene, settings,
      GpuDiffusePathLoopSceneSupportReasons{
        "Vulkan diffuse path-loop backend requires positive max depth",
        "Vulkan diffuse path-loop backend currently supports empty geometry or triangle, sphere, "
        "plane, rectangle, disk, open-cylinder, or torus primitives with static transforms only",
        "Vulkan diffuse path-loop backend currently supports Matte, Phong finite glossy, "
        "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
        "Vulkan diffuse path-loop backend currently supports ConstantColor, CheckerBoard texture "
        "graphs, nearest/bilinear ImageTexture, UVColorTexture, and bounded Tinted wrapper chains "
        "over those textures only",
        "Vulkan diffuse path-loop backend currently supports point, directional, or rectangular "
        "area lights only"});
#else
    (void)scene;
    (void)settings;
    return {false, fullGpuPathLoopUnavailableReason()};
#endif
  }

  GpuDiffusePathLoopResult VulkanGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    const VulkanAccumulationPlan accumulation = accumulationPlanFor(initialPathStates);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, initialPathStates, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    const VulkanGpuDiffusePathLoopKernelResult vulkanResult =
      VulkanGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                            settings.capturePlatformAccumulation,
                                                            settings.captureResolvedDisplay);
    return makeLoopResult(initialPathStates, settings, vulkanResult);
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }

  GpuDiffusePathLoopResult VulkanGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates =
      primaryPathGeneration.pathStates;
    const VulkanAccumulationPlan accumulation = accumulationPlanFor(primaryPathGeneration);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, primaryPathGeneration, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    const VulkanGpuDiffusePathLoopKernelResult vulkanResult =
      VulkanGpuDiffusePathLoopKernel().runWavefrontPathLoop(plan, initialPathStates,
                                                            settings.capturePlatformAccumulation,
                                                            settings.captureResolvedDisplay);
    return makeLoopResult(plan.parameters.initialPathCount, settings, vulkanResult);
#else
    (void)scene;
    (void)primaryPathGeneration;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
