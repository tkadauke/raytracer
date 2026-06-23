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
    primitiveUsesSupportedGeometry(const GpuIntersectionPrimitiveRecord& primitive) {
      const auto kind = static_cast<GpuIntersectionPrimitiveKind>(primitive.kind);
      return primitive.transform == 0u && primitive.payloadCount == 1u &&
             (kind == GpuIntersectionPrimitiveKind::Triangle ||
              kind == GpuIntersectionPrimitiveKind::Sphere ||
              kind == GpuIntersectionPrimitiveKind::Plane ||
              kind == GpuIntersectionPrimitiveKind::Rectangle ||
              kind == GpuIntersectionPrimitiveKind::Disk);
    }

    [[nodiscard]] bool sceneHasSupportedGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return !geometry.primitives.empty() && !geometry.bvh.empty() &&
             geometry.openCylinders.empty() && geometry.tori.empty() &&
             geometry.transforms.empty() &&
             geometry.triangles.size() + geometry.spheres.size() + geometry.planes.size() +
                 geometry.rectangles.size() + geometry.disks.size() ==
               geometry.primitives.size() &&
             std::all_of(geometry.primitives.begin(), geometry.primitives.end(),
                         primitiveUsesSupportedGeometry);
    }

    [[nodiscard]] bool sceneHasNoGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return geometry.primitives.empty() && geometry.bvh.empty();
    }

    [[nodiscard]] bool supportedMaterials(const GpuTracingSceneSections& scene) {
      return std::all_of(scene.materials.begin(), scene.materials.end(),
                         [](const GpuTracingMaterialRecord& material) {
                           const auto kind = static_cast<GpuTracingMaterialKind>(material.kind);
                           return kind == GpuTracingMaterialKind::Matte ||
                                  kind == GpuTracingMaterialKind::Emissive;
                         });
    }

    [[nodiscard]] bool supportedTextures(const GpuTracingSceneSections& scene) {
      return std::all_of(scene.textures.begin(), scene.textures.end(),
                         [](const GpuTracingTextureRecord& texture) {
                           return static_cast<GpuTracingTextureKind>(texture.kind) ==
                                  GpuTracingTextureKind::ConstantColor;
                         });
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
          "Metal diffuse path-loop backend accumulation pixel index exceeds layout range");
      }

      std::vector<bool> seen(static_cast<std::size_t>(maxPixel + 1u), false);
      for (const GpuDiffusePathStateRecord& path : pathStates) {
        if (!gpuDiffusePathStateIsActive(path)) {
          continue;
        }
        if (seen[path.pixelIndex]) {
          throw std::invalid_argument(
            "Metal diffuse path-loop backend requires unique active pixel targets");
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
          "Metal diffuse path-loop backend accumulation pixel index exceeds layout range");
      }
      return TracingAccumulationLayout::image(static_cast<int>(maxPixel + 1u), 1);
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
                     "untransformed triangle/sphere/plane/rectangle/disk geometry only"};
    }
    if (!supportedMaterials(scene)) {
      return {
        false,
        "Metal diffuse path-loop backend currently supports Matte and Emissive materials only"};
    }
    if (!supportedTextures(scene)) {
      return {false,
              "Metal diffuse path-loop backend currently supports ConstantColor textures only"};
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
    validateUniqueActivePixels(initialPathStates);

    const TracingAccumulationLayout layout = accumulationLayoutFor(initialPathStates);
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(scene, initialPathStates, layout, settings);
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
