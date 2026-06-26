#include "render/VulkanGpuDiffusePathLoopBackend.h"

#include "render/GpuFloat4.h"
#include "render/TracingAccumulationLayout.h"
#include "render/VulkanGpuDiffusePathLoopKernel.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kVulkanPathStateResidency = "vulkan_host_visible_diffuse_path_state";

    [[nodiscard]] bool sceneHasNoGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return geometry.primitives.empty() && geometry.bvh.empty();
    }

    struct SupportedGeometryCounts {
      std::size_t triangles{0};
      std::size_t spheres{0};
      std::size_t planes{0};
      std::size_t rectangles{0};
      std::size_t disks{0};
      std::size_t openCylinders{0};
      std::size_t tori{0};
    };

    [[nodiscard]] bool
    primitiveUsesSupportedGeometry(const GpuIntersectionPrimitiveRecord& primitive,
                                   const GpuIntersectionSceneBuffers& geometry,
                                   SupportedGeometryCounts& counts) {
      if (primitive.transform != 0u && primitive.transform >= geometry.transforms.size()) {
        return false;
      }
      switch (static_cast<GpuIntersectionPrimitiveKind>(primitive.kind)) {
      case GpuIntersectionPrimitiveKind::Triangle:
        if (primitive.payloadOffset >= geometry.triangles.size()) {
          return false;
        }
        ++counts.triangles;
        return true;
      case GpuIntersectionPrimitiveKind::Sphere:
        if (primitive.payloadOffset >= geometry.spheres.size()) {
          return false;
        }
        ++counts.spheres;
        return true;
      case GpuIntersectionPrimitiveKind::Plane:
        if (primitive.payloadOffset >= geometry.planes.size()) {
          return false;
        }
        ++counts.planes;
        return true;
      case GpuIntersectionPrimitiveKind::Rectangle:
        if (primitive.payloadOffset >= geometry.rectangles.size()) {
          return false;
        }
        ++counts.rectangles;
        return true;
      case GpuIntersectionPrimitiveKind::Disk:
        if (primitive.payloadOffset >= geometry.disks.size()) {
          return false;
        }
        ++counts.disks;
        return true;
      case GpuIntersectionPrimitiveKind::OpenCylinder:
        if (primitive.payloadOffset >= geometry.openCylinders.size()) {
          return false;
        }
        ++counts.openCylinders;
        return true;
      case GpuIntersectionPrimitiveKind::Torus:
        if (primitive.payloadOffset >= geometry.tori.size()) {
          return false;
        }
        ++counts.tori;
        return true;
      case GpuIntersectionPrimitiveKind::Unsupported:
        return false;
      }
      return false;
    }

    [[nodiscard]] bool sceneHasSupportedStaticGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      if (geometry.primitives.empty()) {
        return false;
      }
      SupportedGeometryCounts counts;
      for (const GpuIntersectionPrimitiveRecord& primitive : geometry.primitives) {
        if (!primitiveUsesSupportedGeometry(primitive, geometry, counts)) {
          return false;
        }
      }
      return counts.triangles == geometry.triangles.size() &&
             counts.spheres == geometry.spheres.size() && counts.planes == geometry.planes.size() &&
             counts.rectangles == geometry.rectangles.size() &&
             counts.disks == geometry.disks.size() &&
             counts.openCylinders == geometry.openCylinders.size() &&
             counts.tori == geometry.tori.size();
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
        if (kind != GpuTracingMaterialKind::Matte && kind != GpuTracingMaterialKind::Phong &&
            kind != GpuTracingMaterialKind::Reflective &&
            kind != GpuTracingMaterialKind::Transparent &&
            kind != GpuTracingMaterialKind::Emissive) {
          return false;
        }
      }
      return true;
    }

    [[nodiscard]] bool supportedTexture(const GpuTracingSceneSections& scene,
                                        std::size_t textureIndex, std::uint32_t depth = 0u);

    [[nodiscard]] bool supportedUntintedTexture(const GpuTracingSceneSections& scene,
                                                std::size_t textureIndex, std::uint32_t depth) {
      const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
      const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
      if (kind == GpuTracingTextureKind::Unsupported) {
        return textureIndex == 0u && depth == 0u;
      }
      if (kind == GpuTracingTextureKind::ConstantColor) {
        return true;
      }
      if (kind == GpuTracingTextureKind::UVColor) {
        return true;
      }
      if (kind == GpuTracingTextureKind::CheckerBoard) {
        const auto mapping =
          static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
        if (mapping != GpuTracingTextureMappingKind::Planar &&
            mapping != GpuTracingTextureMappingKind::UV) {
          return false;
        }
        if (texture.payloadOffset >= scene.textures.size() ||
            texture.payloadCount >= scene.textures.size()) {
          return false;
        }
        return supportedTexture(scene, texture.payloadOffset, depth + 1u) &&
               supportedTexture(scene, texture.payloadCount, depth + 1u);
      }
      if (kind == GpuTracingTextureKind::Image) {
        const auto mapping =
          static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
        if (mapping != GpuTracingTextureMappingKind::Planar &&
            mapping != GpuTracingTextureMappingKind::UV) {
          return false;
        }
        const std::uint32_t width = static_cast<std::uint32_t>(std::round(texture.parameters[2]));
        const std::uint32_t height = static_cast<std::uint32_t>(std::round(texture.parameters[3]));
        const std::uint64_t texelCount =
          static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        if (width == 0u || height == 0u || texture.payloadCount != texelCount ||
            texture.payloadOffset >= scene.textures.size() ||
            static_cast<std::uint64_t>(texture.payloadOffset) + texture.payloadCount >
              scene.textures.size()) {
          return false;
        }
        for (std::uint32_t offset = 0; offset != texture.payloadCount; ++offset) {
          if (static_cast<GpuTracingTextureKind>(
                scene.textures[texture.payloadOffset + offset].kind) !=
              GpuTracingTextureKind::ConstantColor) {
            return false;
          }
        }
        return true;
      }
      return false;
    }

    [[nodiscard]] bool supportedTexture(const GpuTracingSceneSections& scene,
                                        std::size_t textureIndex, std::uint32_t depth) {
      constexpr std::uint32_t maxTextureEvaluationDepth = 8u;
      if (depth >= maxTextureEvaluationDepth) {
        return false;
      }
      if (textureIndex >= scene.textures.size()) {
        return false;
      }
      const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
      const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
      if (kind == GpuTracingTextureKind::Tinted) {
        return texture.payloadOffset < scene.textures.size() &&
               supportedTexture(scene, texture.payloadOffset, depth + 1u);
      }
      return supportedUntintedTexture(scene, textureIndex, depth);
    }

    [[nodiscard]] bool supportedTextures(const GpuTracingSceneSections& scene) {
      for (std::size_t index = 0; index != scene.textures.size(); ++index) {
        if (!supportedTexture(scene, index)) {
          return false;
        }
      }
      return true;
    }

    [[nodiscard]] bool supportedLights(const GpuTracingSceneSections& scene) {
      return std::all_of(
        scene.lights.begin(), scene.lights.end(), [](const GpuTracingLightRecord& light) {
          const auto kind = static_cast<GpuTracingLightKind>(light.kind);
          return kind == GpuTracingLightKind::Point || kind == GpuTracingLightKind::Directional ||
                 kind == GpuTracingLightKind::RectangularArea;
        });
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
          "Vulkan diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      if (maxSampleSlot >= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error(
          "Vulkan diffuse path-loop backend accumulation sample index exceeds layout range");
      }
      return {activePathCount == 0u ? 1u : maxPixel + 1u,
              activePathCount == 0u ? 1u : maxSampleSlot + 1u, activePathCount};
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
      loop.frontierCompactionUploadWorkerSeconds = vulkanResult.uploadWorkerSeconds;
      loop.frontierCompactionKernelWorkerSeconds = vulkanResult.kernelWorkerSeconds;
      loop.frontierCompactionReadbackWorkerSeconds = vulkanResult.readbackWorkerSeconds;
      loop.platformAccumulationColorSums = vulkanResult.accumulationColorSums;
      loop.platformAccumulationSampleCounts = vulkanResult.accumulationSampleCounts;
      loop.platformAccumulationBackend = "vulkan_diffuse_path_loop";
      loop.platformAccumulationResidency = "vulkan_accumulation_buffer";
      loop.platformAccumulationTargetMode = vulkanResult.echoedParameters.accumulationTargetMode;
      loop.platformAccumulationWidth = vulkanResult.echoedParameters.imageWidth;
      loop.platformAccumulationHeight = vulkanResult.echoedParameters.imageHeight;
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
    if (!sceneHasSupportedStaticGeometry(scene)) {
      return {false, "Vulkan diffuse path-loop backend currently supports empty geometry or "
                     "triangle, sphere, plane, rectangle, disk, open-cylinder, or torus "
                     "primitives with static transforms only"};
    }
    if (!supportedMaterials(scene)) {
      return {false,
              "Vulkan diffuse path-loop backend currently supports Matte, Phong finite glossy, "
              "Reflective mirror, Transparent refraction, and Emissive materials only"};
    }
    if (!supportedTextures(scene)) {
      return {false, "Vulkan diffuse path-loop backend currently supports ConstantColor, "
                     "CheckerBoard texture graphs, nearest/bilinear ImageTexture, "
                     "UVColorTexture, and bounded Tinted wrapper chains over those textures "
                     "only"};
    }
    if (!supportedLights(scene)) {
      return {false, "Vulkan diffuse path-loop backend currently supports point, directional, or "
                     "rectangular area lights only"};
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
    const VulkanAccumulationPlan accumulation = accumulationPlanFor(initialPathStates);
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      scene, initialPathStates, accumulation.layout, settings);
    plan.parameters.accumulationTargetMode = accumulation.targetMode;
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
