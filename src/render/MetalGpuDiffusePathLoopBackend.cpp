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

    [[nodiscard]] bool sceneHasOnlySphereGeometry(const GpuTracingSceneSections& scene) {
      const GpuIntersectionSceneBuffers& geometry = scene.geometry;
      return !geometry.primitives.empty() && !geometry.bvh.empty() && geometry.triangles.empty() &&
             geometry.planes.empty() && geometry.rectangles.empty() && geometry.disks.empty() &&
             geometry.openCylinders.empty() && geometry.tori.empty() &&
             geometry.transforms.empty() && geometry.spheres.size() == geometry.primitives.size();
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

    [[nodiscard]] std::uint64_t
    activePathCount(const std::vector<GpuDiffusePathStateRecord>& pathStates) {
      return static_cast<std::uint64_t>(
        std::count_if(pathStates.begin(), pathStates.end(), gpuDiffusePathStateIsActive));
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
      const std::uint64_t active = activePathCount(initialPathStates);
      loop.metrics.activePaths = active;
      loop.metrics.closestHitRays = active;
      loop.metrics.closestHitExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.emissionExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightVisibilityExecutionPath = kFullGpuSubsetExecutionPath;
      loop.metrics.directLightContributionExecutionPath = kFullGpuSubsetExecutionPath;

      for (const GpuDiffusePathStepRecord& step : metalResult.stepRecords) {
        const auto event = static_cast<GpuDiffusePathStepEvent>(step.event);
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
        } else if (event == GpuDiffusePathStepEvent::Unsupported) {
          ++loop.metrics.unsupportedHits;
        }
      }
      loop.metrics.spawnedContinuations =
        static_cast<std::uint64_t>(metalResult.retainedPathIndices.size());
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
      const std::uint64_t active = activePathCount(initialPathStates);
      if (active != 0u) {
        loop.depthCount = 1;
        loop.activePathsPerDepth.push_back(active);
      }
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
    if (settings.maxDepth != 1u) {
      return {false, "Metal diffuse path-loop backend currently supports exactly one path depth"};
    }
    if (!sceneHasNoGeometry(scene) && !sceneHasOnlySphereGeometry(scene)) {
      return {false,
              "Metal diffuse path-loop backend currently supports empty or untransformed sphere "
              "geometry only"};
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
      sceneHasNoGeometry(scene)
        ? MetalGpuDiffusePathLoopKernel().runAllMissProbe(plan, initialPathStates)
        : MetalGpuDiffusePathLoopKernel().runMatteContinuationProbe(plan, initialPathStates);
    return makeLoopResult(initialPathStates, settings, metalResult);
#else
    (void)scene;
    (void)initialPathStates;
    (void)settings;
    throw std::runtime_error(fullGpuPathLoopUnavailableReason());
#endif
  }
}
