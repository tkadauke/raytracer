#include "render/VulkanGpuDiffusePathLoopBackend.h"

#include "render/TracingAccumulationLayout.h"
#include "render/VulkanGpuDiffusePathLoopKernel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kVulkanPathStateResidency = "vulkan_host_visible_diffuse_path_state";

    [[nodiscard]] bool arrayHasValue(const std::array<float, 4>& value) {
      return std::any_of(value.begin(), value.end(),
                         [](float component) { return std::fabs(component) > 1.0e-8f; });
    }

    [[nodiscard]] bool sceneHasNoGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return geometry.primitives.empty() && geometry.bvh.empty();
    }

    void validateUniqueActivePixels(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      std::uint64_t maxPixel = 0;
      bool hasActivePath = false;
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        hasActivePath = true;
        maxPixel = std::max<std::uint64_t>(maxPixel, path.pixelIndex);
      }
      if (!hasActivePath) {
        return;
      }
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation pixel index exceeds layout range");
      }

      std::vector<bool> seen(static_cast<std::size_t>(maxPixel + 1u), false);
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        if (seen[path.pixelIndex]) {
          throw std::invalid_argument(
            "Vulkan diffuse path-loop backend requires unique active pixel targets");
        }
        seen[path.pixelIndex] = true;
      }
    }

    [[nodiscard]] TracingAccumulationLayout
    accumulationLayoutFor(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      std::uint64_t maxPixel = 0;
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        maxPixel = std::max<std::uint64_t>(maxPixel, path.pixelIndex);
      }
      if (maxPixel >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      return TracingAccumulationLayout::image(static_cast<int>(maxPixel + 1u), 1);
    }

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const VulkanGpuDiffusePathLoopKernelResult& vulkanResult) {
      std::uint64_t activeSteps = 0;
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
          if (arrayHasValue(step.emittedRadiance)) {
            ++loop.metrics.emissiveHits;
            ++loop.metrics.emissionContributionEvaluations;
          }
          if (arrayHasValue(step.directLightRadiance)) {
            ++loop.metrics.directLightSamples;
            ++loop.metrics.directLightContributionEvaluations;
            ++loop.metrics.directLightContributingSamples;
          }
          if (arrayHasValue(step.continuationThroughput)) {
            ++loop.metrics.spawnedContinuations;
          }
        } else if (event == GpuDiffusePathStepEvent::Unsupported) {
          ++loop.metrics.unsupportedHits;
        }
      }
      loop.metrics.activePaths = activeSteps;
      loop.metrics.closestHitRays = activeSteps;
    }

    void recordDepthCounts(GpuDiffusePathLoopResult& loop,
                           const VulkanGpuDiffusePathLoopKernelResult& vulkanResult,
                           const GpuDiffusePathLoopSettings& settings) {
      std::vector<std::uint64_t> counts(settings.maxDepth, 0u);
      for (const GpuDiffusePathStepRecord& step : vulkanResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
        if (event == GpuDiffusePathStepEvent::Inactive || step.depth >= counts.size()) {
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
    makeLoopResult(const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                   const GpuDiffusePathLoopSettings& settings,
                   const VulkanGpuDiffusePathLoopKernelResult& vulkanResult) {
      GpuDiffusePathLoopResult loop;
      loop.executionPath = kFullGpuSubsetExecutionPath;
      loop.pathStateResidency = kVulkanPathStateResidency;
      loop.frontierCompactionExecutionPath = vulkanResult.executionPath;
      loop.frontierCompactionPathStateResidency = vulkanResult.pathStateResidency;
      loop.platformName = "vulkan";
      loop.initialPathCount = static_cast<std::uint64_t>(initialPathStates.size());
      loop.stepRecords = vulkanResult.stepRecords;
      loop.retainedIndexBytes =
        static_cast<std::uint64_t>(vulkanResult.retainedPathIndices.size() * sizeof(std::uint32_t));
      loop.roundTrips = 1;
      recordDepthCounts(loop, vulkanResult, settings);
      mergeStepMetrics(loop, vulkanResult);

      if (vulkanResult.resolvedPathStates.size() != initialPathStates.size()) {
        throw std::logic_error(
          "Vulkan diffuse path-loop backend returned mismatched path-state count");
      }
      for (GpuDiffusePathStateRecord path : vulkanResult.resolvedPathStates) {
        if (gpuDiffusePathStateIsTerminated(path)) {
          ++loop.metrics.terminatedPaths;
          loop.resolvedPathStates.push_back(path);
        }
      }
      return loop;
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
    return "Vulkan diffuse path-loop launch path is not available";
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
    if (settings.maxDepth == 0u) {
      return {false, "Vulkan diffuse path-loop backend requires positive max depth"};
    }
    if (!sceneHasNoGeometry(scene)) {
      return {false,
              "Vulkan diffuse path-loop backend currently supports empty compiled geometry only"};
    }
    return {true, {}};
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
    validateUniqueActivePixels(initialPathStates);

    const TracingAccumulationLayout layout = accumulationLayoutFor(initialPathStates);
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(scene, initialPathStates, layout, settings);
    const VulkanGpuDiffusePathLoopKernelResult vulkanResult =
      VulkanGpuDiffusePathLoopKernel().runAllMissPathLoop(plan, initialPathStates);
    return makeLoopResult(initialPathStates, settings, vulkanResult);
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
