#include "render/MetalGpuDiffusePathLoopBackend.h"

#include "render/GpuDiffusePathLoopSceneSupport.h"
#include "render/GpuFloat4.h"
#include "render/MetalGpuDiffusePathLoopKernel.h"
#include "render/TracingAccumulationLayout.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kMetalPathStateResidency = "metal_shared_diffuse_path_state";

    void terminate(GpuDiffusePathStateRecord& path) {
      path.flags &= ~gpuDiffusePathStateActiveFlag;
      path.flags |= gpuDiffusePathStateTerminatedFlag;
    }

    [[nodiscard]] bool stepHasContinuation(const GpuDiffusePathStepRecord& step) {
      return gpuFloat4HasValue(step.continuationThroughput);
    }

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
          "Metal diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Metal diffuse path-loop backend accumulation sample index exceeds layout range");
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
          "Metal diffuse path-loop backend requires a supported primary path descriptor");
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
          "Metal diffuse path-loop primary descriptor is outside request");
      }
      const std::uint64_t maxPixel =
        static_cast<std::uint64_t>(maxRowOffset) * rectilinear.requestedWidth +
        static_cast<std::uint64_t>(maxColumnOffset);
      const std::uint64_t maxSampleSlot = rectilinear.samplesPerPixel - 1u;
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Metal diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Metal diffuse path-loop backend accumulation sample index exceeds layout range");
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
          "Metal diffuse path-loop backend sample-slot accumulation shape overflows");
      }
      const std::uint64_t slotCount = shape.pixelCount * shape.sampleSlotCount;
      if (slotCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(
          "Metal diffuse path-loop backend sample-slot accumulation shape exceeds host range");
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
          "Metal diffuse path-loop backend path accumulation index exceeds layout range");
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::size_t>(1u, pathStates.size())), 1);
    }

    [[nodiscard]] TracingAccumulationLayout
    pathAccumulationLayoutFor(const GpuPrimaryPathDescriptor& descriptor) {
      const std::uint64_t pathCount = descriptor.pathCount();
      if (pathCount >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Metal diffuse path-loop backend path accumulation index exceeds layout range");
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::uint64_t>(1u, pathCount)), 1);
    }

    struct MetalAccumulationPlan {
      TracingAccumulationLayout layout;
      std::uint32_t targetMode{gpuDiffusePathLoopAccumulationTargetPixel};
    };

    [[nodiscard]] MetalAccumulationPlan
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

    [[nodiscard]] MetalAccumulationPlan
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

    [[nodiscard]] MetalAccumulationPlan
    accumulationPlanFor(const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration) {
      if (primaryPathGeneration.canGeneratePrimaryPathsOnDevice() &&
          primaryPathGeneration.pathStates.empty()) {
        return accumulationPlanFor(*primaryPathGeneration.primaryPathDescriptor);
      }
      return accumulationPlanFor(primaryPathGeneration.pathStates);
    }

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                          const MetalGpuDiffusePathLoopKernelResult& metalResult,
                          const GpuDiffusePathLoopSettings& settings) {
      (void)initialPathStates;
      std::uint64_t activeSteps = 0;
      std::uint64_t countedActiveSteps = 0;
      for (const std::uint32_t count : metalResult.activePathCountsPerDepth) {
        countedActiveSteps += count;
      }
      loop.metrics.closestHitExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.emissionExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightVisibilityExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightContributionExecutionPath = kFullGpuSubsetExecutionPath;

      for (const GpuDiffusePathStepRecord& step : metalResult.stepRecords) {
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
                           const MetalGpuDiffusePathLoopKernelResult& metalResult,
                           const GpuDiffusePathLoopSettings& settings) {
      std::vector<std::uint64_t> counts(settings.maxDepth, 0u);
      if (!metalResult.activePathCountsPerDepth.empty()) {
        for (std::size_t depth = 0;
             depth != metalResult.activePathCountsPerDepth.size() && depth != counts.size();
             ++depth) {
          counts[depth] = metalResult.activePathCountsPerDepth[depth];
        }
      }
      for (const GpuDiffusePathStepRecord& step : metalResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
        if (event == GpuDiffusePathStepEvent::Inactive || step.depth >= counts.size()) {
          continue;
        }
        if (!metalResult.activePathCountsPerDepth.empty()) {
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
                   const MetalGpuDiffusePathLoopKernelResult& metalResult) {
      GpuDiffusePathLoopResult loop;
      loop.executionPath = kFullGpuSubsetExecutionPath;
      loop.pathStateResidency = kMetalPathStateResidency;
      loop.frontierCompactionExecutionPath = metalResult.executionPath;
      loop.frontierCompactionPathStateResidency = metalResult.pathStateResidency;
      loop.platformName = "metal";
      loop.initialPathCount = initialPathCount;
      loop.stepRecords = metalResult.stepRecords;
      loop.denoiserFeatureRecords = metalResult.denoiserFeatureRecords;
      loop.denoiserFeatureRecordsCaptured = settings.captureDenoiserFeatures;
      loop.retainedIndexBytes =
        static_cast<std::uint64_t>(metalResult.retainedPathCount) * sizeof(std::uint32_t);
      loop.roundTrips = 1;
      loop.frontierCompactionUploadWorkerSeconds = metalResult.uploadWorkerSeconds;
      loop.frontierCompactionKernelWorkerSeconds = metalResult.kernelWorkerSeconds;
      loop.frontierCompactionReadbackWorkerSeconds = metalResult.readbackWorkerSeconds;
      loop.platformAccumulationColorSums = metalResult.accumulationColorSums;
      loop.platformAccumulationSampleCounts = metalResult.accumulationSampleCounts;
      loop.platformResolvedDisplayPixels = metalResult.resolvedDisplayPixels;
      loop.platformAccumulationAddedSamples = initialPathCount;
      loop.platformAccumulationBackend = "metal_diffuse_path_loop";
      loop.platformAccumulationResidency = "metal_accumulation_buffer";
      loop.platformAccumulationTargetMode = metalResult.echoedParameters.accumulationTargetMode;
      loop.platformAccumulationWidth = metalResult.echoedParameters.imageWidth;
      loop.platformAccumulationHeight = metalResult.echoedParameters.imageHeight;
      recordDepthCounts(loop, metalResult, settings);
      mergeStepMetrics(loop, {}, metalResult, settings);

      if (!settings.captureDiagnostics) {
        return loop;
      }

      std::vector<GpuDiffusePathStateRecord> nextPathStates = metalResult.nextPathStates.empty()
                                                                ? metalResult.resolvedPathStates
                                                                : metalResult.nextPathStates;
      if (nextPathStates.size() != initialPathCount) {
        throw std::logic_error(
          "Metal diffuse path-loop backend returned mismatched path-state count");
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
                   const MetalGpuDiffusePathLoopKernelResult& metalResult) {
      return makeLoopResult(static_cast<std::uint64_t>(initialPathStates.size()), settings,
                            metalResult);
    }
#endif
  }

  std::shared_ptr<const MetalGpuDiffusePathLoopBackend>
  MetalGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const MetalGpuDiffusePathLoopBackend> instance =
      std::make_shared<MetalGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* MetalGpuDiffusePathLoopBackend::name() const {
    return "metal_diffuse_path_loop";
  }

  bool MetalGpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    return MetalGpuDiffusePathLoopKernel().launchPathAvailable();
#else
    return false;
#endif
  }

  const char* MetalGpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    static thread_local std::string reason;
    reason = MetalGpuDiffusePathLoopKernel().launchPathUnavailableReason();
    if (reason.empty()) {
      reason = "Metal diffuse path-loop launch path is available";
    }
    return reason.c_str();
#else
    return "Metal diffuse path-loop backend is not enabled in this build";
#endif
  }

  const char* MetalGpuDiffusePathLoopBackend::platformName() const {
    return "metal";
  }

  GpuDiffusePathLoopBackendSupport MetalGpuDiffusePathLoopBackend::fullGpuPathLoopSupport(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      return {false, kernel.launchPathUnavailableReason()};
    }
    return GpuDiffusePathLoopSceneSupport().support(
      scene, settings,
      GpuDiffusePathLoopSceneSupportReasons{
        "Metal diffuse path-loop backend requires positive max depth",
        "Metal diffuse path-loop backend currently supports empty geometry or "
        "triangle/sphere/plane/rectangle/disk/open-cylinder/torus geometry only",
        "Metal diffuse path-loop backend currently supports Matte, Phong finite glossy, "
        "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
        "Metal diffuse path-loop backend currently supports ConstantColor, CheckerBoard texture "
        "graphs, nearest/bilinear ImageTexture, UVColorTexture, and bounded Tinted wrapper chains "
        "over those textures only",
        "Metal diffuse path-loop backend currently supports point, directional, and rectangular "
        "area lights only"});
#else
    (void)scene;
    (void)settings;
    return {false, fullGpuPathLoopUnavailableReason()};
#endif
  }

  GpuDiffusePathLoopResult MetalGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    const MetalAccumulationPlan accumulation = accumulationPlanFor(initialPathStates);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, initialPathStates, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    const MetalGpuDiffusePathLoopKernelResult metalResult =
      MetalGpuDiffusePathLoopKernel().runMattePathLoop(plan, initialPathStates,
                                                       settings.capturePlatformAccumulation,
                                                       settings.captureResolvedDisplay);
    return makeLoopResult(initialPathStates, settings, metalResult);
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }

  GpuDiffusePathLoopResult MetalGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) const {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const GpuDiffusePathLoopBackendSupport support = fullGpuPathLoopSupport(scene, settings);
    if (!support.supported) {
      throw std::invalid_argument(support.reason);
    }
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates =
      primaryPathGeneration.pathStates;
    const MetalAccumulationPlan accumulation = accumulationPlanFor(primaryPathGeneration);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, primaryPathGeneration, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
    const MetalGpuDiffusePathLoopKernelResult metalResult =
      MetalGpuDiffusePathLoopKernel().runMattePathLoop(plan, initialPathStates,
                                                       settings.capturePlatformAccumulation,
                                                       settings.captureResolvedDisplay);
    return makeLoopResult(plan.parameters.initialPathCount, settings, metalResult);
#else
    (void)scene;
    (void)primaryPathGeneration;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
