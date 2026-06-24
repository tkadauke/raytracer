#include "render/MetalGpuDiffusePathLoopBackend.h"

#include "render/MetalGpuDiffusePathLoopKernel.h"
#include "render/TracingAccumulationLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace render {
  namespace {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    constexpr const char* kFullGpuSubsetExecutionPath = "full_gpu_subset";
    constexpr const char* kMetalPathStateResidency = "metal_shared_diffuse_path_state";

    [[nodiscard]] bool arrayHasValue(const std::array<float, 4>& value) {
      return std::any_of(value.begin(), value.end(),
                         [](float component) { return std::fabs(component) > 1.0e-8f; });
    }

    [[nodiscard]] bool
    primitiveUsesSupportedGeometry(const GpuIntersectionPrimitiveRecord& primitive,
                                   std::size_t transformCount) {
      const auto kind = static_cast<GpuIntersectionPrimitiveKind>(primitive.kind);
      return primitive.payloadCount == 1u &&
             (primitive.transform == 0u || primitive.transform < transformCount) &&
             (kind == GpuIntersectionPrimitiveKind::Triangle ||
              kind == GpuIntersectionPrimitiveKind::Sphere ||
              kind == GpuIntersectionPrimitiveKind::Plane ||
              kind == GpuIntersectionPrimitiveKind::Rectangle ||
              kind == GpuIntersectionPrimitiveKind::Disk ||
              kind == GpuIntersectionPrimitiveKind::OpenCylinder ||
              kind == GpuIntersectionPrimitiveKind::Torus);
    }

    [[nodiscard]] bool sceneHasSupportedGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return !geometry.primitives.empty() && !geometry.bvh.empty() &&
             geometry.triangles.size() + geometry.spheres.size() + geometry.planes.size() +
                 geometry.rectangles.size() + geometry.disks.size() +
                 geometry.openCylinders.size() + geometry.tori.size() ==
               geometry.primitives.size() &&
             std::all_of(geometry.primitives.begin(), geometry.primitives.end(),
                         [&geometry](const GpuIntersectionPrimitiveRecord& primitive) {
                           return primitiveUsesSupportedGeometry(primitive,
                                                                 geometry.transforms.size());
                         });
    }

    [[nodiscard]] bool sceneHasNoGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return geometry.primitives.empty() && geometry.bvh.empty();
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
            kind != GpuTracingMaterialKind::Emissive) {
          return false;
        }
      }
      return true;
    }

    [[nodiscard]] bool supportedTexture(const GpuTracingSceneSections& scene,
                                        std::size_t textureIndex) {
      const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
      const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
      if (kind == GpuTracingTextureKind::Unsupported) {
        return textureIndex == 0u;
      }
      if (kind == GpuTracingTextureKind::ConstantColor) {
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
        return static_cast<GpuTracingTextureKind>(scene.textures[texture.payloadOffset].kind) ==
                 GpuTracingTextureKind::ConstantColor &&
               static_cast<GpuTracingTextureKind>(scene.textures[texture.payloadCount].kind) ==
                 GpuTracingTextureKind::ConstantColor;
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

    void terminate(GpuDiffusePathStateRecord& path) {
      path.flags &= ~gpuDiffusePathStateActiveFlag;
      path.flags |= gpuDiffusePathStateTerminatedFlag;
    }

    [[nodiscard]] bool stepHasContinuation(const GpuDiffusePathStepRecord& step) {
      return arrayHasValue(step.continuationThroughput);
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

    void mergeStepMetrics(GpuDiffusePathLoopResult& loop,
                          const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
                          const MetalGpuDiffusePathLoopKernelResult& metalResult,
                          const GpuDiffusePathLoopSettings& settings) {
      (void)initialPathStates;
      std::uint64_t activeSteps = 0;
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
                           const MetalGpuDiffusePathLoopKernelResult& metalResult,
                           const GpuDiffusePathLoopSettings& settings) {
      std::vector<std::uint64_t> counts(settings.maxDepth, 0u);
      for (const GpuDiffusePathStepRecord& step : metalResult.stepRecords) {
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
                   const MetalGpuDiffusePathLoopKernelResult& metalResult) {
      GpuDiffusePathLoopResult loop;
      loop.executionPath = kFullGpuSubsetExecutionPath;
      loop.pathStateResidency = kMetalPathStateResidency;
      loop.frontierCompactionExecutionPath = metalResult.executionPath;
      loop.frontierCompactionPathStateResidency = metalResult.pathStateResidency;
      loop.platformName = "metal";
      loop.initialPathCount = static_cast<std::uint64_t>(initialPathStates.size());
      loop.stepRecords = metalResult.stepRecords;
      loop.retainedIndexBytes =
        static_cast<std::uint64_t>(metalResult.retainedPathIndices.size() * sizeof(std::uint32_t));
      loop.roundTrips = 1;
      loop.frontierCompactionUploadWorkerSeconds = metalResult.uploadWorkerSeconds;
      loop.frontierCompactionKernelWorkerSeconds = metalResult.kernelWorkerSeconds;
      loop.frontierCompactionReadbackWorkerSeconds = metalResult.readbackWorkerSeconds;
      loop.platformAccumulationColorSums = metalResult.accumulationColorSums;
      loop.platformAccumulationSampleCounts = metalResult.accumulationSampleCounts;
      loop.platformAccumulationBackend = "metal_diffuse_path_loop";
      loop.platformAccumulationResidency = "metal_accumulation_buffer";
      loop.platformAccumulationTargetMode = metalResult.echoedParameters.accumulationTargetMode;
      loop.platformAccumulationWidth = metalResult.echoedParameters.imageWidth;
      loop.platformAccumulationHeight = metalResult.echoedParameters.imageHeight;
      recordDepthCounts(loop, metalResult, settings);
      mergeStepMetrics(loop, initialPathStates, metalResult, settings);

      std::vector<GpuDiffusePathStateRecord> nextPathStates = metalResult.nextPathStates.empty()
                                                                ? metalResult.resolvedPathStates
                                                                : metalResult.nextPathStates;
      if (nextPathStates.size() != initialPathStates.size()) {
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
    return "Metal diffuse path-loop launch path is not available";
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
    if (settings.maxDepth == 0u) {
      return {false, "Metal diffuse path-loop backend requires positive max depth"};
    }
    if (!sceneHasNoGeometry(scene) && !sceneHasSupportedGeometry(scene)) {
      return {false, "Metal diffuse path-loop backend currently supports empty geometry or "
                     "triangle/sphere/plane/rectangle/disk/open-cylinder/torus geometry only"};
    }
    if (!supportedMaterials(scene)) {
      return {false, "Metal diffuse path-loop backend currently supports Matte, Phong finite "
                     "glossy, Reflective mirror, and Emissive materials only"};
    }
    if (!supportedTextures(scene)) {
      return {false, "Metal diffuse path-loop backend currently supports ConstantColor, simple "
                     "CheckerBoard, and nearest ImageTexture textures only"};
    }
    if (!supportedLights(scene)) {
      return {false, "Metal diffuse path-loop backend currently supports point, directional, and "
                     "rectangular area lights only"};
    }
    return {true, {}};
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
      MetalGpuDiffusePathLoopKernel().runMattePathLoop(plan, initialPathStates);
    return makeLoopResult(initialPathStates, settings, metalResult);
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
