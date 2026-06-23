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

    [[nodiscard]] bool sceneHasSingleUntransformedSphere(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      if (geometry.primitives.size() != 1u || geometry.spheres.size() != 1u ||
          !geometry.triangles.empty() || !geometry.planes.empty() || !geometry.rectangles.empty() ||
          !geometry.disks.empty() || !geometry.openCylinders.empty() || !geometry.tori.empty() ||
          !geometry.transforms.empty()) {
        return false;
      }
      const GpuIntersectionPrimitiveRecord& primitive = geometry.primitives.front();
      return static_cast<GpuIntersectionPrimitiveKind>(primitive.kind) ==
               GpuIntersectionPrimitiveKind::Sphere &&
             primitive.transform == 0u && primitive.payloadOffset == 0u;
    }

    [[nodiscard]] bool supportedMaterials(const GpuTracingSceneSections& scene) {
      for (std::size_t index = 0; index != scene.materials.size(); ++index) {
        const auto kind = static_cast<GpuTracingMaterialKind>(scene.materials[index].kind);
        if (kind == GpuTracingMaterialKind::Unsupported) {
          if (index == 0u) {
            continue;
          }
          return false;
        }
        if (kind != GpuTracingMaterialKind::Matte && kind != GpuTracingMaterialKind::Emissive) {
          return false;
        }
      }
      return true;
    }

    [[nodiscard]] bool supportedTextures(const GpuTracingSceneSections& scene) {
      return std::all_of(scene.textures.begin(), scene.textures.end(),
                         [](const GpuTracingTextureRecord& texture) {
                           const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
                           return kind == GpuTracingTextureKind::Unsupported ||
                                  kind == GpuTracingTextureKind::ConstantColor;
                         });
    }

    [[nodiscard]] bool supportedLights(const GpuTracingSceneSections& scene) {
      if (scene.lights.empty()) {
        return true;
      }
      return scene.lights.size() == 1u &&
             static_cast<GpuTracingLightKind>(scene.lights.front().kind) ==
               GpuTracingLightKind::Point;
    }

    void terminate(GpuDiffusePathStateRecord& path) {
      path.flags &= ~gpuDiffusePathStateActiveFlag;
      path.flags |= gpuDiffusePathStateTerminatedFlag;
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

    [[nodiscard]] bool stepHasContinuation(const GpuDiffusePathStepRecord& step) {
      return arrayHasValue(step.continuationThroughput);
    }

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const VulkanGpuDiffusePathLoopKernelResult& vulkanResult,
                          const GpuDiffusePathLoopSettings& settings) {
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
      mergeStepMetrics(loop, vulkanResult, settings);

      std::vector<GpuDiffusePathStateRecord> nextPathStates = vulkanResult.nextPathStates.empty()
                                                                ? vulkanResult.resolvedPathStates
                                                                : vulkanResult.nextPathStates;
      if (nextPathStates.size() != initialPathStates.size()) {
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
    if (sceneHasNoGeometry(scene)) {
      return {true, {}};
    }
    if (settings.maxDepth != 1u) {
      return {false, "Vulkan diffuse path-loop backend currently supports non-empty scenes only at "
                     "maxDepth=1"};
    }
    if (!sceneHasSingleUntransformedSphere(scene)) {
      return {false, "Vulkan diffuse path-loop backend currently supports empty geometry or one "
                     "untransformed sphere only"};
    }
    if (!supportedMaterials(scene)) {
      return {false,
              "Vulkan diffuse path-loop backend currently supports Matte and Emissive materials "
              "only"};
    }
    if (!supportedTextures(scene)) {
      return {false,
              "Vulkan diffuse path-loop backend currently supports ConstantColor textures only"};
    }
    if (!supportedLights(scene)) {
      return {false,
              "Vulkan diffuse path-loop backend currently supports zero or one point light only"};
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
