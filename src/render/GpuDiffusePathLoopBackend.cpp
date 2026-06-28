#include "render/GpuDiffusePathLoopBackend.h"

#include "render/GpuFloat4.h"

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalGpuDiffusePathFrontierCompactionBackend.h"
#include "render/MetalGpuDiffusePathLoopBackend.h"
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"
#include "render/VulkanGpuDiffusePathLoopBackend.h"
#endif

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace render {
  namespace {
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kNoPlatformPathLoopReason =
      "platform full-GPU path-loop kernel is not available yet";
    constexpr const char* kNoPlatformFullGpuPathLoopBackendReason =
      "platform full-GPU path-loop backend is not enabled in this build";

    struct ActiveAccumulationTargetShape {
      std::uint64_t pixelCount{1};
      std::uint64_t sampleSlotCount{1};
      std::uint64_t activePathCount{0};
    };

    [[nodiscard]] std::string backendMessage(const char* backendDisplayName, const char* message) {
      return std::string(backendDisplayName) + " diffuse path-loop backend " + message;
    }

    [[nodiscard]] std::uint32_t flag(bool enabled) {
      return enabled ? 1u : 0u;
    }

    [[nodiscard]] std::uint64_t pixelCount(const GpuDiffusePathLoopLaunchParameters& parameters,
                                           const char* backendDisplayName) {
      if (parameters.imageWidth != 0u &&
          parameters.imageHeight > std::numeric_limits<std::uint64_t>::max() /
                                     static_cast<std::uint64_t>(parameters.imageWidth)) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "returned accumulation dimensions that overflow"));
      }
      return static_cast<std::uint64_t>(parameters.imageWidth) *
             static_cast<std::uint64_t>(parameters.imageHeight);
    }

    [[nodiscard]] std::uint64_t
    displayPixelCount(const GpuDiffusePathLoopLaunchParameters& parameters,
                      const char* backendDisplayName) {
      if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetPath) {
        return 0u;
      }
      if (parameters.accumulationTargetMode == gpuDiffusePathLoopAccumulationTargetSampleSlot) {
        return parameters.imageWidth;
      }
      if (parameters.accumulationTargetMode != gpuDiffusePathLoopAccumulationTargetPixel) {
        throw std::logic_error(
          backendMessage(backendDisplayName, "returned unknown accumulation target mode"));
      }
      return pixelCount(parameters, backendDisplayName);
    }

    void validateEchoedLaunchParameters(std::uint64_t initialPathCount,
                                        const GpuDiffusePathLoopSettings& settings,
                                        const GpuDiffusePathLoopPlatformResult& platformResult,
                                        const char* backendDisplayName) {
      if (initialPathCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "initial path count exceeds launch descriptor"));
      }
      const GpuDiffusePathLoopLaunchParameters& echoed = platformResult.echoedParameters;
      const bool matches =
        echoed.layoutVersion == gpuDiffusePathLoopLaunchLayoutVersion &&
        echoed.maxDepth == settings.maxDepth &&
        echoed.russianRouletteDepth == settings.russianRouletteDepth &&
        echoed.directLightSamples == settings.directLightSamples &&
        echoed.captureDiagnostics == flag(settings.captureDiagnostics) &&
        echoed.captureMetrics == flag(settings.captureMetrics) &&
        echoed.captureDenoiserFeatures == flag(settings.captureDenoiserFeatures) &&
        echoed.displayResolveTonemap ==
          static_cast<std::uint32_t>(settings.displayResolveTonemap) &&
        echoed.initialPathCount == static_cast<std::uint32_t>(initialPathCount);
      if (!matches) {
        throw std::logic_error(
          backendMessage(backendDisplayName, "returned mismatched launch parameters"));
      }
    }

    void validatePlatformReadbacks(const GpuDiffusePathLoopSettings& settings,
                                   const GpuDiffusePathLoopPlatformResult& platformResult,
                                   const char* backendDisplayName) {
      const GpuDiffusePathLoopLaunchParameters& echoed = platformResult.echoedParameters;
      const std::uint64_t accumulationPixels = pixelCount(echoed, backendDisplayName);

      if (platformResult.accumulationColorSums.size() !=
          platformResult.accumulationSampleCounts.size()) {
        throw std::logic_error(backendMessage(
          backendDisplayName, "returned mismatched platform accumulation plane sizes"));
      }
      if (settings.capturePlatformAccumulation) {
        if (platformResult.accumulationColorSums.size() != accumulationPixels) {
          throw std::logic_error(backendMessage(
            backendDisplayName, "returned accumulation planes with unexpected size"));
        }
      } else if (!platformResult.accumulationColorSums.empty()) {
        throw std::logic_error(backendMessage(
          backendDisplayName, "returned accumulation planes when capture was disabled"));
      }

      const std::uint64_t resolvedPixels = displayPixelCount(echoed, backendDisplayName);
      if (settings.captureResolvedDisplay) {
        if (resolvedPixels == 0u || platformResult.resolvedDisplayPixels.size() != resolvedPixels) {
          throw std::logic_error(backendMessage(
            backendDisplayName, "returned display resolve pixels with unexpected size"));
        }
      } else if (!platformResult.resolvedDisplayPixels.empty()) {
        throw std::logic_error(backendMessage(
          backendDisplayName, "returned display resolve pixels when capture was disabled"));
      }

      if (settings.captureDenoiserFeatures) {
        if (platformResult.denoiserFeatureRecords.size() != accumulationPixels) {
          throw std::logic_error(backendMessage(
            backendDisplayName, "returned denoiser feature records with unexpected size"));
        }
      } else if (!platformResult.denoiserFeatureRecords.empty()) {
        throw std::logic_error(backendMessage(
          backendDisplayName, "returned denoiser feature records when capture was disabled"));
      }

      const bool hasRadianceDeltaMetrics =
        !platformResult.radianceDeltaSquaredSumPerDepth.empty() ||
        !platformResult.maxRadianceDeltaPerDepth.empty();
      if (hasRadianceDeltaMetrics) {
        if (!settings.captureMetrics) {
          throw std::logic_error(backendMessage(
            backendDisplayName, "returned radiance-delta metrics when capture was disabled"));
        }
        if (platformResult.radianceDeltaSquaredSumPerDepth.size() != echoed.maxDepth ||
            platformResult.maxRadianceDeltaPerDepth.size() != echoed.maxDepth) {
          throw std::logic_error(backendMessage(
            backendDisplayName, "returned radiance-delta metrics with unexpected depth count"));
        }
      }
    }

    [[nodiscard]] ActiveAccumulationTargetShape
    activeAccumulationTargetShapeFor(const std::vector<GpuDiffusePathStateRecord>& pathStates,
                                     const char* backendDisplayName) {
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
          backendMessage(backendDisplayName, "accumulation pixel index exceeds layout range"));
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "accumulation sample index exceeds layout range"));
      }
      return {activePathCount == 0u ? 1u : maxPixel + 1u,
              activePathCount == 0u ? 1u : maxSampleSlot + 1u, activePathCount};
    }

    [[nodiscard]] ActiveAccumulationTargetShape
    activeAccumulationTargetShapeFor(const GpuPrimaryPathDescriptor& descriptor,
                                     const char* backendDisplayName) {
      if (descriptor.mode != gpuPrimaryPathGenerationModePinhole &&
          descriptor.mode != gpuPrimaryPathGenerationModeOrthographic &&
          descriptor.mode != gpuPrimaryPathGenerationModeThinLens &&
          descriptor.mode != gpuPrimaryPathGenerationModeEquirectangular &&
          descriptor.mode != gpuPrimaryPathGenerationModeSpherical &&
          descriptor.mode != gpuPrimaryPathGenerationModeFishEye &&
          descriptor.mode != gpuPrimaryPathGenerationModeTiltShift) {
        throw std::invalid_argument(
          backendMessage(backendDisplayName, "requires a supported primary path descriptor"));
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
          backendMessage(backendDisplayName, "primary descriptor is outside request"));
      }
      const std::uint64_t maxPixel =
        static_cast<std::uint64_t>(maxRowOffset) * rectilinear.requestedWidth +
        static_cast<std::uint64_t>(maxColumnOffset);
      if (rectilinear.samplesPerPixel != 0u &&
          rectilinear.sampleOffset >
            std::numeric_limits<std::uint32_t>::max() - rectilinear.samplesPerPixel) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "primary descriptor sample range overflows"));
      }
      const std::uint64_t maxSampleSlot =
        rectilinear.sampleOffset + rectilinear.samplesPerPixel - 1u;
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "accumulation pixel index exceeds layout range"));
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "accumulation sample index exceeds layout range"));
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
                                 const ActiveAccumulationTargetShape& shape,
                                 const char* backendDisplayName) {
      if (shape.pixelCount > std::numeric_limits<std::uint64_t>::max() / shape.sampleSlotCount) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "sample-slot accumulation shape overflows"));
      }
      const std::uint64_t slotCount = shape.pixelCount * shape.sampleSlotCount;
      if (slotCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "sample-slot accumulation shape exceeds host range"));
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
    pathAccumulationLayoutFor(const std::vector<GpuDiffusePathStateRecord>& pathStates,
                              const char* backendDisplayName) {
      if (pathStates.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "path accumulation index exceeds layout range"));
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::size_t>(1u, pathStates.size())), 1);
    }

    [[nodiscard]] TracingAccumulationLayout
    pathAccumulationLayoutFor(const GpuPrimaryPathDescriptor& descriptor,
                              const char* backendDisplayName) {
      const std::uint64_t pathCount = descriptor.pathCount();
      if (pathCount >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          backendMessage(backendDisplayName, "path accumulation index exceeds layout range"));
      }
      return TracingAccumulationLayout::image(
        static_cast<int>(std::max<std::uint64_t>(1u, pathCount)), 1);
    }

    void terminate(GpuDiffusePathStateRecord& path) {
      path.flags &= ~gpuDiffusePathStateActiveFlag;
      path.flags |= gpuDiffusePathStateTerminatedFlag;
    }

    [[nodiscard]] bool stepHasContinuation(const GpuDiffusePathStepRecord& step) {
      return gpuFloat4HasValue(step.continuationThroughput);
    }

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const GpuDiffusePathLoopPlatformResult& platformResult) {
      std::uint64_t activeSteps = 0;
      std::uint64_t countedActiveSteps = 0;
      for (const std::uint32_t count : platformResult.activePathCountsPerDepth) {
        countedActiveSteps += count;
      }
      loop.metrics.closestHitExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.emissionExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightVisibilityExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightContributionExecutionPath = kFullGpuSubsetExecutionPath;

      for (const GpuDiffusePathStepRecord& step : platformResult.stepRecords) {
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
          loop.metrics.directLightSamples += step.directLightSampleCount;
          loop.metrics.directLightVisibilityRays += step.directLightVisibilityRayCount;
          loop.metrics.directLightContributionEvaluations +=
            step.directLightVisibilityRayCount -
            std::min(step.directLightVisibilityRayCount, step.directLightOccludedSampleCount);
          loop.metrics.directLightContributingSamples += step.directLightContributingSampleCount;
          loop.metrics.directLightOccludedSamples += step.directLightOccludedSampleCount;
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
                           const GpuDiffusePathLoopPlatformResult& platformResult,
                           const GpuDiffusePathLoopSettings& settings) {
      std::vector<std::uint64_t> counts(settings.maxDepth, 0u);
      if (!platformResult.activePathCountsPerDepth.empty()) {
        for (std::size_t depth = 0;
             depth != platformResult.activePathCountsPerDepth.size() && depth != counts.size();
             ++depth) {
          counts[depth] = platformResult.activePathCountsPerDepth[depth];
        }
      }
      for (const GpuDiffusePathStepRecord& step : platformResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
        if (event == GpuDiffusePathStepEvent::Inactive || step.depth >= counts.size()) {
          continue;
        }
        if (!platformResult.activePathCountsPerDepth.empty()) {
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

    void recordRadianceDeltaMetrics(GpuDiffusePathLoopResult& loop,
                                    const GpuDiffusePathLoopPlatformResult& platformResult) {
      if (platformResult.radianceDeltaSquaredSumPerDepth.empty()) {
        return;
      }
      const std::size_t depthCount = static_cast<std::size_t>(loop.depthCount);
      loop.radianceDeltaSquaredSumPerDepth.assign(
        platformResult.radianceDeltaSquaredSumPerDepth.begin(),
        platformResult.radianceDeltaSquaredSumPerDepth.begin() + depthCount);
      loop.maxRadianceDeltaPerDepth.assign(platformResult.maxRadianceDeltaPerDepth.begin(),
                                           platformResult.maxRadianceDeltaPerDepth.begin() +
                                             depthCount);
    }

    std::shared_ptr<const GpuDiffusePathLoopBackend> firstAvailableFullGpuBackend(
      const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>>& backends) {
      for (const auto& backend : backends) {
        if (backend && backend->fullGpuPathLoopAvailable()) {
          return backend;
        }
      }
      for (const auto& backend : backends) {
        if (backend) {
          return backend;
        }
      }
      return {};
    }

    std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>> fullGpuBackendsForGpuRequest() {
      std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>> backends;
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
      backends.push_back(MetalGpuDiffusePathLoopBackend::sharedInstance());
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
      backends.push_back(VulkanGpuDiffusePathLoopBackend::sharedInstance());
#endif
      return backends;
    }

    void appendMovedPathStates(std::vector<GpuDiffusePathStateRecord>& target,
                               std::vector<GpuDiffusePathStateRecord>&& source) {
      target.insert(target.end(), std::make_move_iterator(source.begin()),
                    std::make_move_iterator(source.end()));
    }

    void appendMovedStepRecords(std::vector<GpuDiffusePathStepRecord>& target,
                                std::vector<GpuDiffusePathStepRecord>&& source) {
      target.insert(target.end(), std::make_move_iterator(source.begin()),
                    std::make_move_iterator(source.end()));
    }

    void
    mergeMovedDenoiserFeatureRecords(std::vector<GpuDiffusePathDenoiserFeatureRecord>& target,
                                     std::vector<GpuDiffusePathDenoiserFeatureRecord>&& source) {
      if (source.empty()) {
        return;
      }
      if (target.empty()) {
        target = std::move(source);
        return;
      }
      if (target.size() != source.size()) {
        throw std::logic_error("GPU diffuse path-loop chunk denoiser feature size mismatch");
      }
      for (GpuDiffusePathDenoiserFeatureRecord& record : source) {
        if ((record.flags & gpuDiffusePathDenoiserFeatureValidFlag) == 0u ||
            record.pixelIndex >= target.size()) {
          continue;
        }
        target[record.pixelIndex] = record;
      }
    }

    void addActivePathCounts(std::vector<std::uint32_t>& target,
                             const std::vector<std::uint32_t>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size(), 0u);
      }
      for (std::size_t i = 0; i != source.size(); ++i) {
        if (target[i] > std::numeric_limits<std::uint32_t>::max() - source[i]) {
          throw std::overflow_error("GPU diffuse path-loop chunk active path count overflows");
        }
        target[i] += source[i];
      }
    }

    void addRadianceDeltaSquaredSums(std::vector<double>& target,
                                     const std::vector<double>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size(), 0.0);
      }
      for (std::size_t i = 0; i != source.size(); ++i) {
        target[i] += source[i];
      }
    }

    void mergeMaxRadianceDeltas(std::vector<double>& target, const std::vector<double>& source) {
      if (target.size() < source.size()) {
        target.resize(source.size(), 0.0);
      }
      for (std::size_t i = 0; i != source.size(); ++i) {
        target[i] = std::max(target[i], source[i]);
      }
    }

    constexpr std::uint64_t kAutoPrimaryLaunchPathWorkBudget = 1024ull * 1024ull;
    constexpr std::uint64_t kAutoPrimaryLaunchMinimumPathBudget = 16ull * 1024ull;
    constexpr std::uint64_t kAutoPrimaryLaunchMaximumPathBudget = 128ull * 1024ull;
    constexpr std::uint64_t kInteractiveAutoPrimaryLaunchPathWorkBudget = 640ull * 1024ull;
    constexpr std::uint64_t kInteractiveAutoPrimaryLaunchMinimumPathBudget = 16ull * 1024ull;
    constexpr std::uint64_t kInteractiveAutoPrimaryLaunchMaximumPathBudget = 64ull * 1024ull;

    [[nodiscard]] bool primaryGenerationCanUseSampleChunks(
      const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
      const GpuDiffusePathLoopSettings& settings) {
      return !settings.captureDiagnostics &&
             primaryPathGeneration.canGeneratePrimaryPathsOnDevice() &&
             primaryPathGeneration.pathStates.empty() &&
             primaryPathGeneration.primaryPathDescriptor.has_value();
    }

    [[nodiscard]] std::uint64_t descriptorPixelCount(const GpuPrimaryPathDescriptor& descriptor) {
      const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor.rectilinear;
      return static_cast<std::uint64_t>(rectilinear.actualWidth) *
             static_cast<std::uint64_t>(rectilinear.actualHeight);
    }

    [[nodiscard]] std::uint64_t
    autoPrimaryLaunchPathBudget(const GpuDiffusePathLoopSettings& settings) {
      const bool interactiveProgress =
        settings.interactiveDisplay ||
        (settings.captureResolvedDisplay && static_cast<bool>(settings.chunkProgressObserver));
      const std::uint64_t workBudget = interactiveProgress
                                         ? kInteractiveAutoPrimaryLaunchPathWorkBudget
                                         : kAutoPrimaryLaunchPathWorkBudget;
      const std::uint64_t minimumPathBudget = interactiveProgress
                                                ? kInteractiveAutoPrimaryLaunchMinimumPathBudget
                                                : kAutoPrimaryLaunchMinimumPathBudget;
      const std::uint64_t maximumPathBudget = interactiveProgress
                                                ? kInteractiveAutoPrimaryLaunchMaximumPathBudget
                                                : kAutoPrimaryLaunchMaximumPathBudget;
      const std::uint64_t depthWork = std::max<std::uint64_t>(1u, settings.maxDepth);
      const std::uint64_t directLightWork =
        std::max<std::uint64_t>(1u, settings.directLightSamples);
      const std::uint64_t workScale =
        depthWork > std::numeric_limits<std::uint64_t>::max() / directLightWork
          ? std::numeric_limits<std::uint64_t>::max()
          : depthWork * directLightWork;
      const std::uint64_t scaledBudget = std::max<std::uint64_t>(1u, workBudget / workScale);
      return std::clamp(scaledBudget, minimumPathBudget, maximumPathBudget);
    }

    [[nodiscard]] std::uint32_t
    budgetedSampleChunkSize(const GpuPrimaryPathDescriptor& descriptor,
                            const GpuDiffusePathLoopSettings& settings) {
      const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor.rectilinear;
      const std::uint64_t pixelCount = descriptorPixelCount(descriptor);
      const std::uint64_t pathBudget = autoPrimaryLaunchPathBudget(settings);
      if (pixelCount == 0u || descriptor.pathCount() <= pathBudget) {
        return 0u;
      }
      const std::uint64_t samplesByBudget = std::max<std::uint64_t>(1u, pathBudget / pixelCount);
      return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(rectilinear.samplesPerPixel, samplesByBudget));
    }

    [[nodiscard]] bool canUsePixelAccumulationAcrossPrimarySampleChunks(
      const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
      const GpuDiffusePathLoopSettings& settings) {
      return primaryGenerationCanUseSampleChunks(primaryPathGeneration, settings) &&
             resolvedGpuDiffusePrimarySampleChunkSize(primaryPathGeneration, settings) == 1u &&
             primaryPathGeneration.primaryPathDescriptor->rectilinear.samplesPerPixel > 1u;
    }

    [[nodiscard]] bool descriptorNeedsPixelTiles(const GpuPrimaryPathDescriptor& descriptor,
                                                 std::uint32_t sampleCount,
                                                 const GpuDiffusePathLoopSettings& settings) {
      const std::uint64_t pixelCount = descriptorPixelCount(descriptor);
      return sampleCount != 0u && pixelCount > autoPrimaryLaunchPathBudget(settings) / sampleCount;
    }

    [[nodiscard]] std::vector<Recti>
    primaryPixelTilesFor(const GpuPrimaryPathDescriptor& descriptor, std::uint32_t sampleCount,
                         const GpuDiffusePathLoopSettings& settings) {
      const Recti actualRect = descriptor.actualRect();
      if (!descriptorNeedsPixelTiles(descriptor, sampleCount, settings)) {
        return {actualRect};
      }

      const std::uint64_t maxPixelsPerChunk =
        std::max<std::uint64_t>(1u, autoPrimaryLaunchPathBudget(settings) / sampleCount);
      std::vector<Recti> tiles;
      for (int y = actualRect.top(); y < actualRect.bottom();) {
        const int remainingHeight = actualRect.bottom() - y;
        if (static_cast<std::uint64_t>(actualRect.width()) <= maxPixelsPerChunk) {
          const int tileHeight = std::max<int>(
            1, static_cast<int>(std::min<std::uint64_t>(
                 static_cast<std::uint64_t>(remainingHeight),
                 maxPixelsPerChunk / static_cast<std::uint64_t>(actualRect.width()))));
          tiles.emplace_back(actualRect.left(), y, actualRect.width(), tileHeight);
          y += tileHeight;
          continue;
        }

        int x = actualRect.left();
        while (x < actualRect.right()) {
          const int remainingWidth = actualRect.right() - x;
          const int tileWidth =
            std::max<int>(1, static_cast<int>(std::min<std::uint64_t>(
                               static_cast<std::uint64_t>(remainingWidth), maxPixelsPerChunk)));
          tiles.emplace_back(x, y, tileWidth, 1);
          x += tileWidth;
        }
        ++y;
      }
      return tiles;
    }
  }

  std::uint32_t resolvedGpuDiffusePrimarySampleChunkSize(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) {
    if (!primaryGenerationCanUseSampleChunks(primaryPathGeneration, settings)) {
      return 0u;
    }
    const GpuPrimaryPathDescriptor& descriptor = *primaryPathGeneration.primaryPathDescriptor;
    const std::uint32_t budgetedChunkSize = budgetedSampleChunkSize(descriptor, settings);
    if (settings.primarySampleChunkSize == 0u) {
      return budgetedChunkSize;
    }
    if (budgetedChunkSize == 0u) {
      return settings.primarySampleChunkSize;
    }
    return std::min(settings.primarySampleChunkSize, budgetedChunkSize);
  }

  bool canChunkGpuDiffusePrimarySamples(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) {
    if (!primaryGenerationCanUseSampleChunks(primaryPathGeneration, settings)) {
      return false;
    }
    const std::uint32_t chunkSize =
      resolvedGpuDiffusePrimarySampleChunkSize(primaryPathGeneration, settings);
    if (chunkSize == 0u) {
      return descriptorNeedsPixelTiles(
        *primaryPathGeneration.primaryPathDescriptor,
        primaryPathGeneration.primaryPathDescriptor->rectilinear.samplesPerPixel, settings);
    }
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      primaryPathGeneration.primaryPathDescriptor->rectilinear;
    return descriptor.samplesPerPixel > chunkSize ||
           descriptorNeedsPixelTiles(*primaryPathGeneration.primaryPathDescriptor, chunkSize,
                                     settings);
  }

  std::vector<GpuDiffusePrimaryPathSampleChunk> gpuDiffusePrimarySampleChunksFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings) {
    if (!primaryGenerationCanUseSampleChunks(primaryPathGeneration, settings)) {
      return {};
    }

    const GpuPrimaryPathDescriptor& descriptor = *primaryPathGeneration.primaryPathDescriptor;
    const std::uint32_t sampleBegin = descriptor.rectilinear.sampleOffset;
    const std::uint32_t sampleCount = descriptor.rectilinear.samplesPerPixel;
    const std::uint32_t resolvedChunkSize =
      resolvedGpuDiffusePrimarySampleChunkSize(primaryPathGeneration, settings);
    if (resolvedChunkSize == 0u && !descriptorNeedsPixelTiles(descriptor, sampleCount, settings)) {
      return {};
    }
    if (sampleBegin > std::numeric_limits<std::uint32_t>::max() - sampleCount) {
      throw std::overflow_error("GPU diffuse primary sample chunks exceed sample index range");
    }
    const std::uint32_t sampleChunkSize = resolvedChunkSize == 0u ? sampleCount : resolvedChunkSize;
    const std::uint32_t sampleEnd = sampleBegin + sampleCount;
    std::vector<GpuDiffusePrimaryPathSampleChunk> chunks;
    for (std::uint32_t sampleOffset = sampleBegin; sampleOffset < sampleEnd;) {
      const std::uint32_t remaining = sampleEnd - sampleOffset;
      const std::uint32_t chunkSampleCount = std::min(sampleChunkSize, remaining);

      const GpuPrimaryPathDescriptor sampleDescriptor =
        descriptor.withSampleRange(sampleOffset, chunkSampleCount);
      const std::vector<Recti> pixelTiles =
        primaryPixelTilesFor(sampleDescriptor, chunkSampleCount, settings);
      for (std::size_t tileIndex = 0; tileIndex != pixelTiles.size(); ++tileIndex) {
        const Recti& pixelTile = pixelTiles[tileIndex];
        GpuDiffusePrimaryPathSampleChunk chunk;
        chunk.primaryPathGeneration = primaryPathGeneration;
        chunk.primaryPathGeneration.pathStates.clear();
        chunk.primaryPathGeneration.primaryPathDescriptor =
          sampleDescriptor.withActualRect(pixelTile);
        chunk.primaryPathGeneration.requestedRect =
          chunk.primaryPathGeneration.primaryPathDescriptor->requestedRect();
        chunk.primaryPathGeneration.actualRect =
          chunk.primaryPathGeneration.primaryPathDescriptor->actualRect();
        chunk.primaryPathGeneration.generatedPrimarySamples =
          chunk.primaryPathGeneration.primaryPathDescriptor->pathCount();
        chunk.primaryPathGeneration.skippedPrimarySamples = 0u;
        chunk.firstChunk = chunks.empty();
        chunk.completesSampleRange = tileIndex + 1u == pixelTiles.size();
        chunks.push_back(std::move(chunk));
      }
      sampleOffset += chunkSampleCount;
    }
    if (!chunks.empty()) {
      chunks.back().finalChunk = true;
    }
    return chunks;
  }

  bool shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(
    const GpuDiffusePathLoopSettings& settings, const GpuDiffusePrimaryPathSampleChunk& chunk) {
    return settings.captureResolvedDisplay &&
           (chunk.finalChunk ||
            (settings.chunkProgressObserver && (chunk.firstChunk || chunk.completesSampleRange)));
  }

  void mergePlatformGpuDiffusePathLoopChunkResult(GpuDiffusePathLoopPlatformResult& merged,
                                                  GpuDiffusePathLoopPlatformResult&& chunkResult) {
    if (merged.executionPath.empty()) {
      merged = std::move(chunkResult);
      return;
    }

    appendMovedPathStates(merged.resolvedPathStates, std::move(chunkResult.resolvedPathStates));
    appendMovedPathStates(merged.nextPathStates, std::move(chunkResult.nextPathStates));
    appendMovedStepRecords(merged.stepRecords, std::move(chunkResult.stepRecords));
    mergeMovedDenoiserFeatureRecords(merged.denoiserFeatureRecords,
                                     std::move(chunkResult.denoiserFeatureRecords));
    if (!chunkResult.accumulationColorSums.empty()) {
      merged.accumulationColorSums = std::move(chunkResult.accumulationColorSums);
    }
    if (!chunkResult.accumulationSampleCounts.empty()) {
      merged.accumulationSampleCounts = std::move(chunkResult.accumulationSampleCounts);
    }
    if (!chunkResult.resolvedDisplayPixels.empty()) {
      merged.resolvedDisplayPixels = std::move(chunkResult.resolvedDisplayPixels);
    }
    if (merged.resolvedDisplayReadbacks >
        std::numeric_limits<std::uint64_t>::max() - chunkResult.resolvedDisplayReadbacks) {
      throw std::overflow_error("GPU diffuse path-loop chunk resolved-display readback count "
                                "overflows");
    }
    merged.resolvedDisplayReadbacks += chunkResult.resolvedDisplayReadbacks;
    addActivePathCounts(merged.activePathCountsPerDepth, chunkResult.activePathCountsPerDepth);
    addRadianceDeltaSquaredSums(merged.radianceDeltaSquaredSumPerDepth,
                                chunkResult.radianceDeltaSquaredSumPerDepth);
    mergeMaxRadianceDeltas(merged.maxRadianceDeltaPerDepth, chunkResult.maxRadianceDeltaPerDepth);
    if (merged.retainedPathCount >
        std::numeric_limits<std::uint32_t>::max() - chunkResult.retainedPathCount) {
      throw std::overflow_error("GPU diffuse path-loop chunk retained path count overflows");
    }
    merged.retainedPathCount += chunkResult.retainedPathCount;
    merged.retainedFrontierDispatchesIndirect =
      merged.retainedFrontierDispatchesIndirect || chunkResult.retainedFrontierDispatchesIndirect;
    merged.sceneUploadCacheHit = chunkResult.sceneUploadCacheHit;
    if (merged.sceneUploadBytesWritten >
        std::numeric_limits<std::uint64_t>::max() - chunkResult.sceneUploadBytesWritten) {
      throw std::overflow_error("GPU diffuse path-loop chunk scene upload byte count overflows");
    }
    merged.sceneUploadBytesWritten += chunkResult.sceneUploadBytesWritten;
    merged.uploadWorkerSeconds += chunkResult.uploadWorkerSeconds;
    merged.kernelWorkerSeconds += chunkResult.kernelWorkerSeconds;
    merged.readbackWorkerSeconds += chunkResult.readbackWorkerSeconds;
  }

  void notifyGpuDiffusePathLoopChunkProgress(
    const GpuDiffusePathLoopSettings& settings,
    const GpuDiffusePrimaryPathStateGeneration& fullPrimaryPathGeneration,
    const GpuDiffusePrimaryPathSampleChunk& chunk,
    const GpuDiffusePathLoopPlatformResult& chunkResult) {
    if (!settings.chunkProgressObserver || !fullPrimaryPathGeneration.primaryPathDescriptor ||
        !chunk.primaryPathGeneration.primaryPathDescriptor) {
      return;
    }

    const GpuRectilinearPrimaryPathDescriptor& fullDescriptor =
      fullPrimaryPathGeneration.primaryPathDescriptor->rectilinear;
    const GpuRectilinearPrimaryPathDescriptor& chunkDescriptor =
      chunk.primaryPathGeneration.primaryPathDescriptor->rectilinear;
    std::uint32_t completedSampleCount =
      chunk.completesSampleRange ? chunkDescriptor.samplesPerPixel : 0u;
    if (chunk.completesSampleRange && chunkDescriptor.sampleOffset >= fullDescriptor.sampleOffset) {
      completedSampleCount += chunkDescriptor.sampleOffset - fullDescriptor.sampleOffset;
    } else if (!chunk.completesSampleRange &&
               chunkDescriptor.sampleOffset > fullDescriptor.sampleOffset) {
      completedSampleCount = chunkDescriptor.sampleOffset - fullDescriptor.sampleOffset;
    }
    completedSampleCount = std::min(completedSampleCount, fullDescriptor.samplesPerPixel);

    GpuDiffusePathLoopChunkProgress progress;
    progress.sampleOffset = chunkDescriptor.sampleOffset;
    progress.sampleCount = chunkDescriptor.samplesPerPixel;
    progress.totalSampleCount = fullDescriptor.samplesPerPixel;
    progress.completedSampleCount = completedSampleCount;
    progress.firstChunk = chunk.firstChunk;
    progress.finalChunk = chunk.finalChunk;
    progress.resolvedDisplayPixels =
      chunkResult.resolvedDisplayPixels.empty() ? nullptr : &chunkResult.resolvedDisplayPixels;
    settings.chunkProgressObserver(progress);
  }

  bool gpuDiffusePathLoopCancelled(const GpuDiffusePathLoopSettings& settings) {
    return settings.cancellationCallback && settings.cancellationCallback();
  }

  void throwIfGpuDiffusePathLoopCancelled(const GpuDiffusePathLoopSettings& settings) {
    if (gpuDiffusePathLoopCancelled(settings)) {
      throw std::runtime_error("GPU diffuse path-loop cancelled");
    }
  }

  GpuDiffusePathLoopBackendChoice selectFullGpuDiffusePathLoopBackend(
    const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>>& backends,
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) {
    bool sawBackend = false;
    bool sawAvailableBackend = false;
    std::string firstUnavailableReason;
    std::string firstUnsupportedReason;

    for (const auto& backend : backends) {
      if (!backend) {
        continue;
      }
      sawBackend = true;
      if (!backend->fullGpuPathLoopAvailable()) {
        if (firstUnavailableReason.empty()) {
          firstUnavailableReason = backend->fullGpuPathLoopUnavailableReason();
        }
        continue;
      }

      sawAvailableBackend = true;
      const GpuDiffusePathLoopBackendSupport support =
        backend->fullGpuPathLoopSupport(scene, settings);
      if (support.supported) {
        return {backend, {}};
      }
      if (firstUnsupportedReason.empty()) {
        firstUnsupportedReason = support.reason.empty()
                                   ? "platform full-GPU path-loop backend rejected scene/settings"
                                   : support.reason;
      }
    }

    if (!sawBackend) {
      return {nullptr, kNoPlatformFullGpuPathLoopBackendReason};
    }
    if (sawAvailableBackend) {
      return {nullptr, firstUnsupportedReason.empty()
                         ? "no platform full-GPU path-loop backend supports this scene/settings"
                         : firstUnsupportedReason};
    }
    return {nullptr, firstUnavailableReason.empty() ? kNoPlatformFullGpuPathLoopBackendReason
                                                    : firstUnavailableReason};
  }

  GpuDiffusePathLoopPlatformAccumulationPlan platformGpuDiffusePathLoopAccumulationPlanFor(
    const std::vector<GpuDiffusePathStateRecord>& pathStates, const char* backendDisplayName) {
    const ActiveAccumulationTargetShape shape =
      activeAccumulationTargetShapeFor(pathStates, backendDisplayName);
    if (!hasDuplicateActivePixelTarget(pathStates, shape)) {
      return {pixelAccumulationLayoutFor(shape), gpuDiffusePathLoopAccumulationTargetPixel};
    }

    const TracingAccumulationLayout pathLayout =
      pathAccumulationLayoutFor(pathStates, backendDisplayName);
    const TracingAccumulationLayout sampleSlotLayout = sampleSlotAccumulationLayoutFor(shape);
    if (!hasDuplicateActiveSampleSlot(pathStates, shape, backendDisplayName) &&
        sampleSlotLayout.pixelCount() <= pathLayout.pixelCount()) {
      return {sampleSlotLayout, gpuDiffusePathLoopAccumulationTargetSampleSlot};
    }
    return {pathLayout, gpuDiffusePathLoopAccumulationTargetPath};
  }

  GpuDiffusePathLoopPlatformAccumulationPlan
  platformGpuDiffusePathLoopAccumulationPlanFor(const GpuPrimaryPathDescriptor& descriptor,
                                                const char* backendDisplayName) {
    const ActiveAccumulationTargetShape shape =
      activeAccumulationTargetShapeFor(descriptor, backendDisplayName);
    const bool hasDuplicatePixelTargets =
      descriptor.generatesOnDevice() && descriptor.rectilinear.samplesPerPixel > 1u;
    if (!hasDuplicatePixelTargets) {
      return {pixelAccumulationLayoutFor(shape), gpuDiffusePathLoopAccumulationTargetPixel};
    }

    const TracingAccumulationLayout pathLayout =
      pathAccumulationLayoutFor(descriptor, backendDisplayName);
    const TracingAccumulationLayout sampleSlotLayout = sampleSlotAccumulationLayoutFor(shape);
    if (sampleSlotLayout.pixelCount() <= pathLayout.pixelCount()) {
      return {sampleSlotLayout, gpuDiffusePathLoopAccumulationTargetSampleSlot};
    }
    return {pathLayout, gpuDiffusePathLoopAccumulationTargetPath};
  }

  GpuDiffusePathLoopPlatformAccumulationPlan platformGpuDiffusePathLoopAccumulationPlanFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const char* backendDisplayName) {
    if (primaryPathGeneration.canGeneratePrimaryPathsOnDevice() &&
        primaryPathGeneration.pathStates.empty()) {
      return platformGpuDiffusePathLoopAccumulationPlanFor(
        *primaryPathGeneration.primaryPathDescriptor, backendDisplayName);
    }
    return platformGpuDiffusePathLoopAccumulationPlanFor(primaryPathGeneration.pathStates,
                                                         backendDisplayName);
  }

  GpuDiffusePathLoopPlatformAccumulationPlan platformGpuDiffusePathLoopAccumulationPlanFor(
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const GpuDiffusePathLoopSettings& settings, const char* backendDisplayName) {
    if (primaryPathGeneration.canGeneratePrimaryPathsOnDevice() &&
        primaryPathGeneration.pathStates.empty()) {
      if (canUsePixelAccumulationAcrossPrimarySampleChunks(primaryPathGeneration, settings)) {
        const ActiveAccumulationTargetShape shape = activeAccumulationTargetShapeFor(
          *primaryPathGeneration.primaryPathDescriptor, backendDisplayName);
        return {pixelAccumulationLayoutFor(shape), gpuDiffusePathLoopAccumulationTargetPixel};
      }
      return platformGpuDiffusePathLoopAccumulationPlanFor(
        *primaryPathGeneration.primaryPathDescriptor, backendDisplayName);
    }
    return platformGpuDiffusePathLoopAccumulationPlanFor(primaryPathGeneration.pathStates,
                                                         backendDisplayName);
  }

  GpuDiffusePathLoopResult makePlatformGpuDiffusePathLoopResult(
    std::uint64_t initialPathCount, const GpuDiffusePathLoopSettings& settings,
    GpuDiffusePathLoopPlatformResult&& platformResult, const char* backendDisplayName,
    const char* platformName, const char* pathStateResidency, const char* accumulationBackend,
    const char* accumulationResidency) {
    validateEchoedLaunchParameters(initialPathCount, settings, platformResult, backendDisplayName);
    validatePlatformReadbacks(settings, platformResult, backendDisplayName);

    GpuDiffusePathLoopResult loop;
    loop.executionPath = kFullGpuSubsetExecutionPath;
    loop.schedule = std::move(platformResult.schedule);
    loop.pathStateResidency = pathStateResidency;
    loop.frontierCompactionExecutionPath = std::move(platformResult.executionPath);
    loop.frontierCompactionPathStateResidency = std::move(platformResult.pathStateResidency);
    loop.retainedFrontierDispatchesIndirect = platformResult.retainedFrontierDispatchesIndirect;
    loop.platformName = platformName;
    loop.initialPathCount = initialPathCount;
    if (settings.captureMetrics || settings.captureDiagnostics) {
      loop.retainedIndexBytes =
        static_cast<std::uint64_t>(platformResult.retainedPathCount) * sizeof(std::uint32_t);
    }
    loop.roundTrips = 1;
    loop.frontierCompactionUploadWorkerSeconds = platformResult.uploadWorkerSeconds;
    loop.frontierCompactionKernelWorkerSeconds = platformResult.kernelWorkerSeconds;
    loop.frontierCompactionReadbackWorkerSeconds = platformResult.readbackWorkerSeconds;
    loop.platformSceneUploadCacheHit = platformResult.sceneUploadCacheHit;
    loop.platformSceneUploadBytesWritten = platformResult.sceneUploadBytesWritten;
    loop.platformAccumulationColorSums = std::move(platformResult.accumulationColorSums);
    loop.platformAccumulationSampleCounts = std::move(platformResult.accumulationSampleCounts);
    loop.platformResolvedDisplayPixels = std::move(platformResult.resolvedDisplayPixels);
    loop.platformResolvedDisplayReadbacks = platformResult.resolvedDisplayReadbacks;
    loop.platformAccumulationAddedSamples = initialPathCount;
    loop.platformAccumulationBackend = accumulationBackend;
    loop.platformAccumulationResidency = accumulationResidency;
    loop.platformAccumulationTargetMode = platformResult.echoedParameters.accumulationTargetMode;
    loop.platformAccumulationWidth = platformResult.echoedParameters.imageWidth;
    loop.platformAccumulationHeight = platformResult.echoedParameters.imageHeight;
    if (settings.captureMetrics || settings.captureDiagnostics) {
      recordDepthCounts(loop, platformResult, settings);
      recordRadianceDeltaMetrics(loop, platformResult);
      mergeStepMetrics(loop, platformResult);
    }
    loop.stepRecords = std::move(platformResult.stepRecords);
    loop.denoiserFeatureRecords = std::move(platformResult.denoiserFeatureRecords);
    loop.denoiserFeatureRecordsCaptured = settings.captureDenoiserFeatures;

    if (!settings.captureDiagnostics) {
      return loop;
    }

    std::vector<GpuDiffusePathStateRecord> nextPathStates =
      platformResult.nextPathStates.empty() ? std::move(platformResult.resolvedPathStates)
                                            : std::move(platformResult.nextPathStates);
    if (nextPathStates.size() != initialPathCount) {
      throw std::logic_error(
        backendMessage(backendDisplayName, "returned mismatched path-state count"));
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
      } else if (gpuDiffusePathStateIsActive(path)) {
        loop.retainedFrontierPathStates.push_back(path);
      }
    }
    return loop;
  }

  std::shared_ptr<const GpuDiffusePathLoopBackend>
  GpuDiffusePathLoopBackend::defaultBackendForGpuRequest() {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    {
      auto compactionBackend = std::make_shared<MetalGpuDiffusePathFrontierCompactionBackend>();
      if (compactionBackend->compactionPathAvailable()) {
        return std::make_shared<CompactingGpuDiffusePathLoopBackend>(compactionBackend);
      }
    }
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    {
      auto compactionBackend = std::make_shared<VulkanGpuDiffusePathFrontierCompactionBackend>();
      if (compactionBackend->compactionPathAvailable()) {
        return std::make_shared<CompactingGpuDiffusePathLoopBackend>(compactionBackend);
      }
    }
#endif
    return CpuReferenceGpuDiffusePathLoopBackend::sharedInstance();
  }

  std::shared_ptr<const GpuDiffusePathLoopBackend>
  GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest() {
    return firstAvailableFullGpuBackend(fullGpuBackendsForGpuRequest());
  }

  GpuDiffusePathLoopBackendChoice GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings) {
    return selectFullGpuDiffusePathLoopBackend(fullGpuBackendsForGpuRequest(), scene, settings);
  }

  bool GpuDiffusePathLoopBackend::fullGpuPathLoopAvailable() const {
    return false;
  }

  const char* GpuDiffusePathLoopBackend::fullGpuPathLoopUnavailableReason() const {
    return kNoPlatformPathLoopReason;
  }

  const char* GpuDiffusePathLoopBackend::platformName() const {
    return "";
  }

  GpuDiffusePathLoopBackendSupport
  GpuDiffusePathLoopBackend::fullGpuPathLoopSupport(const GpuTracingSceneSections& scene) const {
    return fullGpuPathLoopSupport(scene, GpuDiffusePathLoopSettings());
  }

  GpuDiffusePathLoopBackendSupport
  GpuDiffusePathLoopBackend::fullGpuPathLoopSupport(const GpuTracingSceneSections&,
                                                    const GpuDiffusePathLoopSettings&) const {
    if (!fullGpuPathLoopAvailable()) {
      return {false, fullGpuPathLoopUnavailableReason()};
    }
    return {true, {}};
  }

  GpuDiffusePathLoopResult
  GpuDiffusePathLoopBackend::run(const GpuTracingSceneSections& scene,
                                 const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
                                 const GpuDiffusePathLoopSettings& settings) const {
    return run(scene, primaryPathGeneration.pathStates, settings);
  }

  std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend>
  CpuReferenceGpuDiffusePathLoopBackend::sharedInstance() {
    static const std::shared_ptr<const CpuReferenceGpuDiffusePathLoopBackend> instance =
      std::make_shared<CpuReferenceGpuDiffusePathLoopBackend>();
    return instance;
  }

  const char* CpuReferenceGpuDiffusePathLoopBackend::name() const {
    return "compiled_cpu_reference";
  }

  GpuDiffusePathLoopResult CpuReferenceGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
    return GpuDiffusePathLoop().run(scene, initialPathStates, settings);
  }

  CompactingGpuDiffusePathLoopBackend::CompactingGpuDiffusePathLoopBackend(
    std::shared_ptr<const GpuDiffusePathFrontierCompactionBackend> compactionBackend)
      : m_compactionBackend(std::move(compactionBackend)) {
    if (!m_compactionBackend) {
      throw std::invalid_argument("compacting GPU diffuse path-loop backend requires compaction");
    }
  }

  const char* CompactingGpuDiffusePathLoopBackend::name() const {
    return "compiled_cpu_reference_with_compaction_backend";
  }

  const GpuDiffusePathFrontierCompactionBackend&
  CompactingGpuDiffusePathLoopBackend::compactionBackend() const {
    return *m_compactionBackend;
  }

  GpuDiffusePathLoopResult CompactingGpuDiffusePathLoopBackend::run(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const GpuDiffusePathLoopSettings& settings) const {
    return GpuDiffusePathLoop().run(scene, initialPathStates, settings, *m_compactionBackend);
  }
}
